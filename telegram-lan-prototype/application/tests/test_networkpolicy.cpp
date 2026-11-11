#include "node.h"
#include "filetransfer.h"
#include "screenservice.h"
#include <QBuffer>
#include <QImage>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QtTest>

// This suite uses only loopback. It exercises VPN *policy mode*, not a real
// encrypted tunnel, routes, NAT traversal, a firewall or cross-city delivery.
// Interface observations are deterministic fixtures; no OS settings are changed.
class NetworkPolicyTest : public QObject {
    Q_OBJECT
    NetworkPolicy localPolicy() {
        NetworkPolicy p; p.vpnOnly=true; p.localAddress=QHostAddress::LocalHost;
        p.allowedPeers={QHostAddress::LocalHost};
        p.interfaceName="fixture-loopback";
        p.m_interfaceSnapshot=[] { return QJsonArray{QJsonObject{{"name","fixture-loopback"},
            {"up",true},{"running",true},{"ipv4",QJsonArray{"127.0.0.1"}}}}; };
        return p;
    }
    quint16 freePort() {
        QTcpServer server;
        if(!server.listen(QHostAddress::LocalHost,0))return 0;
        return server.serverPort();
    }
    bool startVpn(Node &n) {
        auto p=localPolicy();
        return n.setNetworkPolicy(p) && n.start(freePort(),45871);
    }
  private slots:
    void realInterfaceEnumerationWhenAvailable() {
        bool found=false;
        for (const auto &iface : NetworkPolicy::interfaces())
            if (iface.toObject()["ipv4"].toArray().contains("127.0.0.1")) found=true;
        if (!found) QSKIP("This environment cannot enumerate a real loopback interface; transport tests use explicit interface fixtures");
        NetworkPolicy real;real.vpnOnly=true;real.localAddress=QHostAddress::LocalHost;real.allowedPeers={QHostAddress::LocalHost};
        for (const auto &iface : NetworkPolicy::interfaces())
            if (iface.toObject()["ipv4"].toArray().contains("127.0.0.1")) real.interfaceName=iface.toObject()["name"].toString();
        QVERIFY(real.availabilityError().isEmpty());
    }
    void addressBoundariesAndExactAllowlist() {
        NetworkPolicy lan;
        QVERIFY(lan.allows(QHostAddress("10.1.2.3")));
        for(const auto &s:QStringList{"8.8.8.8","100.64.0.1","100.127.255.254","100.128.0.1","0.0.0.0","224.0.0.1"})
            QVERIFY2(!lan.allows(QHostAddress(s)),qPrintable(s));
        auto vpn=localPolicy();vpn.allowedPeers={QHostAddress("100.64.0.1"),QHostAddress("10.8.0.2")};
        QVERIFY(vpn.configurationError().isEmpty());
        QVERIFY(vpn.allows(QHostAddress("100.64.0.1")));
        QVERIFY(vpn.allows(QHostAddress("10.8.0.2")));
        for(const auto &s:QStringList{"100.64.0.2","100.63.255.255","100.128.0.0","10.8.0.3","127.0.0.1","::ffff:100.64.0.1"})
            QVERIFY2(!vpn.allows(QHostAddress(s)),qPrintable(s));
        for(const auto &s:QStringList{"8.8.8.8","example.com","100.64.0.0/10","::1","fc00::1","0.0.0.0"}) {
            vpn.allowedPeers={QHostAddress(s)};
            QVERIFY2(!vpn.configurationError().isEmpty(),qPrintable(s));
        }
        vpn.allowedPeers.clear();QVERIFY(!vpn.configurationError().isEmpty());
        lan.allowedPeers={QHostAddress("100.64.0.1")};QVERIFY(!lan.configurationError().isEmpty());
    }
    void legacyQueueMigratesWithoutChangingScope() {
        QTemporaryDir dir;
        const QString connection="vpn-legacy-fixture";
        {
            auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(dir.path()+"/history.sqlite");QVERIFY(db.open());
            QSqlQuery q(db);QVERIFY(q.exec("CREATE TABLE outbox(id TEXT,recipient TEXT,payload TEXT NOT NULL,attempts INTEGER DEFAULT 0,error TEXT DEFAULT '',PRIMARY KEY(id,recipient))"));
            q.prepare("INSERT INTO outbox(id,recipient,payload) VALUES(?,?,?)");
            q.addBindValue(QString(32,'a'));q.addBindValue(QString(32,'f'));q.addBindValue("{\"type\":\"message\"}");QVERIFY(q.exec());
            q.prepare("INSERT INTO outbox(id,recipient,payload) VALUES(?,?,?)");
            q.addBindValue(QString(32,'b'));q.addBindValue(QString(32,'f'));q.addBindValue("{\"type\":\"message\",\"_network_mode\":\"vpn\"}");QVERIFY(q.exec());
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);
        Node node("a",dir.path());QVERIFY(node.m_queueScopeReady);
        QSqlQuery q("SELECT id,network_mode FROM outbox ORDER BY id",node.m_db);
        QVERIFY(q.next());QCOMPARE(q.value(1).toString(),QString("lan"));
        QVERIFY(q.next());QCOMPARE(q.value(1).toString(),QString("vpn"));QVERIFY(!q.next());
    }
    void configurationFailsClosedAndFixedPort() {
        QTemporaryDir dir;Node n("a",dir.path());auto vpn=localPolicy();
        QVERIFY(vpn.availabilityError().isEmpty());
        vpn.interfaceName="__lanlink_missing_interface__";
        QVERIFY(n.setNetworkPolicy(vpn));QVERIFY(!n.start(45870,0));QCOMPARE(n.port(),0);
        auto report=n.networkDiagnostics();QVERIFY(!report["last_error"].toString().isEmpty());
        vpn=localPolicy();vpn.localAddress=QHostAddress("100.64.12.34");
        QVERIFY(!vpn.availabilityError().isEmpty());
        QVERIFY(n.setNetworkPolicy(localPolicy()));QVERIFY(!n.start(0,0));
        const auto port=freePort();QVERIFY(port);QVERIFY(n.start(port));QCOMPARE(n.port(),port);
        report=n.networkDiagnostics();QVERIFY(report["listening"].toBool());
        QVERIFY(!report["discovery_enabled"].toBool());QVERIFY(!report["vpn_tunnel_verified"].toBool());
        QVERIFY(!n.setNetworkPolicy(NetworkPolicy()));
        Node collision("collision",dir.path()+"/collision");QVERIFY(collision.setNetworkPolicy(localPolicy()));
        QVERIFY(!collision.start(port));QCOMPARE(collision.port(),0);
    }
    void outgoingAndIncomingRejectUnapprovedAddress() {
        QTemporaryDir dir;Node n("a",dir.path());QVERIFY(startVpn(n));
        bool done=false;QString error;
        n.requestAddress("127.0.0.2",n.port(),{{"type","hello"}},[&](auto,auto e){error=e;done=true;});
        QVERIFY(done);QVERIFY(!error.isEmpty());
        auto peer=QJsonObject{{"id",QString(32,'a')},{"port",45870}};
        QVERIFY(!n.remember(peer,"127.0.0.2"));
        QTcpSocket denied;QVERIFY(denied.bind(QHostAddress("127.0.0.2"),0));
        denied.connectToHost(QHostAddress::LocalHost,n.port());
        QVERIFY(denied.waitForConnected());denied.write(Node::frame({{"type","hello"}}));
        QTRY_VERIFY(denied.state()==QAbstractSocket::UnconnectedState);
        QCOMPARE(denied.bytesAvailable(),0);
    }
    void chatFilesSharesAndScreenUseSamePolicy() {
        QTemporaryDir dir;
        Node a("a",dir.path()+"/a"),b("b",dir.path()+"/b");
        QVERIFY(startVpn(a));QVERIFY(startVpn(b));
        FileTransfer fa(&a),fb(&b);ScreenService sa(&a),sb(&b);
        a.serviceHandler=[&](auto s,auto r,auto h){return fa.handle(s,r,h)||sa.handle(s,r,h);};
        b.serviceHandler=[&](auto s,auto r,auto h){return fb.handle(s,r,h)||sb.handle(s,r,h);};
        a.addPeer("127.0.0.1",b.port());QTRY_COMPARE(a.peers().size(),1);QTRY_COMPARE(b.peers().size(),1);
        QCOMPARE(a.peers().first().toObject()["network_mode"].toString(),QString("vpn"));
        const auto message=a.sendMessage(b.id(),"VPN policy local integration 测试");
        QVERIFY(!message.isEmpty());QTRY_COMPARE(b.history(a.id()).size(),1);
        QTRY_COMPARE(a.deliveryStatus(message)["received"].toInt(),1);
        const auto group=a.createGroup({b.id()},"VPN fixture group");QVERIFY(!group.isEmpty());
        const auto groupMessage=a.sendMessage("group:"+group,"group across policy gate");QVERIFY(!groupMessage.isEmpty());
        QTRY_COMPARE(a.deliveryStatus(groupMessage)["received"].toInt(),1);
        QTRY_COMPARE(b.history("group:"+group).size(),1);
        bool diagnosed=false;
        a.diagnoseAddress("127.0.0.1",b.port(),[&](auto data,auto error){
            QVERIFY(error.isEmpty());QVERIFY(data["reachable"].toBool());QVERIFY(!data["identity_verified"].toBool());diagnosed=true;
        });QTRY_VERIFY(diagnosed);
        const auto source=dir.path()+"/payload.bin";QFile f(source);QVERIFY(f.open(QIODevice::WriteOnly));
        const QByteArray bytes(200000,'v');QCOMPARE(f.write(bytes),bytes.size());f.close();
        fa.offerFile(b.id(),source);QTRY_COMPARE(fb.incomingOffers().size(),1);
        const auto transfer=fb.incomingOffers().first().toObject()["id"].toString();
        QDir().mkdir(dir.path()+"/received");fb.acceptFile(transfer,dir.path()+"/received");
        QTRY_COMPARE(fa.transferSnapshot(transfer)["state"].toString(),QString("remote_completed"));
        QFile received(dir.path()+"/received/payload.bin");QVERIFY(received.open(QIODevice::ReadOnly));QCOMPARE(received.readAll(),bytes);
        QSignalSpy listed(&fb,&FileTransfer::shareListed);
        const auto share=fa.publishShare(dir.path()+"/received");QVERIFY(!share.isEmpty());
        fb.listShare(a.id(),share);QTRY_COMPARE(listed.size(),1);
        // Shared downloads go through the same file socket preparation gate.
        QDir().mkdir(dir.path()+"/shared-copy");QSignalSpy complete(&fb,&FileTransfer::completed);
        fb.downloadShare(a.id(),share,"payload.bin",dir.path()+"/shared-copy");QTRY_COMPARE(complete.size(),1);
        fa.revokeShare(share);
        bool stopped=false;
        connect(&sb,&ScreenService::event,&sb,[&](auto k,auto){if(k=="screen_stopped")stopped=true;});
        QString offer;connect(&sb,&ScreenService::event,&sb,[&](auto k,auto d){if(k=="screen_offer")offer=d["session"].toString();});
        QSignalSpy frames(&sb,&ScreenService::frameReceived);QCOMPARE(sa.startMany({b.id()}).size(),1);QTRY_VERIFY(!offer.isEmpty());
        sb.decide(offer,true);QTest::qWait(30);
        QImage img(32,32,QImage::Format_RGB32);img.fill(Qt::blue);QByteArray jpg;QBuffer buf(&jpg);buf.open(QIODevice::WriteOnly);img.save(&buf,"JPEG");
        sa.publish(jpg);QTRY_COMPARE(frames.size(),1);
        // Inject only our policy state, never mutate OS interface settings.
        a.m_networkPolicy.interfaceName="__lanlink_removed_interface__";
        QTRY_COMPARE_WITH_TIMEOUT(a.port(),0,2500);
        QTRY_VERIFY(!sa.sending());QTRY_VERIFY(stopped);
        QVERIFY(!a.networkDiagnostics()["last_error"].toString().isEmpty());
        a.m_networkPolicy=localPolicy();
        QTcpSocket socket;QString error;
        QVERIFY(!a.prepareOutgoingSocket(&socket,QHostAddress::LocalHost,b.port(),&error));
        QVERIFY(!error.isEmpty()); // No automatic outgoing restart after interface recovery.
    }
    void activeDownloadStopsOnInterfaceLoss_data() {
        QTest::addColumn<bool>("shared");
        QTest::newRow("ordinary-file") << false;
        QTest::newRow("shared-file") << true;
    }
    void activeDownloadStopsOnInterfaceLoss() {
        QFETCH(bool,shared);
        QTemporaryDir dir;Node a("a",dir.path()+"/a"),b("b",dir.path()+"/b");
        QVERIFY(startVpn(a));QVERIFY(startVpn(b));FileTransfer fa(&a),fb(&b);
        a.serviceHandler=[&](auto s,auto r,auto h){return fa.handle(s,r,h);};
        b.serviceHandler=[&](auto s,auto r,auto h){return fb.handle(s,r,h);};
        a.addPeer("127.0.0.1",b.port());QTRY_COMPARE(a.peers().size(),1);QTRY_COMPARE(b.peers().size(),1);
        QDir().mkdir(dir.path()+"/source");QDir().mkdir(dir.path()+"/target");
        QFile file(dir.path()+"/source/interrupted.bin");QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray bytes(4*1024*1024,'i');QCOMPARE(file.write(bytes),bytes.size());file.close();
        bool stopped=false;QString transfer;
        QSignalSpy completed(&fb,&FileTransfer::completed);
        connect(&fb,&FileTransfer::progress,&fb,[&](QString id,qint64 done,qint64 total,QString status){
            if(stopped || status!="接收中" || done<=0 || done>=total)return;
            stopped=true;transfer=id;
            b.m_networkPolicy.interfaceName="__lanlink_removed_interface__";
            b.checkNetworkAvailability();
        });
        if(shared) {
            const auto share=fa.publishShare(dir.path()+"/source");QVERIFY(!share.isEmpty());
            fb.downloadShare(a.id(),share,"interrupted.bin",dir.path()+"/target");
        } else {
            fa.offerFile(b.id(),file.fileName());QTRY_COMPARE(fb.incomingOffers().size(),1);
            fb.acceptFile(fb.incomingOffers().first().toObject()["id"].toString(),dir.path()+"/target");
        }
        QTRY_VERIFY(stopped);QCOMPARE(b.port(),0);
        QTRY_COMPARE(fb.transferSnapshot(transfer)["state"].toString(),QString("paused"));
        QCOMPARE(completed.size(),0);QVERIFY(!QFileInfo::exists(dir.path()+"/target/interrupted.bin"));
        for(auto *socket:b.m_transportSockets)QCOMPARE(socket->state(),QAbstractSocket::UnconnectedState);
        b.m_networkPolicy=localPolicy();QTest::qWait(100);
        QCOMPARE(b.port(),0);QCOMPARE(completed.size(),0); // Recovering an interface does not restart transfers.
    }
    void revokedPolicyBlocksDirectFileAndScreenPaths() {
        QTemporaryDir dir;Node a("a",dir.path()+"/a"),b("b",dir.path()+"/b");
        QVERIFY(startVpn(a));QVERIFY(startVpn(b));FileTransfer fa(&a),fb(&b);ScreenService sa(&a);
        a.serviceHandler=[&](auto s,auto r,auto h){return fa.handle(s,r,h);};
        b.serviceHandler=[&](auto s,auto r,auto h){return fb.handle(s,r,h);};
        a.addPeer("127.0.0.1",b.port());QTRY_COMPARE(a.peers().size(),1);
        QFile file(dir.path()+"/blocked.bin");QVERIFY(file.open(QIODevice::WriteOnly));file.write("not transmitted");file.close();
        fa.offerFile(b.id(),file.fileName());QTRY_COMPARE(fb.incomingOffers().size(),1);
        const auto offer=fb.incomingOffers().first().toObject()["id"].toString();
        a.m_networkPolicy.allowedPeers={QHostAddress("127.0.0.2")};
        b.m_networkPolicy.allowedPeers={QHostAddress("127.0.0.2")};
        QSignalSpy fileError(&fb,&FileTransfer::error);QDir().mkdir(dir.path()+"/target");
        fb.acceptFile(offer,dir.path()+"/target");QCOMPARE(fileError.size(),1);
        QVERIFY(!QFileInfo::exists(dir.path()+"/target/blocked.bin"));
        QSignalSpy screenError(&sa,&ScreenService::event);QVERIFY(sa.startMany({b.id()}).isEmpty());
        QCOMPARE(screenError.size(),1);QVERIFY(!sa.sending());
        bool done=false;QString error;
        a.request(b.id(),{{"type","hello"}},[&](auto,auto e){error=e;done=true;});
        QVERIFY(done);QVERIFY(!error.isEmpty());
    }
    void diagnosticsReportTimeoutWithoutRetryOrIdentityClaim() {
        QTemporaryDir dir;Node node("a",dir.path());QVERIFY(startVpn(node));
        QTcpServer silent;QVERIFY(silent.listen(QHostAddress::LocalHost,0));
        int replies=0;QString error;QJsonObject report;
        node.diagnoseAddress("127.0.0.1",silent.serverPort(),[&](auto data,auto e){report=data;error=e;++replies;},50);
        QTRY_COMPARE(replies,1);QVERIFY(error.contains("timeout",Qt::CaseInsensitive));
        QVERIFY(!report["reachable"].toBool());QVERIFY(!report["identity_verified"].toBool());
        QTest::qWait(100);QCOMPARE(replies,1);QVERIFY(node.peers().isEmpty());
    }
    void queuedMessageAndFileNeverFallbackAcrossRestart() {
        QTemporaryDir dir;QString peerId, message, fileId;quint16 port=0;const int dormantCount=125;
        {
            Node a("a",dir.path()+"/a"),b("b",dir.path()+"/b");QVERIFY(startVpn(a));QVERIFY(startVpn(b));port=b.port();peerId=b.id();
            FileTransfer fa(&a),fb(&b);
            a.serviceHandler=[&](auto s,auto r,auto h){return fa.handle(s,r,h);};
            b.serviceHandler=[&](auto s,auto r,auto h){return fb.handle(s,r,h);};
            a.addPeer("127.0.0.1",port);QTRY_COMPARE(a.peers().size(),1);
            QFile source(dir.path()+"/pending.bin");QVERIFY(source.open(QIODevice::WriteOnly));source.write("pending");source.close();
            fa.offerFile(peerId,source.fileName());QTRY_COMPARE(fb.incomingOffers().size(),1);
            fileId=fb.incomingOffers().first().toObject()["id"].toString();
            b.stop();
            for(int i=0;i<dormantCount;++i) {message=a.sendMessage(peerId,QString("must stay in VPN queue %1").arg(i));QVERIFY(!message.isEmpty());}
            QTRY_COMPARE(a.pendingMessages().size(),dormantCount);
        }
        Node a("a",dir.path()+"/a"),b("b",dir.path()+"/b");
        QVERIFY(a.peers().isEmpty());QVERIFY(b.peers().isEmpty());
        QVERIFY(a.start(0,0));QVERIFY(b.start(port,0));
        FileTransfer fa(&a),fb(&b);
        a.serviceHandler=[&](auto s,auto r,auto h){return fa.handle(s,r,h);};
        b.serviceHandler=[&](auto s,auto r,auto h){return fb.handle(s,r,h);};
        // Even explicitly re-adding the same ID in LAN cannot migrate old jobs.
        a.addPeer("127.0.0.1",port);QTRY_COMPARE(a.peers().size(),1);QTRY_COMPARE(b.peers().size(),1);
        QTest::qWait(100);QCOMPARE(b.history(a.id()).size(),0);QCOMPARE(a.pendingMessages().size(),dormantCount);
        QVERIFY(a.pendingMessages().first().toObject()["error"].toString().contains("原网络模式"));
        const auto current=a.sendMessage(b.id(),"current LAN message must not starve");QVERIFY(!current.isEmpty());
        // Deterministic ordering: every dormant row precedes the active one,
        // irrespective of UUID order or earlier connection-refused callbacks.
        QSqlQuery priority(a.m_db);QVERIFY(priority.exec("UPDATE outbox SET attempts=0"));
        priority.prepare("UPDATE outbox SET attempts=1 WHERE id=?");priority.addBindValue(current);QVERIFY(priority.exec());
        QTRY_COMPARE(b.history(a.id()).size(),1);
        QTRY_COMPARE(a.deliveryStatus(current)["received"].toInt(),1);
        QCOMPARE(a.pendingMessages().size(),dormantCount);
        bool fetchDone=false,fetchOk=true;
        b.request(a.id(),{{"type","fetch"},{"id",fileId},{"offset",0}},[&](auto r,auto){fetchOk=r["ok"].toBool();fetchDone=true;});
        QTRY_VERIFY(fetchDone);QVERIFY(!fetchOk);
        QSignalSpy errors(&fb,&FileTransfer::error);QDir().mkdir(dir.path()+"/target");
        fb.acceptFile(fileId,dir.path()+"/target");QCOMPARE(errors.size(),1);
        QVERIFY(!QFileInfo::exists(dir.path()+"/target/pending.bin"));
    }
};
QTEST_GUILESS_MAIN(NetworkPolicyTest)
#include "test_networkpolicy.moc"
