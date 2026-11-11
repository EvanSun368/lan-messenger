#include "filetransfer.h"
#include "node.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QUuid>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
class FileTests : public QObject {
    Q_OBJECT
  private slots:
    void resultReceiptsPersistAndRejectForgery() {
        QTemporaryDir a,b;
        Node one("one",a.path()),two("two",b.path());
        auto sender=std::make_unique<FileTransfer>(&one);
        auto receiver=std::make_unique<FileTransfer>(&two);
        bool dropResults=true;
        one.serviceHandler=[&](auto socket,auto request,auto host){
            if(dropResults && request["type"]=="file_result") {
                Node::writeFrame(socket,{{"ok",false}});socket->disconnectFromHost();return true;
            }
            return sender->handle(socket,request,host);
        };
        two.serviceHandler=[&](auto socket,auto request,auto host){return receiver->handle(socket,request,host);};
        QVERIFY(one.start(0,0));QVERIFY(two.start(0,0));
        one.addPeer("127.0.0.1",two.port());
        QTRY_COMPARE(one.peers().size(),1);QTRY_COMPARE(two.peers().size(),1);
        QFile source(a.path()+"/receipt.bin");QVERIFY(source.open(QIODevice::WriteOnly));source.write("abc");source.close();
        sender->offerFile(two.id(),source.fileName());
        QTRY_COMPARE(receiver->incomingOffers().size(),1);
        const auto offer=receiver->incomingOffers().first().toObject();const auto id=offer["id"].toString();
        receiver->rejectFile(id);
        QVERIFY(receiver->transferSnapshot(id)["result_pending"].toBool());
        receiver.reset();receiver=std::make_unique<FileTransfer>(&two);
        sender.reset();sender=std::make_unique<FileTransfer>(&one);
        // A legacy release frees the ticket but cannot declare completion.
        bool released=false;
        two.request(one.id(),{{"type","file_release"},{"id",id}},[&](auto,auto){released=true;});
        QTRY_VERIFY(released);
        QVERIFY(sender->transferSnapshot(id)["state"]!="remote_completed");
        const auto storage=sender->m_storagePath;
        sender->m_storagePath=a.path()+"/absent-parent/state.json";
        dropResults=false;receiver->sendResults();
        QTest::qWait(60);
        QVERIFY(sender->transferSnapshot(id)["state"]!="remote_rejected");
        QVERIFY(receiver->transferSnapshot(id)["result_pending"].toBool());
        sender->m_storagePath=storage;
        receiver->sendResults();
        QTRY_COMPARE(sender->transferSnapshot(id)["state"].toString(),QString("remote_rejected"));
        QTRY_VERIFY(!receiver->transferSnapshot(id)["result_pending"].toBool());
        QJsonObject result{{"type","file_result"},{"version",1},{"id",id},{"sender",two.hello()},
            {"size",offer["size"]},{"sha256",offer["sha256"]},{"state","rejected"},{"sequence",1}};
        auto request=[&](QJsonObject packet) {
            bool done=false;bool ok=false;
            two.request(one.id(),packet,[&](auto response,auto){ok=response["ok"].toBool();done=true;});
            QTest::qWait(1);
            for(int i=0;i<200 && !done;++i)QTest::qWait(5);
            return done && ok;
        };
        QVERIFY(request(result)); // exact duplicate is idempotent
        auto changed=result;changed["state"]="completed";QVERIFY(!request(changed));
        changed=result;changed["sequence"]=2;QVERIFY(!request(changed));
        changed=result;changed["version"]=1.5;QVERIFY(!request(changed));
        changed=result;changed["sha256"]=QString(64,'f');QVERIFY(!request(changed));
        changed=result;changed["size"]=4;QVERIFY(!request(changed));
        QTemporaryDir c;Node outsider("outsider",c.path());QVERIFY(outsider.start(0,0));
        outsider.addPeer("127.0.0.1",one.port());QTRY_COMPARE(outsider.peers().size(),1);
        bool forgedDone=false,forgedOk=true;
        outsider.request(one.id(),result,[&](auto response,auto){forgedOk=response["ok"].toBool();forgedDone=true;});
        QTRY_VERIFY(forgedDone);QVERIFY(!forgedOk);
        changed=result;changed["id"]=QUuid::createUuid().toString(QUuid::Id128);QVERIFY(!request(changed));
        QCOMPARE(sender->transferSnapshot(id)["state"].toString(),QString("remote_rejected"));
        sender.reset();sender=std::make_unique<FileTransfer>(&one);
        QCOMPARE(sender->transferSnapshot(id)["state"].toString(),QString("remote_rejected"));
    }
    void corruptReceiverReportsFailureNotCompletion() {
        QTemporaryDir a,b,d;Node one("one",a.path()),two("two",b.path());
        FileTransfer sender(&one),receiver(&two);
        one.serviceHandler=[&](auto s,auto r,auto h){return sender.handle(s,r,h);};
        two.serviceHandler=[&](auto s,auto r,auto h){return receiver.handle(s,r,h);};
        QVERIFY(one.start(0,0));QVERIFY(two.start(0,0));one.addPeer("127.0.0.1",two.port());
        QTRY_COMPARE(one.peers().size(),1);
        QFile source(a.path()+"/corrupt.bin");QVERIFY(source.open(QIODevice::WriteOnly));source.write("abc");source.close();
        sender.offerFile(two.id(),source.fileName());QTRY_COMPARE(receiver.incomingOffers().size(),1);
        const auto id=receiver.incomingOffers().first().toObject()["id"].toString();
        QSignalSpy complete(&receiver,&FileTransfer::completed);
        connect(&receiver,&FileTransfer::progress,&receiver,[&](QString task,qint64,qint64,QString status){
            if(task==id && status=="校验中") {
                QFile part(d.path()+"/.lanlink-"+id+".part");
                QVERIFY(part.open(QIODevice::WriteOnly));part.write("BAD");part.close();
            }
        });
        receiver.acceptFile(id,d.path());
        QTRY_COMPARE(sender.transferSnapshot(id)["state"].toString(),QString("remote_failed"));
        QCOMPARE(complete.size(),0);QVERIFY(!QFileInfo::exists(d.path()+"/corrupt.bin"));
        QCOMPARE(sender.transferSnapshot(id)["done"].toInt(),3);
    }
    void rejectionProgressMayDestroyService() {
        QTemporaryDir root;Node node("node",root.path());
        QPointer<FileTransfer> service=new FileTransfer(&node);
        const auto id=QUuid::createUuid().toString(QUuid::Id128);
        service->m_incoming[id]=QJsonObject{{"id",id},{"size",3}};
        service->m_verifying.insert(id);
        connect(service,&FileTransfer::progress,&node,[&]{delete service.data();});
        service->rejectFile(id);
        QVERIFY(!service);
    }
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
        QTRY_COMPARE(f1.transferSnapshot(offer["id"].toString())["state"].toString(),QString("remote_completed"));
        QTRY_VERIFY(!f2.transferSnapshot(offer["id"].toString())["result_pending"].toBool());
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
    void cancelDuringVerification_data() {
        QTest::addColumn<bool>("reject");
        QTest::newRow("pause then resume") << false;
        QTest::newRow("reject never publishes") << true;
    }
    void cancelDuringVerification() {
        QFETCH(bool,reject);
        QTemporaryDir a,b,d; Node sender("sender",a.path()),receiver("receiver",b.path());
        FileTransfer send(&sender),receive(&receiver);
        sender.serviceHandler=[&](auto s,auto r,auto h){return send.handle(s,r,h);};
        receiver.serviceHandler=[&](auto s,auto r,auto h){return receive.handle(s,r,h);};
        QVERIFY(sender.start(0,0)); QVERIFY(receiver.start(0,0));
        sender.addPeer("127.0.0.1",receiver.port()); QTRY_COMPARE(sender.peers().size(),1);
        QFile source(a.path()+"/cancel.bin"); QVERIFY(source.open(QIODevice::WriteOnly));
        source.write(QByteArray(131072,'c')); source.close();
        QSignalSpy offers(&receive,&FileTransfer::offerReceived),done(&receive,&FileTransfer::completed);
        send.offerFile(receiver.id(),source.fileName()); QTRY_COMPARE(offers.size(),1);
        const auto id=offers.first()[0].toJsonObject()["id"].toString();
        bool cancelled=false;
        auto connection=connect(&receive,&FileTransfer::progress,&receive,[&](QString task,qint64,qint64,QString status){
            if(task==id && status=="校验中" && !cancelled) {
                cancelled=true;
                if(reject) receive.rejectFile(id); else receive.pauseFile(id);
            }
        });
        receive.acceptFile(id,d.path()); QTRY_VERIFY(cancelled);
        QTRY_VERIFY(!receive.m_verifying.contains(id));
        QCOMPARE(done.size(),0); QVERIFY(!QFileInfo::exists(d.path()+"/cancel.bin"));
        QCOMPARE(receive.transferSnapshot(id)["done"].toDouble(),double(131072));
        QCOMPARE(receive.transferSnapshot(id)["state"].toString(),reject?QString("rejected"):QString("paused"));
        QVERIFY(QFileInfo::exists(d.path()+"/.lanlink-"+id+".part"));
        QTRY_COMPARE(send.transferSnapshot(id)["state"].toString(),reject?QString("remote_rejected"):QString("remote_paused"));
        disconnect(connection);
        if(reject) QCOMPARE(receive.incomingOffers().size(),0);
        else {
            QTRY_VERIFY(!receive.m_active.contains(id));
            receive.acceptFile(id,d.path()); QTRY_COMPARE(done.size(),1);
            QTRY_COMPARE(send.transferSnapshot(id)["state"].toString(),QString("remote_completed"));
            QCOMPARE(FileTransfer::sha256(source.fileName()),FileTransfer::sha256(d.path()+"/cancel.bin"));
        }
    }
    void actualSharedDownloadAndContext() {
        QTemporaryDir a,b,d; Node sender("sender",a.path()),receiver("receiver",b.path());
        FileTransfer send(&sender),receive(&receiver);
        sender.serviceHandler=[&](auto s,auto r,auto h){return send.handle(s,r,h);};
        receiver.serviceHandler=[&](auto s,auto r,auto h){return receive.handle(s,r,h);};
        QVERIFY(sender.start(0,0)); QVERIFY(receiver.start(0,0));
        receiver.addPeer("127.0.0.1",sender.port()); QTRY_COMPARE(receiver.peers().size(),1);
        QVERIFY(QDir(a.path()).mkdir("nested"));
        const QString name=QString::fromUtf8("固件 数据.bin");
        QFile source(a.path()+"/nested/"+name); QVERIFY(source.open(QIODevice::WriteOnly));
        source.write(QByteArray(262144,'s')); source.close();
        const auto share=send.publishShare(a.path()); QVERIFY(!share.isEmpty());
        QSignalSpy lists(&receive,&FileTransfer::shareListedFor),done(&receive,&FileTransfer::completed);
        receive.listShare(sender.id(),share,"nested"); QTRY_COMPARE(lists.size(),1);
        QCOMPARE(lists.first()[0].toString(),sender.id()); QCOMPARE(lists.first()[1].toString(),share);
        QCOMPARE(lists.first()[2].toString(),QString("nested"));
        QCOMPARE(lists.first()[3].toJsonArray().first().toObject()["name"].toString(),name);
        receive.downloadShare(sender.id(),share,"nested/"+name,d.path()); QTRY_COMPARE(done.size(),1);
        QCOMPARE(FileTransfer::sha256(source.fileName()),FileTransfer::sha256(d.path()+"/"+name));
        const auto completedId=done.first()[0].toString();
        QVERIFY(!receive.transferSnapshot(completedId)["result_pending"].toBool());
        QTRY_VERIFY(!send.m_outgoing.contains(completedId));
        QCOMPARE(receive.transferSnapshot(completedId)["state"].toString(),QString("completed"));
        QCOMPARE(receive.transferSnapshot(completedId)["done"].toDouble(),double(262144));
        {
            FileTransfer restored(&receiver);
            QCOMPARE(restored.transferSnapshot(completedId)["state"].toString(),QString("validating"));
            QTRY_COMPARE(restored.transferSnapshot(completedId)["state"].toString(),QString("completed"));
        }
        QFile changed(d.path()+"/"+name);QVERIFY(changed.open(QIODevice::WriteOnly));changed.write(QByteArray(262144,'x'));changed.close();
        {
            FileTransfer restored(&receiver);
            QTRY_COMPARE(restored.transferSnapshot(completedId)["state"].toString(),QString("missing"));
        }

        QFile empty(a.path()+"/nested/empty.bin");QVERIFY(empty.open(QIODevice::WriteOnly));empty.close();
        receive.downloadShare(sender.id(),share,"nested/empty.bin",d.path());QTRY_COMPARE(done.size(),2);
        QVERIFY(QFileInfo::exists(d.path()+"/empty.bin"));QCOMPARE(QFileInfo(d.path()+"/empty.bin").size(),qint64(0));
    }
    void hashCallbackCanDestroyService() {
        QTemporaryDir root; Node node("n",root.path());
        QFile f(root.path()+"/tiny"); QVERIFY(f.open(QIODevice::WriteOnly));f.write("x");f.close();
        QPointer<FileTransfer> service=new FileTransfer(&node);
        bool called=false;
        service->hashAsync(f.fileName(),[&](QByteArray){called=true;delete service.data();});
        QTRY_VERIFY(called); QVERIFY(service.isNull());
    }
    void duplicateFetchDoesNotReplaceActiveSocket() {
        QTemporaryDir a,b;Node sender("sender",a.path()),receiver("receiver",b.path());
        FileTransfer send(&sender),receive(&receiver);
        sender.serviceHandler=[&](auto s,auto r,auto h){return send.handle(s,r,h);};
        receiver.serviceHandler=[&](auto s,auto r,auto h){return receive.handle(s,r,h);};
        QVERIFY(sender.start(0,0));QVERIFY(receiver.start(0,0));
        sender.addPeer("127.0.0.1",receiver.port());QTRY_COMPARE(sender.peers().size(),1);
        QFile source(a.path()+"/large.bin");QVERIFY(source.open(QIODevice::WriteOnly));
        source.write(QByteArray(16*1024*1024,'d'));source.close();
        QSignalSpy offers(&receive,&FileTransfer::offerReceived);
        send.offerFile(receiver.id(),source.fileName());QTRY_COMPARE(offers.size(),1);
        const auto id=offers.first()[0].toJsonObject()["id"].toString();
        QTcpSocket first;first.setReadBufferSize(1);first.connectToHost(QHostAddress::LocalHost,sender.port());
        QTRY_COMPARE(first.state(),QAbstractSocket::ConnectedState);
        Node::writeFrame(&first,{{"type","fetch"},{"id",id},{"offset",0}});
        QTRY_VERIFY(send.m_active.value(id));const auto original=send.m_active.value(id);
        bool replied=false;QJsonObject reply;
        receiver.request(sender.id(),{{"type","fetch"},{"id",id},{"offset",0}},[&](auto r,auto){reply=r;replied=true;});
        QTRY_VERIFY(replied);QVERIFY(!reply["ok"].toBool());
        QCOMPARE(reply["error"].toString(),QString("Transfer already active"));
        QCOMPARE(send.m_active.value(id),original);
        first.abort();QTRY_VERIFY(!send.m_active.contains(id));
    }
    void offerAckPrecedesModalNotification() {
        QTemporaryDir a,b;Node sender("sender",a.path()),receiver("receiver",b.path());
        FileTransfer send(&sender),receive(&receiver);
        sender.serviceHandler=[&](auto s,auto r,auto h){return send.handle(s,r,h);};
        receiver.serviceHandler=[&](auto s,auto r,auto h){return receive.handle(s,r,h);};
        QVERIFY(sender.start(0,0));QVERIFY(receiver.start(0,0));
        sender.addPeer("127.0.0.1",receiver.port());QTRY_COMPARE(sender.peers().size(),1);
        QFile source(a.path()+"/modal.bin");QVERIFY(source.open(QIODevice::WriteOnly));source.write("test");source.close();
        bool acked=false,notified=false,ackDuringUi=false;
        connect(&send,&FileTransfer::progress,&send,[&](QString,qint64,qint64,QString status){if(status=="等待接收") acked=true;});
        connect(&receive,&FileTransfer::offerReceived,&receive,[&](QJsonObject){
            QEventLoop modal;QTimer::singleShot(200,&modal,&QEventLoop::quit);modal.exec();
            ackDuringUi=acked;notified=true;
        });
        send.offerFile(receiver.id(),source.fileName());QTRY_VERIFY(notified);QVERIFY(ackDuringUi);
    }
    void persistenceFailureIsReportedAndRecoverable() {
        QTemporaryDir dir;Node node("node",dir.path());FileTransfer service(&node);
        QSignalSpy errors(&service,&FileTransfer::error);
        const auto path=dir.filePath("cpp-transfers.json");QVERIFY(QFile::remove(path));
        QVERIFY(QDir().mkdir(path)); // Deterministic open/commit target failure, no permission assumptions.
        QVERIFY(!service.persist());QVERIFY(!service.persistenceError().isEmpty());
        QTRY_COMPARE(errors.size(),1);
        QVERIFY(errors.first()[0].toString().contains("无法保存"));
        QVERIFY(!service.persist());QCoreApplication::processEvents();QCOMPARE(errors.size(),1);
        QVERIFY(QDir().rmdir(path));QVERIFY(service.persist());QVERIFY(service.persistenceError().isEmpty());
        QFile saved(path);QVERIFY(saved.open(QIODevice::ReadOnly));QVERIFY(QJsonDocument::fromJson(saved.readAll()).isObject());
    }
    void recordNotificationCanDestroyService() {
        QTemporaryDir dir;Node node("node",dir.path());QPointer<FileTransfer> service=new FileTransfer(&node);
        connect(service,&FileTransfer::transferChanged,&node,[&](QJsonObject){delete service.data();});
        service->recordTransfer("12345678901234567890123456789012",{{"name","file.bin"},{"size",3}},node.id(),"in");
        QVERIFY(service); // Notification is intentionally outside the state mutation stack.
        QTRY_VERIFY(service.isNull());
    }
    void groupFileFirstMessageAndContextValidation() {
        QTemporaryDir a,b,c,bd,cd,x;
        Node sender("sender",a.path()),bob("bob",b.path()),carol("carol",c.path()),outsider("outsider",x.path());
        FileTransfer send(&sender),receiveB(&bob),receiveC(&carol);
        sender.serviceHandler=[&](auto s,auto r,auto h){return send.handle(s,r,h);};
        bob.serviceHandler=[&](auto s,auto r,auto h){return receiveB.handle(s,r,h);};
        carol.serviceHandler=[&](auto s,auto r,auto h){return receiveC.handle(s,r,h);};
        QVERIFY(sender.start(0,0));QVERIFY(bob.start(0,0));QVERIFY(carol.start(0,0));QVERIFY(outsider.start(0,0));
        sender.addPeer("127.0.0.1",bob.port());sender.addPeer("127.0.0.1",carol.port());
        outsider.addPeer("127.0.0.1",bob.port());
        QTRY_COMPARE(sender.peers().size(),2);QTRY_COMPARE(outsider.peers().size(),1);
        const auto gid=sender.createGroup({bob.id(),carol.id()},"File-first group");QVERIFY(!gid.isEmpty());
        const auto conversation="group:"+gid;
        QVERIFY(bob.groups().isEmpty());QVERIFY(carol.groups().isEmpty());
        QFile source(a.filePath("group.bin"));QVERIFY(source.open(QIODevice::WriteOnly));source.write(QByteArray(8192,'g'));source.close();
        QSignalSpy offersB(&receiveB,&FileTransfer::offerReceived),offersC(&receiveC,&FileTransfer::offerReceived);
        QSignalSpy doneB(&receiveB,&FileTransfer::completed),doneC(&receiveC,&FileTransfer::completed);
        send.offerFile(bob.id(),source.fileName(),conversation);send.offerFile(carol.id(),source.fileName(),conversation);
        QTRY_COMPARE(offersB.size(),1);QTRY_COMPARE(offersC.size(),1);
        const auto offerB=offersB.first()[0].toJsonObject(),offerC=offersC.first()[0].toJsonObject();
        const auto idB=offerB["id"].toString(),idC=offerC["id"].toString();
        QCOMPARE(receiveB.transferSnapshot(idB)["conversation_id"].toString(),conversation);
        QCOMPARE(receiveC.transferSnapshot(idC)["conversation_id"].toString(),conversation);
        QTRY_VERIFY(send.transferSnapshot(idB)["remote_group_context_supported"].toBool());
        QTRY_VERIFY(send.transferSnapshot(idC)["remote_group_context_supported"].toBool());
        QVERIFY(!QFileInfo::exists(bd.filePath("group.bin"))); // Still waiting, no implicit acceptance.
        receiveB.acceptFile(idB,bd.path());receiveC.acceptFile(idC,cd.path());
        QTRY_COMPARE(doneB.size(),1);QTRY_COMPARE(doneC.size(),1);
        QCOMPARE(FileTransfer::sha256(source.fileName()),FileTransfer::sha256(bd.filePath("group.bin")));
        QCOMPARE(FileTransfer::sha256(source.fileName()),FileTransfer::sha256(cd.filePath("group.bin")));
        auto exchange=[&](Node &from,QJsonObject payload) {
            QJsonObject response;bool done=false;QEventLoop loop;
            from.request(bob.id(),payload,[&](auto r,auto){response=r;done=true;loop.quit();},1000);
            if(!done)loop.exec();return response;
        };
        QVERIFY(!exchange(sender,offerB)["ok"].toBool()); // Completed ID cannot be replayed as a new invitation.
        QSignalSpy reflected(&send,&FileTransfer::offerReceived);
        receiveB.offerFile(sender.id(),source.fileName());QTRY_COMPARE(reflected.size(),1);
        auto collision=offerB;collision["id"]=reflected.first()[0].toJsonObject()["id"];
        QVERIFY(!exchange(sender,collision)["ok"].toBool()); // Cannot overwrite Bob's outgoing direction/context.
        send.rejectFile(collision["id"].toString());
        auto offer=offerB;offer["id"]=QUuid::createUuid().toString(QUuid::Id128);
        auto context=offer["group_context"].toObject();context["version"]=99;offer["group_context"]=context;
        QVERIFY(!exchange(sender,offer)["ok"].toBool());
        context=offerB["group_context"].toObject();context["members"]=QJsonArray{sender.id(),bob.id()};offer["group_context"]=context;
        QVERIFY(!exchange(sender,offer)["ok"].toBool()); // Existing group's immutable members differ.
        offer["group_context"]=QJsonObject{{"version",1},{"id",gid}};
        QVERIFY(!exchange(sender,offer)["ok"].toBool()); // A claimed group id alone proves nothing.
        context=offerB["group_context"].toObject();context["members"]=QJsonArray{sender.id(),bob.id(),bob.id()};offer["group_context"]=context;
        QVERIFY(!exchange(sender,offer)["ok"].toBool());
        context=offerB["group_context"].toObject();context["id"]=QUuid::createUuid().toString(QUuid::Id128);context["members"]=QJsonArray{sender.id(),carol.id()};offer["group_context"]=context;
        QVERIFY(!exchange(sender,offer)["ok"].toBool());QCOMPARE(bob.groups().size(),1); // Receiver absent: do not create group.
        offer["group_context"]=offerB["group_context"];
        QVERIFY(!exchange(outsider,offer)["ok"].toBool()); // Request sender not a group member.
        context=offerB["group_context"].toObject();context["name"]="Malicious rename";offer["group_context"]=context;
        const auto renamed=exchange(sender,offer);QVERIFY(renamed["ok"].toBool());
        QCOMPARE(bob.groups().first().toObject()["name"].toString(),QString("File-first group"));
        receiveB.rejectFile(offer["id"].toString());
        offer.remove("group_context");offer["id"]=QUuid::createUuid().toString(QUuid::Id128);
        const auto legacy=exchange(sender,offer);QVERIFY(legacy["ok"].toBool());
        QCOMPARE(receiveB.transferSnapshot(offer["id"].toString())["conversation_id"].toString(),sender.id());
        receiveB.rejectFile(offer["id"].toString());
    }
    void legacyReceiverGroupRouteRemainsUnconfirmed() {
        QTemporaryDir a,b;Node sender("sender",a.path()),legacy("legacy",b.path());FileTransfer send(&sender);
        QVERIFY(sender.start(0,0));QVERIFY(legacy.start(0,0));
        legacy.serviceHandler=[](QTcpSocket *s,QJsonObject r,QString){
            if(r["type"]!="offer")return false;
            Node::writeFrame(s,{{"ok",true}});s->disconnectFromHost();return true;
        };
        sender.addPeer("127.0.0.1",legacy.port());QTRY_COMPARE(sender.peers().size(),1);
        const auto gid=sender.createGroup({legacy.id()},"Legacy group");
        QFile file(a.filePath("old.bin"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("old");file.close();
        send.offerFile(legacy.id(),file.fileName(),"group:"+gid);
        QTRY_COMPARE(send.transferSnapshots().size(),1);
        const auto id=send.transferSnapshots().first().toObject()["id"].toString();
        QTRY_COMPARE(send.transferSnapshot(id)["remote_group_route"].toString(),QString("unconfirmed"));
        QVERIFY(!send.transferSnapshot(id)["remote_group_context_supported"].toBool());
    }
    void groupNotificationMayDestroyReceiverAndService() {
        QTemporaryDir a,b;Node sender("sender",a.path());FileTransfer send(&sender);
        QPointer<Node> receiver=new Node("receiver",b.path());
        QPointer<FileTransfer> receive=new FileTransfer(receiver,receiver);
        receiver->serviceHandler=[receive](auto s,auto r,auto h){return receive?receive->handle(s,r,h):false;};
        QVERIFY(sender.start(0,0));QVERIFY(receiver->start(0,0));
        sender.addPeer("127.0.0.1",receiver->port());QTRY_COMPARE(sender.peers().size(),1);
        const auto receiverId=receiver->id();const auto gid=sender.createGroup({receiverId},"Disposable group");
        connect(receiver,&Node::event,&sender,[&](QString kind,QJsonObject){if(kind=="group")delete receiver.data();});
        QFile file(a.filePath("dispose.bin"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("test");file.close();
        send.offerFile(receiverId,file.fileName(),"group:"+gid);
        QTRY_VERIFY(receiver.isNull());QVERIFY(receive.isNull());
        QCoreApplication::processEvents();
    }
    void sharedResponseCannotRedirectIdentityOrGroup_data() {
        QTest::addColumn<int>("mode");
        QTest::newRow("different-known-peer")<<0;QTest::newRow("unexpected-group-context")<<1;QTest::newRow("wrong-type")<<2;
    }
    void sharedResponseCannotRedirectIdentityOrGroup() {
        QFETCH(int,mode);QTemporaryDir a,b,c,d;Node server("share",a.path()),receiver("receiver",b.path()),third("third",c.path());
        FileTransfer files(&receiver);QSignalSpy errors(&files,&FileTransfer::error);
        QVERIFY(server.start(0,0));QVERIFY(receiver.start(0,0));QVERIFY(third.start(0,0));
        receiver.addPeer("127.0.0.1",server.port());receiver.addPeer("127.0.0.1",third.port());QTRY_COMPARE(receiver.peers().size(),2);
        server.serviceHandler=[&](QTcpSocket *socket,QJsonObject request,QString){
            if(request["type"]!="share_get")return false;
            QJsonObject offer{{"type",mode==2?"message":"offer"},{"id",QUuid::createUuid().toString(QUuid::Id128)},
                {"name","unexpected.bin"},{"size",0},{"sha256",QString(64,'0')},{"sender",mode==0?third.hello():server.hello()}};
            if(mode==1)offer["group_context"]=QJsonObject{{"version",1},{"id",QUuid::createUuid().toString(QUuid::Id128)},
                {"name","Injected"},{"members",QJsonArray{server.id(),receiver.id()}}};
            Node::writeFrame(socket,{{"ok",true},{"offer",offer}});socket->disconnectFromHost();return true;
        };
        files.downloadShare(server.id(),"token","unexpected.bin",d.path());QTRY_COMPARE(errors.size(),1);
        QVERIFY(files.incomingOffers().isEmpty());QVERIFY(files.transferSnapshots().isEmpty());QVERIFY(receiver.groups().isEmpty());
        QVERIFY(QDir(d.path()).entryList(QDir::Files|QDir::Hidden|QDir::NoDotAndDotDot).isEmpty());
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
#ifndef Q_OS_WIN
        QFile outside(b.path()+"/outside"); QVERIFY(outside.open(QIODevice::WriteOnly));outside.write("secret-test");outside.close();
        QVERIFY(QFile::link(outside.fileName(),a.path()+"/escape"));
        QVERIFY(f1.sharePath(t,"escape").isEmpty());
#endif
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
