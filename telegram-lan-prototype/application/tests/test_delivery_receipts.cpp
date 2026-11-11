#include "node.h"
#include <QtTest>
#include <QSqlQuery>
class DeliveryReceiptsTest:public QObject {
 Q_OBJECT
 static int events(const QSignalSpy &spy,QString name) {
    int count=0;for(const auto &args:spy) if(args[0].toString()==name)++count;return count;
 }
 static bool sql(QString root,QString statement) {
    const QString connection=QUuid::createUuid().toString();
    bool ok;
    { auto db=QSqlDatabase::addDatabase("QSQLITE",connection);
      db.setDatabaseName(root+"/history.sqlite");
      ok=db.open() && QSqlQuery(db).exec(statement);
      db.close();
    }
    QSqlDatabase::removeDatabase(connection);return ok;
 }
private slots:
 void groupPartialReceiptAndRestart() {
    QTemporaryDir a,b,c;QString mid,gid,bid,cid;
    {
      Node alice("Alice",a.path()),bob("Bob",b.path()),carol("Carol",c.path());
      QVERIFY(alice.start(0,0));QVERIFY(bob.start(0,0));QVERIFY(carol.start(0,0));
      alice.addPeer("127.0.0.1",bob.port());alice.addPeer("127.0.0.1",carol.port());
      QTRY_COMPARE(alice.peers().size(),2);bid=bob.id();cid=carol.id();
      gid=alice.createGroup({bid,cid},"Results");carol.stop();
      mid=alice.sendMessage("group:"+gid,"partial");QVERIFY(!mid.isEmpty());
      QTRY_COMPARE(alice.deliveryStatus(mid)["received"].toInt(),1);
      auto s=alice.deliveryStatus(mid);QVERIFY(s["known"].toBool());
      QCOMPARE(s["total"].toInt(),2);QCOMPARE(s["queued"].toInt(),1);
      QCOMPARE(s["rejected"].toInt(),0);
      QCOMPARE(alice.pendingMessages().size(),1);
      bool seenBob=false,seenCarol=false;
      for(auto value:s["targets"].toArray()) {
        auto target=value.toObject();
        if(target["recipient"]==bid) {seenBob=true;QCOMPARE(target["state"].toString(),"received");QVERIFY(target["ack_at"].toDouble()>0);}
        if(target["recipient"]==cid) {seenCarol=true;QCOMPARE(target["state"].toString(),"queued");QVERIFY(target["ack_at"].isNull());}
      }
      QVERIFY(seenBob&&seenCarol);
    }
    Node restored("Alice",a.path());
    auto s=restored.deliveryStatus(mid);QVERIFY(s["known"].toBool());
    QCOMPARE(s["total"].toInt(),2);QCOMPARE(s["received"].toInt(),1);QCOMPARE(s["queued"].toInt(),1);
    QCOMPARE(restored.history("group:"+gid).size(),1);
 }
 void permanentRejectionIsNotReceipt() {
    QTemporaryDir a,b,c;Node alice("Alice",a.path()),bob("Bob",b.path()),carol("Carol",c.path());
    QVERIFY(alice.start(0,0));QVERIFY(bob.start(0,0));
    QTcpServer rejecting;
    QVERIFY(rejecting.listen(QHostAddress::LocalHost,0));
    connect(&rejecting,&QTcpServer::newConnection,this,[&]{
      auto *socket=rejecting.nextPendingConnection();
      connect(socket,&QTcpSocket::readyRead,socket,[socket]{
        socket->readAll();
        Node::writeFrame(socket,{{"error","Invalid message"}});
        socket->disconnectFromHost();
      });
      connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
    });
    auto peer=bob.hello();peer["port"]=rejecting.serverPort();
    QVERIFY(alice.remember(peer,"127.0.0.1"));
    QVERIFY(carol.start(0,0));alice.addPeer("127.0.0.1",carol.port());
    QTRY_COMPARE(alice.peers().size(),2);
    const auto group=alice.createGroup({bob.id(),carol.id()},"Mixed results");
    auto mid=alice.sendMessage("group:"+group,"rejected");QVERIFY(!mid.isEmpty());
    QTRY_COMPARE(alice.deliveryStatus(mid)["rejected"].toInt(),1);
    QTRY_COMPARE(alice.deliveryStatus(mid)["received"].toInt(),1);
    auto s=alice.deliveryStatus(mid);
    QCOMPARE(s["received"].toInt(),1);QCOMPARE(s["total"].toInt(),2);QCOMPARE(s["queued"].toInt(),0);
    QJsonObject target;
    for(auto v:s["targets"].toArray())if(v.toObject()["recipient"]==bob.id())target=v.toObject();
    QVERIFY(!target.isEmpty());
    QCOMPARE(target["error"].toString(),"Invalid message");
    QCOMPARE(target["attempts"].toInt(),1);QVERIFY(target["ack_at"].isNull());
    QCOMPARE(alice.pendingMessages().size(),1);
    // Swap the rejecting endpoint for the actual same recipient. Retry the
    // original ID without re-sending to Carol, who already persisted it.
    QVERIFY(alice.remember(bob.hello(),"127.0.0.1"));
    QVERIFY(sql(a.path(),"CREATE TRIGGER fail_retry BEFORE UPDATE ON delivery_targets WHEN NEW.state='queued' BEGIN SELECT RAISE(FAIL,'injected retry error'); END"));
    QVERIFY(!alice.retryMessage(mid));
    QCOMPARE(alice.deliveryStatus(mid)["rejected"].toInt(),1);
    QVERIFY(alice.pendingMessages().first().toObject()["error"].toString().startsWith("rejected:"));
    QVERIFY(sql(a.path(),"DROP TRIGGER fail_retry"));
    QVERIFY(alice.retryMessage(mid));
    auto retry=alice.deliveryStatus(mid);
    QCOMPARE(retry["received"].toInt(),1);QCOMPARE(retry["queued"].toInt(),1);
    QCOMPARE(retry["rejected"].toInt(),0);
    QTRY_COMPARE(alice.deliveryStatus(mid)["received"].toInt(),2);
    QCOMPARE(alice.pendingMessages().size(),0);
    QCOMPARE(alice.history("group:"+group).size(),1);
    QCOMPARE(bob.history("group:"+group).size(),1);
    QCOMPARE(carol.history("group:"+group).size(),1);
    QCOMPARE(bob.history("group:"+group).first().toObject()["id"].toString(),mid);
    QVERIFY(!alice.retryMessage(mid)); // Completed records cannot be resent.
    QVERIFY(!alice.retryMessage("invalid"));
 }
 void failedReceiptCommitRetainsRetryAndNoSuccess() {
    QTemporaryDir a,b;Node alice("Alice",a.path()),bob("Bob",b.path());
    QVERIFY(alice.start(0,0));QVERIFY(bob.start(0,0));
    alice.addPeer("127.0.0.1",bob.port());QTRY_COMPARE(alice.peers().size(),1);
    QVERIFY(sql(a.path(),"CREATE TRIGGER fail_receipt BEFORE UPDATE ON delivery_targets WHEN NEW.state='received' BEGIN SELECT RAISE(FAIL,'injected persistence error'); END"));
    QSignalSpy spy(&alice,&Node::event);
    auto mid=alice.sendMessage(bob.id(),"ack must persist");QVERIFY(!mid.isEmpty());
    QTRY_VERIFY(events(spy,"delivery_status_error")>0);
    QCOMPARE(bob.history(alice.id()).size(),1);
    QCOMPARE(events(spy,"delivered"),0);
    QCOMPARE(alice.pendingMessages().size(),1);
    QCOMPARE(alice.deliveryStatus(mid)["received"].toInt(),0);
    QCOMPARE(alice.deliveryStatus(mid)["queued"].toInt(),1);
    QVERIFY(sql(a.path(),"DROP TRIGGER fail_receipt"));
    QTRY_COMPARE_WITH_TIMEOUT(alice.deliveryStatus(mid)["received"].toInt(),1,10000);
    QCOMPARE(events(spy,"delivered"),1);QCOMPARE(alice.pendingMessages().size(),0);
    QCOMPARE(bob.history(alice.id()).size(),1);
 }
 void oldPendingAckStillUnknown() {
    QTemporaryDir a,b;Node alice("Alice",a.path()),bob("Bob",b.path());
    QVERIFY(bob.start(0,0));QVERIFY(alice.remember(bob.hello(),"127.0.0.1"));
    const auto mid=alice.sendMessage(bob.id(),"legacy queued");QVERIFY(!mid.isEmpty());
    QVERIFY(sql(a.path(),"DELETE FROM delivery_targets"));
    QVERIFY(sql(a.path(),"DELETE FROM delivery_batches"));
    QVERIFY(!alice.deliveryStatus(mid)["known"].toBool());
    QVERIFY(alice.retryMessage(mid));
    QVERIFY(!alice.deliveryStatus(mid)["known"].toBool());
    QCOMPARE(alice.history(bob.id()).size(),1);
    QVERIFY(alice.start(0,0));
    QTRY_COMPARE(alice.pendingMessages().size(),0);
    QCOMPARE(bob.history(alice.id()).size(),1);
    QVERIFY(!alice.deliveryStatus(mid)["known"].toBool());
 }
 void oldHistoryNeverInfersSuccess() {
    QTemporaryDir a;QString mid(32,'e');
    {
      Node initial("Alice",a.path());
      QVERIFY(sql(a.path(),"INSERT INTO messages(id,peer,direction,text,time) VALUES('"+mid+"','old-peer','out','old',1)"));
    }
    Node restored("Alice",a.path());
    QVERIFY(!restored.deliveryStatus(mid)["known"].toBool());
    QCOMPARE(restored.pendingMessages().size(),0);
    QCOMPARE(restored.history("old-peer").size(),1);
 }
};
QTEST_MAIN(DeliveryReceiptsTest)
#include "test_delivery_receipts.moc"
