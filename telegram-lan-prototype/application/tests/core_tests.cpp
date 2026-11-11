#include "node.h"
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>
#include <QtEndian>
class CoreTests : public QObject {
    Q_OBJECT
  private slots:
    void longMessages_data() {
        QTest::addColumn<QString>("text");
        QTest::newRow("ascii-32k") << QString(32768, 'x');
        QTest::newRow("utf8-cjk-120k") << QString(40000, QChar(0x4e2d));
        QTest::newRow("emoji-boundary") << QString::fromUtf8("😀").repeated(Node::MaxTextBytes/4);
        QTest::newRow("ascii-boundary") << QString(Node::MaxTextBytes, 'z');
        QTest::newRow("escaped-boundary") << (QString("a") + QString(Node::MaxTextBytes-1, QChar(1)));
        QTest::newRow("multiline") << QString::fromUtf8("中文😀\n\"\\").repeated(8000);
    }
    void longMessages() {
        QFETCH(QString, text);
        QTemporaryDir a,b;
        {
            Node alice("Alice",a.path()), bob("Bob",b.path());
            QVERIFY(alice.start(0,0)); QVERIFY(bob.start(0,0));
            alice.addPeer("127.0.0.1",bob.port());
            QTRY_COMPARE(alice.peers().size(),1);
            QVERIFY(!alice.sendMessage(bob.id(),text).isEmpty());
            QTRY_COMPARE_WITH_TIMEOUT(bob.history(alice.id()).size(),1,10000);
            QTRY_COMPARE(alice.pendingMessages().size(),0);
            QCOMPARE(bob.history(alice.id()).first().toObject()["text"].toString(),text);
            QVERIFY(!bob.sendMessage(alice.id(),text).isEmpty());
            QTRY_COMPARE(alice.history(bob.id()).size(),2);
        }
        Node alice("Alice",a.path()), bob("Bob",b.path());
        QCOMPARE(bob.history(alice.id()).first().toObject()["text"].toString(),text);
    }
    void longGroupOfflineAndLimits() {
        QTemporaryDir a,b,c;
        QString gid;
        const QString text=QString::fromUtf8("长消息😀\n").repeated(4000);
        {
            Node alice("Alice",a.path()),bob("Bob",b.path()),carol("Carol",c.path());
            QVERIFY(alice.start(0,0)); QVERIFY(bob.start(0,0)); QVERIFY(carol.start(0,0));
            alice.addPeer("127.0.0.1",bob.port()); alice.addPeer("127.0.0.1",carol.port());
            QTRY_COMPARE(alice.peers().size(),2);
            gid=alice.createGroup({bob.id(),carol.id()},"Long");
            bob.stop(); carol.stop();
            QVERIFY(!alice.sendMessage("group:"+gid,text).isEmpty());
            QCOMPARE(alice.pendingMessages().size(),2);
            QVERIFY(alice.sendMessage(bob.id(),QString(Node::MaxTextBytes+1,'x')).isEmpty());
            QCOMPARE(alice.pendingMessages().size(),2);
        }
        Node alice("Alice",a.path()),bob("Bob",b.path()),carol("Carol",c.path());
        QVERIFY(alice.start(0,0)); QVERIFY(bob.start(0,0)); QVERIFY(carol.start(0,0));
        alice.addPeer("127.0.0.1",bob.port()); alice.addPeer("127.0.0.1",carol.port());
        QTRY_COMPARE_WITH_TIMEOUT(alice.pendingMessages().size(),0,10000);
        QCOMPARE(bob.history("group:"+gid).first().toObject()["text"].toString(),text);
        QCOMPARE(carol.history("group:"+gid).first().toObject()["text"].toString(),text);
        auto legacy=bob.hello(); legacy.remove("max_text_bytes");
        QVERIFY(alice.remember(legacy,"127.0.0.1"));
        QSignalSpy errors(&alice,&Node::event);
        QVERIFY(alice.sendMessage(bob.id(),QString(10001,'a')).isEmpty());
        QCOMPARE(alice.pendingMessages().size(),0);
        QVERIFY(!errors.isEmpty());
        QVERIFY(!alice.sendMessage(bob.id(),QString(10000,'a')).isEmpty());
        QTRY_COMPARE(alice.pendingMessages().size(),0);
        bool done=false;
        alice.request(bob.id(),{{"type","message"},{"id",QString(32,'e')},
                                {"text",QString(Node::MaxTextBytes+1,'a')}},[&](auto r, auto){
            QVERIFY(r.contains("error")); done=true;
        });
        QTRY_VERIFY(done);
    }
    void explicitRemoteRejectionDoesNotRetry() {
        QTemporaryDir a,b;
        Node alice("Alice",a.path()),bob("Bob",b.path());
        QVERIFY(alice.start(0,0));
        QTcpServer legacy; QVERIFY(legacy.listen(QHostAddress::LocalHost,0));
        int received=0;
        connect(&legacy,&QTcpServer::newConnection,this,[&]{
            auto *socket=legacy.nextPendingConnection();
            connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{
                if(socket->property("replied").toBool()) return;
                socket->setProperty("replied",true); ++received;
                Node::writeFrame(socket,{{"error","Invalid message"}});
                socket->disconnectFromHost();
            });
            connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        });
        auto peer=bob.hello(); peer["port"]=int(legacy.serverPort());
        QVERIFY(alice.remember(peer,"127.0.0.1"));
        QVERIFY(!alice.sendMessage(bob.id(),QString(32768,'x')).isEmpty());
        QTRY_VERIFY(alice.pendingMessages().first().toObject()["error"].toString().startsWith("rejected:"));
        QTest::qWait(5500); // retry timer tick must not replay a permanent refusal
        QCOMPARE(received,1);
        QCOMPARE(alice.pendingMessages().size(),1);
        QCOMPARE(alice.history(bob.id()).first().toObject()["text"].toString(),QString(32768,'x'));
    }
    void transientRemoteRejectionRetries() {
        QTemporaryDir a,b;
        Node alice("Alice",a.path()),bob("Bob",b.path());
        QVERIFY(alice.start(0,0));
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost,0));
        int received=0;
        connect(&server,&QTcpServer::newConnection,this,[&]{
            auto *socket=server.nextPendingConnection();
            connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{
                if(socket->property("replied").toBool() || socket->bytesAvailable()<4) return;
                auto size=qFromBigEndian<quint32>(socket->peek(4).constData());
                if(socket->bytesAvailable()<size+4) return;
                socket->read(4); auto packet=QJsonDocument::fromJson(socket->read(size)).object();
                socket->setProperty("replied",true); ++received;
                Node::writeFrame(socket,received==1 ? QJsonObject{{"error","Database busy"}} :
                                 QJsonObject{{"ack",packet["id"]}});
                socket->disconnectFromHost();
            });
            connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        });
        auto peer=bob.hello(); peer["port"]=int(server.serverPort());
        QVERIFY(alice.remember(peer,"127.0.0.1"));
        QVERIFY(!alice.sendMessage(bob.id(),QString(32768,'x')).isEmpty());
        QTRY_COMPARE(alice.pendingMessages().first().toObject()["error"].toString(),QString("Database busy"));
        QTRY_COMPARE_WITH_TIMEOUT(alice.pendingMessages().size(),0,10000);
        QCOMPARE(received,2);
    }
    void discoveryAndMessage() {
        QTemporaryDir a, b;
        Node alice("Alice", a.path()), bob("Bob", b.path());
        QVERIFY(alice.start(0, 0));
        QVERIFY(bob.start(0, 0));
        alice.addPeer("127.0.0.1", bob.port());
        QTRY_COMPARE(alice.peers().size(), 1);
        auto id = alice.sendMessage(bob.id(), "hello <b>literal</b>");
        QVERIFY(!id.isEmpty());
        QTRY_COMPARE(bob.history(alice.id()).size(), 1);
        QTRY_COMPARE(alice.pendingMessages().size(), 0);
        QCOMPARE(bob.history(alice.id()).first().toObject()["text"].toString(),
                 QString("hello <b>literal</b>"));
    }
    void groupRoundTrip() {
        QTemporaryDir a, b, c;
        Node alice("Alice", a.path()), bob("Bob", b.path()), carol("Carol", c.path());
        alice.start(0, 0);
        bob.start(0, 0);
        carol.start(0, 0);
        alice.addPeer("127.0.0.1", bob.port());
        alice.addPeer("127.0.0.1", carol.port());
        QTRY_COMPARE(alice.peers().size(), 2);
        auto gid = alice.createGroup({bob.id(), carol.id()}, "Group");
        QVERIFY(!gid.isEmpty());
        alice.sendMessage("group:" + gid, "all");
        QTRY_COMPARE(bob.groups().size(), 1);
        QTRY_COMPARE(carol.groups().size(), 1);
        bob.sendMessage("group:" + gid, "reply");
        QTRY_COMPARE(carol.history("group:" + gid).size(), 2);
        QTRY_COMPARE(alice.history("group:" + gid).size(), 2);
    }
    void offlineRestart() {
        QTemporaryDir a, b;
        QString bid;
        {
            Node alice("Alice", a.path()), bob("Bob", b.path());
            alice.start(0, 0);
            bob.start(0, 0);
            bid = bob.id();
            alice.addPeer("127.0.0.1", bob.port());
            QTRY_COMPARE(alice.peers().size(), 1);
            bob.stop();
            alice.sendMessage(bid, "queued");
            QCOMPARE(alice.pendingMessages().size(), 1);
        }
        Node alice("Alice", a.path()), bob("Bob", b.path());
        alice.start(0, 0);
        bob.start(0, 0);
        alice.addPeer("127.0.0.1", bob.port());
        QTRY_COMPARE_WITH_TIMEOUT(alice.pendingMessages().size(), 0, 8000);
        QCOMPARE(bob.history(alice.id()).size(), 1);
    }
    void profileAndImage() {
        QTemporaryDir a, b;
        Node alice("Alice", a.path()), bob("Bob", b.path());
        alice.start(0, 0);
        bob.start(0, 0);
        auto p = bob.publicProfile();
        p["nickname"] = "New Bob";
        p["signature"] = "<b>literal</b>";
        QVERIFY(bob.updateProfile(p));
        p["nickname"] = "bad\n";
        QVERIFY(!bob.updateProfile(p));
        alice.addPeer("127.0.0.1", bob.port());
        QTRY_COMPARE(alice.peers().size(), 1);
        QSignalSpy spy(&alice, &Node::event);
        alice.fetchProfile(bob.id());
        QTRY_VERIFY([&] {
            for (auto r : spy)
                if (r[0].toString() == "profile_received")
                    return true;
            return false;
        }());
        QImage im(64, 48, QImage::Format_RGB32);
        im.fill(Qt::red);
        QByteArray jpeg;
        QBuffer buf(&jpeg);
        buf.open(QIODevice::WriteOnly);
        im.save(&buf, "JPEG");
        QVERIFY(!alice.sendImage(bob.id(), jpeg).isEmpty());
        QTRY_COMPARE(bob.history(alice.id()).size(), 1);
        QVERIFY(QFile::exists(bob.history(alice.id()).first().toObject()["image_path"].toString()));
    }
    void duplicateAndMalformedGroup() {
        QTemporaryDir a, b;
        Node alice("Alice", a.path()), bob("Bob", b.path());
        alice.start(0, 0);
        bob.start(0, 0);
        alice.addPeer("127.0.0.1", bob.port());
        QTRY_COMPARE(alice.peers().size(), 1);
        QJsonObject payload{{"type", "message"}, {"id", QString(32, 'a')}, {"text", "once"}};
        int replies = 0;
        alice.request(bob.id(), payload, [&](auto r, auto e) {
            QVERIFY(e.isEmpty());
            QCOMPARE(r["ack"].toString(), QString(32, 'a'));
            ++replies;
        });
        QTRY_COMPARE(replies, 1);
        alice.request(bob.id(), payload, [&](auto r, auto e) {
            QVERIFY(e.isEmpty());
            QCOMPARE(r["ack"].toString(), QString(32, 'a'));
            ++replies;
        });
        QTRY_COMPARE(replies, 2);
        QCOMPARE(bob.history(alice.id()).size(), 1);
        payload["type"] = "group_message";
        payload["id"] = QString(32, 'b');
        payload["group"] = QJsonObject{
            {"id", QString(32, 'c')}, {"name", "bad"}, {"members", QJsonArray{alice.id(), alice.id()}}};
        alice.request(bob.id(), payload, [&](auto r, auto) {
            QVERIFY(r.contains("error"));
            ++replies;
        });
        QTRY_COMPARE(replies, 3);
        QCOMPARE(bob.groups().size(), 0);
    }
    void rejectionAndFraming() {
        QVERIFY(Node::privateAddress(QHostAddress("127.0.0.1")));
        QVERIFY(!Node::privateAddress(QHostAddress("8.8.8.8")));
        QVERIFY(Node::validId(QString(32, 'a')));
        QVERIFY(!Node::validId("../../bad"));
        QTemporaryDir a;
        Node node("A", a.path());
        node.start(0, 0);
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, node.port());
        QVERIFY(socket.waitForConnected());
        auto f = Node::frame({{"type", "hello"}});
        socket.write(f.left(2));
        socket.flush();
        QTest::qWait(10);
        QCOMPARE(socket.bytesAvailable(), 0);
        socket.write(f.mid(2));
        QTRY_VERIFY(socket.bytesAvailable() > 4);
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "core_tests.moc"
