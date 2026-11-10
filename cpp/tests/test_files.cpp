#include "filetransfer.h"
#include "node.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
class FileTests : public QObject {
    Q_OBJECT
  private slots:
    void names() {
        QVERIFY(FileTransfer::safeName("固件.bin"));
        for (auto s : {"../x", "CON.txt", "a:", "a.", "a/b", "a\\b", "NUL"})
            QVERIFY(!FileTransfer::safeName(s));
    }
    void transfer() {
        QTemporaryDir a, b, d;
        QVERIFY(a.isValid());
        Node n1("one", a.path()), n2("two", b.path());
        FileTransfer f1(&n1), f2(&n2);
        n1.serviceHandler = [&](auto s, auto r, auto h) { return f1.handle(s, r, h); };
        n2.serviceHandler = [&](auto s, auto r, auto h) { return f2.handle(s, r, h); };
        QVERIFY(n1.start(0, 0));
        QVERIFY(n2.start(0, 0));
        n1.addPeer("127.0.0.1", n2.port());
        QTRY_COMPARE(n1.peers().size(), 1);
        QTRY_COMPARE(n2.peers().size(), 1);
        QString path = a.path() + "/sample.bin";
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(200000, 'x'));
        file.close();
        QSignalSpy offered(&f2, &FileTransfer::offerReceived), complete(&f2, &FileTransfer::completed);
        f1.offerFile(n2.id(), path);
        QTRY_COMPARE(offered.size(), 1);
        auto offer = offered.first()[0].toJsonObject();
        f2.acceptFile(offer["id"].toString(), d.path());
        QTRY_COMPARE(complete.size(), 1);
        QCOMPARE(FileTransfer::sha256(path), FileTransfer::sha256(d.path() + "/sample.bin"));
        QTRY_COMPARE(f2.incomingOffers().size(), 0);
    }
    void shareRevocation() {
        QTemporaryDir a, b;
        Node n1("one", a.path()), n2("two", b.path());
        FileTransfer f1(&n1), f2(&n2);
        n1.serviceHandler = [&](auto s, auto r, auto h) { return f1.handle(s, r, h); };
        n2.serviceHandler = [&](auto s, auto r, auto h) { return f2.handle(s, r, h); };
        QVERIFY(n1.start(0, 0));
        QVERIFY(n2.start(0, 0));
        n2.addPeer("127.0.0.1", n1.port());
        QTRY_COMPARE(n2.peers().size(), 1);
        QFile file(a.path() + "/a.bin");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("abc");
        file.close();
        QString t = f1.publishShare(a.path());
        QVERIFY(!t.isEmpty());
        QJsonObject offer;
        bool got = false;
        n2.request(n1.id(), {{"type", "share_get"}, {"token", t}, {"path", "a.bin"}}, [&](auto r, auto) {
            offer = r["offer"].toObject();
            got = true;
        });
        QTRY_VERIFY(got);
        QVERIFY(!offer.isEmpty());
        f1.revokeShare(t);
        got = false;
        n2.request(n1.id(), {{"type", "fetch"}, {"id", offer["id"]}, {"offset", 0}}, [&](auto r, auto) {
            QVERIFY(!r["ok"].toBool());
            got = true;
        });
        QTRY_VERIFY(got);
    }
    void restartResume() {
        QTemporaryDir a, b, d;
        Node receiver("two", b.path());
        FileTransfer receive(&receiver);
        receiver.serviceHandler = [&](auto s, auto r, auto h) { return receive.handle(s, r, h); };
        QVERIFY(receiver.start(0, 0));
        QString senderId, tid;
        QByteArray bytes(100000, 'r');
        QString filePath = a.path() + "/resume.bin";
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(bytes);
        f.close();
        {
            Node sender("one", a.path());
            FileTransfer send(&sender);
            sender.serviceHandler = [&](auto s, auto r, auto h) { return send.handle(s, r, h); };
            QVERIFY(sender.start(0, 0));
            senderId = sender.id();
            sender.addPeer("127.0.0.1", receiver.port());
            QTRY_COMPARE(sender.peers().size(), 1);
            QSignalSpy offered(&receive, &FileTransfer::offerReceived);
            send.offerFile(receiver.id(), filePath);
            QTRY_COMPARE(offered.size(), 1);
            tid = offered.first()[0].toJsonObject()["id"].toString();
        }
        QFile partial(d.path() + "/.lanlink-" + tid + ".part");
        QVERIFY(partial.open(QIODevice::WriteOnly));
        partial.write(bytes.left(25000));
        partial.close();
        Node sender2("one", a.path());
        FileTransfer send2(&sender2);
        sender2.serviceHandler = [&](auto s, auto r, auto h) { return send2.handle(s, r, h); };
        QVERIFY(sender2.start(0, 0));
        QCOMPARE(sender2.id(), senderId);
        receiver.addPeer("127.0.0.1", sender2.port());
        QTRY_COMPARE(receiver.peers()[0].toObject()["port"].toInt(), int(sender2.port()));
        QSignalSpy completed(&receive, &FileTransfer::completed);
        receive.acceptFile(tid, d.path());
        QTRY_COMPARE(completed.size(), 1);
        QCOMPARE(FileTransfer::sha256(filePath), FileTransfer::sha256(d.path() + "/resume.bin"));
    }
    void noOverwrite() {
        QTemporaryDir a, b, d;
        Node n1("one", a.path()), n2("two", b.path());
        FileTransfer f1(&n1), f2(&n2);
        n1.serviceHandler = [&](auto s, auto r, auto h) { return f1.handle(s, r, h); };
        n2.serviceHandler = [&](auto s, auto r, auto h) { return f2.handle(s, r, h); };
        QVERIFY(n1.start(0, 0));
        QVERIFY(n2.start(0, 0));
        n1.addPeer("127.0.0.1", n2.port());
        QTRY_COMPARE(n1.peers().size(), 1);
        QFile source(a.path() + "/same.bin"), dest(d.path() + "/same.bin");
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write("new");
        source.close();
        QVERIFY(dest.open(QIODevice::WriteOnly));
        dest.write("original");
        dest.close();
        QSignalSpy offered(&f2, &FileTransfer::offerReceived), errors(&f2, &FileTransfer::error);
        f1.offerFile(n2.id(), source.fileName());
        QTRY_COMPARE(offered.size(), 1);
        f2.acceptFile(offered.first()[0].toJsonObject()["id"].toString(), d.path());
        QCOMPARE(errors.size(), 1);
        QVERIFY(dest.open(QIODevice::ReadOnly));
        QCOMPARE(dest.readAll(), QByteArray("original"));
    }
    void pythonInterop() {
        QString python = QStandardPaths::findExecutable("python3");
        if (python.isEmpty())
            python = QStandardPaths::findExecutable("python");
        if (python.isEmpty())
            QSKIP("Python not installed for compatibility fixture");
        QTemporaryDir a, d;
        Node n("cpp", a.path());
        FileTransfer files(&n);
        n.serviceHandler = [&](auto s, auto r, auto h) { return files.handle(s, r, h); };
        QVERIFY(n.start(0, 0));
        QFile f(a.path() + "/interop.bin");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("cpp-to-python");
        f.close();
        auto token = files.publishShare(a.path());
        connect(&files, &FileTransfer::offerReceived, &files,
                [&](QJsonObject o) { files.acceptFile(o["id"].toString(), d.path()); });
        QSignalSpy completed(&files, &FileTransfer::completed);
        QProcess proc;
        proc.start(python, {QFileInfo(QString::fromUtf8(__FILE__)).absolutePath() + "/python_file_peer.py",
                            QString::number(n.port()), token});
        QTRY_COMPARE_WITH_TIMEOUT(proc.state(), QProcess::NotRunning, 20000);
        QVERIFY2(proc.exitCode() == 0, proc.readAllStandardError().constData());
        QTRY_COMPARE(completed.size(), 1);
        QFile received(d.path() + "/python.bin");
        QVERIFY(received.open(QIODevice::ReadOnly));
        QCOMPARE(received.readAll(), QByteArray("python-to-cpp").repeated(100));
    }
    void destroyWithPendingRequests() {
        QTemporaryDir a;
        Node n("cpp", a.path());
        QVERIFY(n.start(0, 0));
        QTcpServer silent;
        QVERIFY(silent.listen(QHostAddress::LocalHost, 0));
        QString pid(32, 'd');
        QVERIFY(
            n.remember({{"id", pid}, {"name", "silent"}, {"port", int(silent.serverPort())}}, "127.0.0.1"));
        QFile f(a.path() + "/pending.bin");
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("pending");
        f.close();
        auto files = new FileTransfer(&n);
        QSignalSpy accepted(&silent, &QTcpServer::newConnection);
        files->offerFile(pid, f.fileName());
        QTRY_COMPARE(accepted.size(), 1);
        files->listShare(pid, "token");
        files->downloadShare(pid, "token", "file", a.path());
        delete files;
        n.stop();
        QCoreApplication::processEvents();
        while (silent.hasPendingConnections())
            delete silent.nextPendingConnection();
    }
    void hashSaturationQueues() {
        QTemporaryDir root;
        Node node("hash", root.path());
        FileTransfer files(&node);
        QString path = root.path() + "/hash.bin";
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(1024 * 1024, 'h'));
        file.close();
        QByteArray expected = FileTransfer::sha256(path);
        int completed = 0;
        for (int i = 0; i < 16; ++i)
            files.hashAsync(path, [&](QByteArray hash) {
                QCOMPARE(hash, expected);
                ++completed;
            });
        QCOMPARE(files.m_hashJobs, 4);
        QCOMPARE(files.m_hashQueue.size(), 12);
        QTRY_COMPARE(completed, 16);
        QCOMPARE(files.m_hashJobs, 0);
        QVERIFY(files.m_hashQueue.isEmpty());
    }
    void busyVerificationPreservesPart() {
        QTemporaryDir a, b, d;
        Node sender("sender", a.path()), receiver("receiver", b.path());
        FileTransfer send(&sender), receive(&receiver);
        sender.serviceHandler = [&](auto s, auto r, auto h) { return send.handle(s, r, h); };
        receiver.serviceHandler = [&](auto s, auto r, auto h) { return receive.handle(s, r, h); };
        QVERIFY(sender.start(0, 0));
        QVERIFY(receiver.start(0, 0));
        sender.addPeer("127.0.0.1", receiver.port());
        QTRY_COMPARE(sender.peers().size(), 1);
        QFile source(a.path() + "/busy.bin");
        QVERIFY(source.open(QIODevice::WriteOnly));
        source.write(QByteArray(8192, 'b'));
        source.close();
        QSignalSpy offered(&receive, &FileTransfer::offerReceived), errors(&receive, &FileTransfer::error),
            complete(&receive, &FileTransfer::completed);
        send.offerFile(receiver.id(), source.fileName());
        QTRY_COMPARE(offered.size(), 1);
        QString id = offered.first()[0].toJsonObject()["id"].toString();
        // Deterministically simulate all workers and the bounded queue occupied.
        receive.m_hashJobs = 4;
        for (int i = 0; i < 128; ++i)
            receive.m_hashQueue.enqueue({source.fileName(), [](QByteArray) {}});
        receive.acceptFile(id, d.path());
        QTRY_COMPARE(errors.size(), 1);
        QString part = d.path() + "/.lanlink-" + id + ".part";
        QVERIFY(QFileInfo::exists(part));
        QCOMPARE(QFileInfo(part).size(), qint64(8192));
        QVERIFY(!QFileInfo::exists(d.path() + "/busy.bin"));
        QCOMPARE(receive.incomingOffers().size(), 1);
        receive.m_hashJobs = 0;
        receive.m_hashQueue.clear();
        QTRY_VERIFY(!receive.m_active.contains(id));
        receive.acceptFile(id, d.path());
        QTRY_COMPARE(complete.size(), 1);
        QCOMPARE(FileTransfer::sha256(source.fileName()), FileTransfer::sha256(d.path() + "/busy.bin"));
    }
    void paths() {
        QTemporaryDir a, b;
        Node n1("one", a.path()), n2("two", b.path());
        FileTransfer f1(&n1);
        n1.serviceHandler = [&](auto s, auto r, auto h) { return f1.handle(s, r, h); };
        QVERIFY(n1.start(0, 0));
        QVERIFY(n2.start(0, 0));
        n2.addPeer("127.0.0.1", n1.port());
        QTRY_COMPARE(n2.peers().size(), 1);
        auto t = f1.publishShare(a.path());
        for (QString p : {"../", "/tmp", "C:/", "a\\b"}) {
            bool got = false;
            n2.request(n1.id(), {{"type", "share_list"}, {"token", t}, {"path", p}}, [&](auto r, auto) {
                QVERIFY(!r["ok"].toBool());
                got = true;
            });
            QTRY_VERIFY(got);
        }
    }
};
QTEST_GUILESS_MAIN(FileTests)
#include "test_files.moc"
