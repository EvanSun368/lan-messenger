#include "mainwindow.h"
#include "node.h"
#include "filetransfer.h"
#include "chatnotifications.h"
#include "transfercard.h"
#include <QtWidgets>
#include <QtTest>
#include <QSqlQuery>
#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_transfer_list.h"
#include "ui/widgets/labels.h"
#endif

class TransferTimelineTest:public QObject {
 Q_OBJECT
 static bool sql(QString root,QString statement) {
    const auto connection=QUuid::createUuid().toString();bool ok;
    {auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(root+"/history.sqlite");
     ok=db.open() && QSqlQuery(db).exec(statement);db.close();}
    QSqlDatabase::removeDatabase(connection);return ok;
 }
 static void select(MainWindow &window,QString peer) {
    window.findChild<QTabBar *>("rosterTabs")->setCurrentIndex(peer.startsWith("group:")?2:1);
    auto *list=window.findChild<QListWidget *>("contacts");
    for(int n=0;n<list->count();++n)if(list->item(n)->data(Qt::UserRole)==peer){list->setCurrentRow(n);break;}
    QCoreApplication::processEvents();
 }
private slots:
 void initTestCase() {QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);}
 void originalTransferListRejectsRealOfferAndRefreshesReceipt() {
#ifndef LANLINK_FULL_TELEGRAM_UI
    QSKIP("Original transfer overlay requires FULL Telegram UI");
#else
    QTemporaryDir a,b;
    MainWindow sender(a.path(),0,false),receiver(b.path(),0,false);
    sender.show();receiver.show();
    sender.node()->stop();receiver.node()->stop();
    QVERIFY(sender.node()->start(0,0));QVERIFY(receiver.node()->start(0,0));
    sender.node()->addPeer("127.0.0.1",receiver.node()->port());
    QTRY_COMPARE(sender.node()->peers().size(),1);QTRY_COMPARE(receiver.node()->peers().size(),1);
    auto *sending=sender.findChild<FileTransfer*>();auto *receiving=receiver.findChild<FileTransfer*>();
    QVERIFY(sending);QVERIFY(receiving);
    QFile source(a.filePath("actual-offer.bin"));QVERIFY(source.open(QIODevice::WriteOnly));
    source.write(QByteArray(32000,'r'));source.close();
    QSignalSpy offers(receiving,&FileTransfer::offerReceived);
    sending->offerFile(receiver.node()->id(),source.fileName());
    QTRY_COMPARE(offers.size(),1);
    const auto id=offers.first().first().toJsonObject()["id"].toString();QVERIFY(!id.isEmpty());
    QCOMPARE(receiving->transferSnapshot(id)["state"].toString(),QString("waiting"));
    auto openTransfers=[](MainWindow &window) {
      auto *menu=window.findChild<QMenu*>("mainNavigationMenu");QVERIFY(menu);
      QAction *action=nullptr;for(auto *candidate:menu->actions())if(candidate->text()=="文件传输记录")action=candidate;
      QVERIFY(action);action->trigger();
    };
    openTransfers(receiver);openTransfers(sender);QTest::qWait(30);
    auto findList=[](MainWindow &window){return dynamic_cast<TelegramLan::TransferList*>(window.findChild<QWidget*>("telegramTransferList"));};
    auto *receiverList=findList(receiver);auto *senderList=findList(sender);QVERIFY(receiverList);QVERIFY(senderList);
    auto *receiverRow=receiverList->rowWidget(id);auto *senderRow=senderList->rowWidget(id);QVERIFY(receiverRow);QVERIFY(senderRow);
    auto *reject=receiverRow->findChild<QWidget*>("originalTransfer_reject");
    QVERIFY(dynamic_cast<Ui::LinkButton*>(reject));QVERIFY(reject->isVisible());
    QCOMPARE(receiving->transferSnapshot(id)["done"].toDouble(),0.);
    QTest::mouseClick(reject,Qt::LeftButton);
    QTRY_COMPARE(receiving->transferSnapshot(id)["state"].toString(),QString("rejected"));
    QTRY_COMPARE(sending->transferSnapshot(id)["state"].toString(),QString("remote_rejected"));
    QTRY_COMPARE(receiverRow->property("transferState").toString(),QString("rejected"));
    QTRY_COMPARE(senderRow->property("transferState").toString(),QString("remote_rejected"));
    auto *senderStatus=dynamic_cast<Ui::FlatLabel*>(senderRow->findChild<QWidget*>("originalTransferStatus"));QVERIFY(senderStatus);
    QCOMPARE(senderStatus->accessibilityName(),QString("对方已拒绝"));
    QVERIFY(reject->isHidden());QVERIFY(receiverRow->findChild<QWidget*>("originalTransfer_accept")->isHidden());
    QCOMPARE(receiving->transferSnapshot(id)["done"].toDouble(),0.);
    receiver.raise();QTest::qWait(30);QVERIFY(receiver.grab().save("/tmp/original-transfer-live.png"));
    QVERIFY(sender.grab().save("/tmp/original-transfer-live-sender.png"));
#endif
 }
 void outgoingReceiptPresentationDoesNotGrantLocalOpen() {
    QTemporaryDir storage;QFile file(storage.filePath("source.bin"));
    QVERIFY(file.open(QIODevice::WriteOnly));file.write(QByteArray(10000,'x'));file.close();
    TransferCard card;card.show();QSignalSpy opened(&card,&TransferCard::openRequested);
    QJsonObject snapshot{{"id",QString(32,'a')},{"direction","out"},{"name","source.bin"},
        {"path",file.fileName()},{"total",10000},{"done",4000},{"state","remote_failed"},
        {"remote_result","failed"},{"can_retry",true},{"status","remote failure"}};
    card.setSnapshot(snapshot);
    QCOMPARE(card.findChild<QLabel *>("transferStatus")->text(),"对方接收失败");
    QCOMPARE(card.findChild<QProgressBar *>("transferProgress")->value(),400);
    QVERIFY(card.findChild<QLabel *>("transferBytes")->text().contains("4000 / 10000"));
    QCOMPARE(card.findChild<QPushButton *>("transferRetry")->text(),"重新发送");
    snapshot["state"]="remote_paused";card.setSnapshot(snapshot);
    QCOMPARE(card.findChild<QLabel *>("transferStatus")->text(),"对方已暂停");
    snapshot["state"]="remote_rejected";snapshot["remote_result"]="rejected";card.setSnapshot(snapshot);
    QCOMPARE(card.findChild<QLabel *>("transferStatus")->text(),"对方已拒绝");
    QVERIFY(card.findChild<QPushButton *>("transferRetry")->isHidden());
    snapshot["state"]="remote_completed";snapshot["remote_result"]="completed";snapshot["done"]=10000;
    card.setSnapshot(snapshot);
    QCOMPARE(card.findChild<QLabel *>("transferStatus")->text(),"对方已接收并校验");
    QVERIFY(card.findChild<QPushButton *>("transferOpen")->isHidden());
    card.findChild<QPushButton *>("transferOpen")->click();QCOMPARE(opened.size(),0);
    QVERIFY(QFile::remove(file.fileName()));card.setSnapshot(snapshot);
    QCOMPARE(card.findChild<QLabel *>("transferStatus")->text(),"对方已接收并校验");
    snapshot["state"]="sent";snapshot.remove("remote_result");snapshot["status"]="old release";
    card.setSnapshot(snapshot);
    QCOMPARE(card.findChild<QLabel *>("transferStatus")->text(),"已发送，最终结果未确认");
    QVERIFY(card.findChild<QPushButton *>("transferOpen")->isHidden());
 }
 void localTransferMappingIsAtomicAndDistinct() {
    QTemporaryDir storage;Node node("local",storage.path());
    const QString id(32,'a'),peer(32,'b');
    QJsonObject snapshot{{"id",id},{"peer_id",peer},{"direction","in"},{"name","sample.bin"},{"timestamp",1234.0}};
    QVERIFY(sql(storage.path(),"CREATE TRIGGER fail_mapping BEFORE INSERT ON message_transfers BEGIN SELECT RAISE(FAIL,'injected'); END"));
    QVERIFY(!node.recordTransferEvent(snapshot));QCOMPARE(node.history(peer).size(),0);
    QVERIFY(sql(storage.path(),"DROP TRIGGER fail_mapping"));
    QVERIFY(node.recordTransferEvent(snapshot));QVERIFY(node.recordTransferEvent(snapshot));
    auto history=node.history(peer);QCOMPARE(history.size(),1);
    QCOMPARE(history.first().toObject()["id"].toString(),"file:"+id);
    QCOMPARE(history.first().toObject()["timestamp"].toDouble(),1234.0);
    QCOMPARE(history.first().toObject()["sender_id"].toString(),peer);
    QVERIFY(!node.deliveryStatus("file:"+id)["known"].toBool());
    snapshot["peer_id"]=QString(32,'c');QVERIFY(!node.recordTransferEvent(snapshot));
 }
 void historyFailureWarnsOnceAndKeepsExistingCardLive() {
    QTemporaryDir a,b;
    MainWindow receiver(b.path(),0,false);receiver.show();
    Node sender("Sender",a.path());FileTransfer sending(&sender);
    sender.serviceHandler=[&](auto socket,auto request,auto host){return sending.handle(socket,request,host);};
    QVERIFY(sender.start(0,0));sender.addPeer("127.0.0.1",receiver.node()->port());
    QTRY_COMPARE(receiver.node()->peers().size(),1);QTRY_COMPARE(sender.peers().size(),1);
    select(receiver,sender.id());
    QFile source(a.filePath("history-error.bin"));QVERIFY(source.open(QIODevice::WriteOnly));source.write("test");source.close();
    sending.offerFile(receiver.node()->id(),source.fileName());
    QTRY_COMPARE(receiver.findChildren<TransferCard *>().size(),1);
    auto *card=receiver.findChild<TransferCard *>();const auto id=card->snapshot()["id"].toString();
    QCOMPARE(card->snapshot()["state"].toString(),"waiting");
    // Delete only this test's local row, then fail the replacement mapping
    // transaction. The live offer and card remain real FileTransfer objects.
    QVERIFY(sql(b.path(),"DELETE FROM message_transfers WHERE transfer_id='"+id+"'"));
    QVERIFY(sql(b.path(),"DELETE FROM messages WHERE id='file:"+id+"'"));
    QVERIFY(sql(b.path(),"DELETE FROM message_authors WHERE id='file:"+id+"'"));
    QVERIFY(sql(b.path(),"CREATE TRIGGER fail_live_mapping BEFORE INSERT ON message_transfers BEGIN SELECT RAISE(FAIL,'injected'); END"));
    QSignalSpy warnings(receiver.statusBar(),&QStatusBar::messageChanged);
    card->findChild<QPushButton *>("transferReject")->click();
    QTRY_COMPARE(card->snapshot()["state"].toString(),"rejected");
    QTRY_VERIFY(!card->property("historySaved").toBool());
    QCOMPARE(receiver.node()->history(sender.id()).size(),0);
    QVERIFY(receiver.statusBar()->currentMessage().contains("未能保存"));
    receiver.statusBar()->clearMessage();
    auto *receiving=receiver.findChild<FileTransfer *>();receiving->rejectFile(id);
    QTest::qWait(30);
    QVERIFY(receiver.statusBar()->currentMessage().isEmpty());
    int failures=0;for(const auto &args:warnings)if(args[0].toString().contains("未能保存"))++failures;
    QCOMPARE(failures,1);
    QCOMPARE(receiver.findChildren<TransferCard *>().size(),1);
 }
 void fileGroupTrustRulesAndPersistenceFailure() {
    QTemporaryDir storage;Node node("Receiver",storage.path());
    const QString sender(32,'a'),other(32,'b'),group(32,'c');
    QJsonObject context{{"version",1},{"id",group},{"name","Original group"},
        {"members",QJsonArray{sender,node.id(),other}}};
    QString error;
    auto bad=context;bad["version"]=2;
    QVERIFY(node.acceptFileGroupContext(sender,bad,&error).isEmpty());QVERIFY(!error.isEmpty());
    for(const auto &version:QList<QJsonValue>{QJsonValue(true),QJsonValue(QString("1")),QJsonValue(1.5),QJsonValue(QJsonValue::Null)}) {
      bad=context;bad["version"]=version;
      QVERIFY(node.acceptFileGroupContext(sender,bad,&error).isEmpty());
    }
    bad=context;bad["members"]="not an array";
    QVERIFY(node.acceptFileGroupContext(sender,bad,&error).isEmpty());
    bad=context;bad["members"]=QJsonArray{sender,node.id(),42};
    QVERIFY(node.acceptFileGroupContext(sender,bad,&error).isEmpty());
    bad=context;bad["name"]=false;
    QVERIFY(node.acceptFileGroupContext(sender,bad,&error).isEmpty());
    bad=context;bad["members"]=QJsonArray{node.id(),other};
    QVERIFY(node.acceptFileGroupContext(sender,bad,&error).isEmpty());
    bad=context;bad["members"]=QJsonArray{sender,other};
    QVERIFY(node.acceptFileGroupContext(sender,bad,&error).isEmpty());
    bad=context;bad["members"]=QJsonArray{sender,node.id(),sender};
    QVERIFY(node.acceptFileGroupContext(sender,bad,&error).isEmpty());
    QCOMPARE(node.groups().size(),0);
    QVERIFY(sql(storage.path(),"CREATE TRIGGER fail_file_group BEFORE INSERT ON groups BEGIN SELECT RAISE(FAIL,'injected'); END"));
    QVERIFY(node.acceptFileGroupContext(sender,context,&error).isEmpty());
    QVERIFY(error.contains("persistence"));QCOMPARE(node.groups().size(),0);
    QVERIFY(sql(storage.path(),"DROP TRIGGER fail_file_group"));
    QCOMPARE(node.acceptFileGroupContext(sender,context,&error),"group:"+group);
    auto changed=context;changed["name"]="Untrusted rename";
    QCOMPARE(node.acceptFileGroupContext(sender,changed,&error),"group:"+group);
    QCOMPARE(node.groups().first().toObject()["name"].toString(),"Original group");
    changed["members"]=QJsonArray{sender,node.id()};
    QVERIFY(node.acceptFileGroupContext(sender,changed,&error).isEmpty());
    QCOMPARE(node.groups().first().toObject()["members"].toArray().size(),3);
 }
 void chatCannotRenameGroupOrCommitGroupWithoutMessage() {
    QTemporaryDir a,b;
    Node alice("Alice",a.path());
    QString groupId;
    {
      Node bob("Bob",b.path());QVERIFY(alice.start(0,0));QVERIFY(bob.start(0,0));
      alice.addPeer("127.0.0.1",bob.port());QTRY_COMPARE(alice.peers().size(),1);
      groupId=alice.createGroup({bob.id()},"Canonical name");
      auto context=alice.fileGroupContext("group:"+groupId);QString error;
      QCOMPARE(bob.acceptFileGroupContext(alice.id(),context,&error),"group:"+groupId);
      context["name"]="Incoming rename";
      bool done=false;
      alice.request(bob.id(),{{"type","group_message"},{"id",QString(32,'d')},{"text","hello"},{"group",context}},
        [&](QJsonObject reply,QString failure){QVERIFY(failure.isEmpty());QVERIFY(reply.contains("ack"));done=true;});
      QTRY_VERIFY(done);
      QCOMPARE(bob.groups().first().toObject()["name"].toString(),"Canonical name");
      QCOMPARE(bob.history("group:"+groupId).size(),1);
      QVERIFY(sql(b.path(),"CREATE TRIGGER fail_group_message BEFORE INSERT ON messages BEGIN SELECT RAISE(FAIL,'injected'); END"));
      auto fresh=context;fresh["id"]=QString(32,'e');fresh["name"]="Must roll back";
      done=false;
      alice.request(bob.id(),{{"type","group_message"},{"id",QString(32,'f')},{"text","cannot persist"},{"group",fresh}},
        [&](QJsonObject reply,QString failure){QVERIFY(failure.isEmpty());QVERIFY(reply.contains("error"));done=true;});
      QTRY_VERIFY(done);
      QCOMPARE(bob.groups().size(),1);QCOMPARE(bob.history("group:"+QString(32,'e')).size(),0);
      QVERIFY(sql(b.path(),"DROP TRIGGER fail_group_message"));
    }
    Node restored("Bob",b.path());
    QCOMPARE(restored.groups().size(),1);
    QCOMPARE(restored.groups().first().toObject()["name"].toString(),"Canonical name");
 }
 void groupNotificationMayDestroyReceiverAfterPersistence() {
    QTemporaryDir a,b;
    Node alice("Alice",a.path());auto *bob=new Node("Bob",b.path());
    QVERIFY(alice.start(0,0));QVERIFY(bob->start(0,0));
    const auto bobId=bob->id();
    alice.addPeer("127.0.0.1",bob->port());QTRY_COMPARE(alice.peers().size(),1);
    const auto gid=alice.createGroup({bobId},"Lifecycle");
    QPointer<Node> guarded(bob);
    connect(bob,&Node::event,this,[&](QString kind,QJsonObject) {
        if(kind=="group") {delete bob;bob=nullptr;}
    });
    const auto mid=alice.sendMessage("group:"+gid,"persist before callback");QVERIFY(!mid.isEmpty());
    QTRY_VERIFY(guarded.isNull());
    QTRY_VERIFY(alice.deliveryStatus(mid)["targets"].toArray().first().toObject()["attempts"].toInt()>0);
    const auto state=alice.deliveryStatus(mid);
    if(state["received"].toInt()==0) {
        QCOMPARE(state["queued"].toInt(),1);QCOMPARE(alice.pendingMessages().size(),1);
        QVERIFY(!state["targets"].toArray().first().toObject()["error"].toString().isEmpty());
    } else {
        QCOMPARE(state["received"].toInt(),1);QCOMPARE(alice.pendingMessages().size(),0);
    }
    // Destroying the endpoint can drop an unflushed ACK. It must not invent
    // receipt; restart and retry the original ID, then explicitly deduplicate.
    Node restarted("Bob",b.path());QCOMPARE(restarted.history("group:"+gid).size(),1);
    QCOMPARE(restarted.groups().size(),1);QVERIFY(restarted.start(0,0));
    QVERIFY(alice.remember(restarted.hello(),"127.0.0.1"));
    QCOMPARE(alice.peers().first().toObject()["port"].toInt(),int(restarted.port()));
    if(!alice.pendingMessages().isEmpty())QVERIFY(alice.retryMessage(mid));
    QTRY_COMPARE(alice.deliveryStatus(mid)["received"].toInt(),1);
    QCOMPARE(alice.pendingMessages().size(),0);
    bool replied=false;QString error;QJsonObject result;
    alice.request(bobId,{{"type","group_message"},{"id",mid},{"text","persist before callback"},
        {"group",alice.fileGroupContext("group:"+gid)}},[&](QJsonObject response,QString failure) {
            result=response;error=failure;replied=true;
        });
    QTRY_VERIFY(replied);QVERIFY2(error.isEmpty(),qPrintable(error));
    QCOMPARE(result["ack"].toString(),mid);QCOMPARE(restarted.history("group:"+gid).size(),1);
 }
 void peerNotificationMayDestroyReceiverAfterReply() {
    QTemporaryDir a,b;Node alice("Alice",a.path());auto *bob=new Node("Bob",b.path());
    QVERIFY(alice.start(0,0));QVERIFY(bob->start(0,0));
    const auto bobId=bob->id();const auto port=bob->port();QPointer<Node> guarded(bob);
    connect(bob,&Node::event,this,[&](QString kind,QJsonObject) {
        if(kind=="peer") {delete bob;bob=nullptr;}
    });
    bool replied=false;QString error;QJsonObject result;
    alice.requestAddress("127.0.0.1",port,{{"type","hello"}},[&](QJsonObject response,QString failure) {
        result=response;error=failure;replied=true;
    });
    QTRY_VERIFY(guarded.isNull());QTRY_VERIFY(replied);
    if(error.isEmpty())QCOMPARE(result["id"].toString(),bobId);
    else QVERIFY(result.isEmpty()); // Connection closure is not successful discovery.
    QCOMPARE(alice.pendingMessages().size(),0); // hello never creates message receipts.
    Node restarted("Bob",b.path());QCOMPARE(restarted.peers().size(),1);
    QCOMPARE(restarted.peers().first().toObject()["id"].toString(),alice.id());
    QVERIFY(restarted.start(0,0));replied=false;error.clear();result={};
    alice.requestAddress("127.0.0.1",restarted.port(),{{"type","hello"}},[&](QJsonObject response,QString failure) {
        result=response;error=failure;replied=true;
    });
    QTRY_VERIFY(replied);QVERIFY2(error.isEmpty(),qPrintable(error));
    QCOMPARE(result["id"].toString(),bobId);
 }
 void localGroupNameUsesWireRules() {
    QTemporaryDir a,b;Node alice("Alice",a.path()),bob("Bob",b.path());
    QVERIFY(bob.start(0,0));QVERIFY(alice.remember(bob.hello(),"127.0.0.1"));
    for(const auto &name:QStringList{QString("bad\nname"),QString("bad\tname"),QString("bad")+QChar(0)+QString("name"),QString("bad")+QChar(127)}) {
        QVERIFY(alice.createGroup({bob.id()},name).isEmpty());
        QCOMPARE(alice.groups().size(),0);
    }
    QVERIFY(!alice.createGroup({bob.id()},"正常 Group").isEmpty());
    QCOMPARE(alice.groups().first().toObject()["name"].toString(),"正常 Group");
 }
 void threeNodeFirstFileRoutesToGroup() {
    QTemporaryDir a,b,c,destination;
    for(const auto &entry:QList<QPair<QString,QString>>{{a.path(),"Alice"},{b.path(),"Bob"},{c.path(),"Carol"}}) {
      Node seed(entry.second,entry.first);auto profile=seed.publicProfile();
      profile["nickname"]=entry.second;QVERIFY(seed.updateProfile(profile));
    }
    QString conversation,aliceId;
    {
    MainWindow alice(a.path(),0,false),bob(b.path(),0,false),carol(c.path(),0,false);
    aliceId=alice.node()->id();
    // Explicit star topology: disable UDP discovery so Bob cannot silently
    // discover Carol while we verify unknown-member delivery semantics.
    for(auto *window:{&alice,&bob,&carol}) {window->node()->stop();QVERIFY(window->node()->start(0,0));}
    alice.show();bob.show();carol.show();
    auto *aFiles=alice.findChild<FileTransfer *>();
    auto *bFiles=bob.findChild<FileTransfer *>();
    auto *cFiles=carol.findChild<FileTransfer *>();
    alice.node()->addPeer("127.0.0.1",bob.node()->port());
    alice.node()->addPeer("127.0.0.1",carol.node()->port());
    QTRY_COMPARE(alice.node()->peers().size(),2);
    QTRY_COMPARE(bob.node()->peers().size(),1);QTRY_COMPARE(carol.node()->peers().size(),1);
    const auto gid=alice.node()->createGroup({bob.node()->id(),carol.node()->id()},"First file group");
    conversation="group:"+gid;QVERIFY(!gid.isEmpty());
    QCOMPARE(bob.node()->groups().size(),0);QCOMPARE(carol.node()->groups().size(),0);
    select(alice,conversation);
    QFile source(a.filePath("group-first.bin"));QVERIFY(source.open(QIODevice::WriteOnly));
    source.write(QByteArray(32000,'g'));source.close();
    aFiles->offerFile(bob.node()->id(),source.fileName(),conversation);
    aFiles->offerFile(carol.node()->id(),source.fileName(),conversation);
    QTRY_COMPARE(bFiles->transferSnapshots().size(),1);
    QTRY_COMPARE(cFiles->transferSnapshots().size(),1);
    const auto bId=bFiles->transferSnapshots().first().toObject()["id"].toString();
    const auto cId=cFiles->transferSnapshots().first().toObject()["id"].toString();
    QTRY_COMPARE(bob.node()->history(conversation).size(),1);
    QTRY_COMPARE(carol.node()->history(conversation).size(),1);
    QCOMPARE(bob.node()->history(alice.node()->id()).size(),0);
    QCOMPARE(carol.node()->history(alice.node()->id()).size(),0);
    QCOMPARE(bFiles->transferSnapshot(bId)["conversation_id"].toString(),conversation);
    QCOMPARE(cFiles->transferSnapshot(cId)["conversation_id"].toString(),conversation);
    auto *bNotice=bob.findChild<ChatNotifications *>();auto *cNotice=carol.findChild<ChatNotifications *>();
    QTRY_COMPARE(bNotice->unread(conversation),1);QTRY_COMPARE(cNotice->unread(conversation),1);
    QCOMPARE(bNotice->unread(alice.node()->id()),0);QCOMPARE(cNotice->unread(alice.node()->id()),0);
    QCOMPARE(bob.node()->groups().first().toObject()["members"].toArray().size(),3);
    select(bob,conversation);select(carol,conversation);
    QTRY_COMPARE(bob.findChildren<TransferCard *>().size(),1);
    QTRY_COMPARE(carol.findChildren<TransferCard *>().size(),1);
    QTRY_COMPARE(alice.findChildren<TransferCard *>().size(),2);
    QCOMPARE(bob.findChild<TransferCard *>()->snapshot()["state"].toString(),"waiting");
    QCOMPARE(carol.findChild<TransferCard *>()->snapshot()["state"].toString(),"waiting");
    // Explicit test approval applies to Bob only; Carol rejects her own offer.
    bFiles->acceptFile(bId,destination.path());
    carol.findChild<TransferCard *>()->findChild<QPushButton *>("transferReject")->click();
    QTRY_COMPARE(bob.findChild<TransferCard *>()->snapshot()["state"].toString(),"completed");
    QTRY_COMPARE(carol.findChild<TransferCard *>()->snapshot()["state"].toString(),"rejected");
    QTRY_COMPARE(aFiles->transferSnapshot(bId)["state"].toString(),"remote_completed");
    QTRY_COMPARE(aFiles->transferSnapshot(cId)["state"].toString(),"remote_rejected");
    QTRY_VERIFY(carol.statusBar()->currentMessage().contains("已拒绝"));
    QVERIFY(!carol.statusBar()->currentMessage().contains("选择接收"));
    for(auto *card:alice.findChildren<TransferCard *>()) {
        const auto id=card->property("transferId").toString();
        QTRY_COMPARE(card->snapshot()["state"].toString(),id==bId?QString("remote_completed"):QString("remote_rejected"));
        QVERIFY(card->findChild<QPushButton *>("transferOpen")->isHidden());
        QCOMPARE(card->findChild<QLabel *>("transferStatus")->text(),
                 id==bId?QString("对方已接收并校验"):QString("对方已拒绝"));
    }
    QCOMPARE(FileTransfer::sha256(source.fileName()),FileTransfer::sha256(destination.filePath("group-first.bin")));
    QCOMPARE(alice.node()->history(conversation).size(),2);
    QCOMPARE(bob.node()->history(conversation).first().toObject()["sender_id"].toString(),alice.node()->id());
    alice.saveDemoScreenshot("/tmp/telegram-group-file-sender.png","chat");
    bob.saveDemoScreenshot("/tmp/telegram-group-file-bob.png","chat");
    carol.saveDemoScreenshot("/tmp/telegram-group-file-carol.png","chat");
    QCOMPARE(bob.node()->peers().size(),1); // Group membership is not peer discovery.
    const auto reply=bob.node()->sendMessage(conversation,"reply with undiscovered member");
    QVERIFY(!reply.isEmpty());
    QTRY_COMPARE(bob.node()->deliveryStatus(reply)["received"].toInt(),1);
    QCOMPARE(bob.node()->deliveryStatus(reply)["queued"].toInt(),1);
    QCOMPARE(bob.node()->deliveryStatus(reply)["total"].toInt(),2);
    QCOMPARE(carol.node()->history(conversation).size(),1); // No invented delivery.
    }
    // Carol saw only the file, never a group text. A new process model must
    // reconstruct the persisted group, route and rejected card from that alone.
    MainWindow restored(c.path(),0,false);restored.show();select(restored,conversation);
    QCOMPARE(restored.node()->groups().size(),1);
    QCOMPARE(restored.node()->groups().first().toObject()["name"].toString(),"First file group");
    QCOMPARE(restored.node()->history(conversation).size(),1);
    QCOMPARE(restored.node()->history(aliceId).size(),0);
    QTRY_COMPARE(restored.findChildren<TransferCard *>().size(),1);
    QCOMPARE(restored.findChild<TransferCard *>()->snapshot()["state"].toString(),"rejected");
 }
 void realTransferUpdatesCardsAndPersists() {
    QTemporaryDir a,b,output;
    QString receivedId,receiverPeer;
    {
      MainWindow sender(a.path(),0,false),receiver(b.path(),0,false);
      sender.show();receiver.show();
      auto *sending=sender.findChild<FileTransfer *>();auto *receiving=receiver.findChild<FileTransfer *>();
      QVERIFY(sending);QVERIFY(receiving);
      connect(receiving,&FileTransfer::offerReceived,this,[&](QJsonObject offer){
         receivedId=offer["id"].toString();
      });
      sender.node()->addPeer("127.0.0.1",receiver.node()->port());
      QTRY_COMPARE(sender.node()->peers().size(),1);QTRY_COMPARE(receiver.node()->peers().size(),1);
      receiverPeer=sender.node()->id();
      select(sender,receiver.node()->id());select(receiver,sender.node()->id());
      receiver.saveDemoScreenshot("/tmp/timeline-history-before.png","chat");
      for(int n=0;n<12;++n)QVERIFY(!sender.node()->sendMessage(receiver.node()->id(),QString("history row %1").arg(n)).isEmpty());
      QTRY_COMPARE(receiver.node()->history(sender.node()->id()).size(),12);
      auto *scroll=receiver.findChild<QScrollArea *>("messages");QVERIFY(scroll);
      auto *bar=scroll->verticalScrollBar();QTRY_VERIFY(bar->maximum()>200);
      QTest::qWait(50);bar->triggerAction(QAbstractSlider::SliderToMinimum);bar->setValue(80);
      QTest::qWait(30);const int oldPosition=bar->value();QCOMPARE(oldPosition,80);
      QFile file(a.filePath("timeline.bin"));QVERIFY(file.open(QIODevice::WriteOnly));
      const QByteArray content(600000,'x');QCOMPARE(file.write(content),qint64(content.size()));file.close();
      QSignalSpy completed(receiving,&FileTransfer::completed);
      auto *receiverChat=receiver.findChild<QDialog *>("chatWindow");QVERIFY(receiverChat);
      receiverChat->hide();QCoreApplication::processEvents();
      QVERIFY(!receiverChat->isVisible()); // Background offer cannot count as reading.
      auto *notifications=receiver.findChild<ChatNotifications *>();QVERIFY(notifications);
      // The earlier real messages may remain unread in FULL/offscreen mode.
      // Preserve that baseline rather than artificially marking them read.
      const int unreadBeforeOffer=notifications->unread(sender.node()->id());
      sending->offerFile(receiver.node()->id(),file.fileName());
      QTRY_VERIFY(!receivedId.isEmpty());
      QTRY_COMPARE(receiver.findChildren<TransferCard *>().size(),1);
      auto *waiting=receiver.findChild<TransferCard *>();
      QCOMPARE(waiting->snapshot()["state"].toString(),"waiting");
      QCOMPARE(completed.size(),0);QVERIFY(!QFileInfo::exists(output.filePath("timeline.bin")));
      QVERIFY(!receiverChat->isVisible());
      QTRY_COMPARE(notifications->unread(sender.node()->id()),unreadBeforeOffer+1);
      receiver.saveDemoScreenshot("/tmp/timeline-waiting-offer.png","chat");
      QTRY_COMPARE(bar->value(),oldPosition);
      bool choseDirectory=false;
      QTimer picker;picker.setInterval(10);
      connect(&picker,&QTimer::timeout,this,[&]{
          for(auto *widget:QApplication::topLevelWidgets())if(auto *dialog=qobject_cast<QFileDialog *>(widget)) {
              dialog->setDirectory(QFileInfo(output.path()).absolutePath());
              if(auto *edit=dialog->findChild<QLineEdit *>("fileNameEdit"))edit->setText(output.path());
              else dialog->selectFile(QFileInfo(output.path()).fileName());
              choseDirectory=true;QMetaObject::invokeMethod(dialog,"accept");picker.stop();return;
          }
      });
      picker.start();
      waiting->findChild<QPushButton *>("transferAccept")->click();
      picker.stop();QVERIFY(choseDirectory);
      QTRY_COMPARE_WITH_TIMEOUT(completed.size(),1,10000);
      QCOMPARE(QFileInfo(completed.first()[1].toString()).absolutePath(),output.path());
      QTRY_COMPARE(receiving->transferSnapshot(receivedId)["state"].toString(),"completed");
      QTRY_COMPARE(sending->transferSnapshot(receivedId)["state"].toString(),"remote_completed");
      QCOMPARE(receiver.node()->history(sender.node()->id()).size(),13);
      QCOMPARE(sender.node()->history(receiver.node()->id()).size(),13);
      QCOMPARE(sending->transferSnapshot(receivedId)["conversation_id"].toString(),receiver.node()->id());
      QCOMPARE(receiving->transferSnapshot(receivedId)["conversation_id"].toString(),sender.node()->id());
      QTRY_COMPARE(bar->value(),oldPosition); // New file must not force a jump to bottom.
      QTRY_COMPARE(receiver.findChildren<TransferCard *>().size(),1);
      QTRY_COMPARE(sender.findChildren<TransferCard *>().size(),1);
      QCOMPARE(receiver.findChild<TransferCard *>()->property("transferId").toString(),receivedId);
      QTRY_COMPARE(receiver.findChild<TransferCard *>()->snapshot()["state"].toString(),"completed");
      QTRY_COMPARE(sender.findChild<TransferCard *>()->snapshot()["state"].toString(),"remote_completed");
      QVERIFY(sender.findChild<TransferCard *>()->findChild<QPushButton *>("transferOpen")->isHidden());
      QVERIFY(receiver.findChild<TransferCard *>()->findChild<QPushButton *>("transferOpen")->isEnabled());
      bar->triggerAction(QAbstractSlider::SliderToMaximum);QTest::qWait(30);
      sender.saveDemoScreenshot("/tmp/timeline-real-sender.png","chat");
      receiver.saveDemoScreenshot("/tmp/timeline-real-receiver.png","chat");
      for(auto *status:sender.findChildren<QToolButton *>("deliveryStatus"))
          QVERIFY(!status->property("messageId").toString().startsWith("file:"));
      QCOMPARE(FileTransfer::sha256(file.fileName()),FileTransfer::sha256(output.filePath("timeline.bin")));
    }
    MainWindow restored(b.path(),0,false);restored.show();select(restored,receiverPeer);
    QTRY_COMPARE(restored.findChildren<TransferCard *>().size(),1);
    QCOMPARE(restored.node()->history(receiverPeer).size(),13);
    QTRY_COMPARE(restored.findChild<FileTransfer *>()->transferSnapshot(receivedId)["state"].toString(),"completed");
    QTRY_COMPARE(restored.findChild<TransferCard *>()->snapshot()["state"].toString(),"completed");
 }
};
QTEST_MAIN(TransferTimelineTest)
#include "test_transfer_timeline.moc"
