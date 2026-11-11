#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_dialogs_inner.h"
#endif
#include "node.h"
#include "chatnotifications.h"
#include "mainwindow.h"
#include <QtTest>
#include <QtWidgets>

class NotificationTransportTest final : public QObject {
 Q_OBJECT
private slots:
 void duplicateGroupLongMessageNotifiesOnce() {
    QTemporaryDir a,b,c;
    Node alice("Alice",a.path()),bob("Bob",b.path()),carol("Carol",c.path());
    QVERIFY(alice.start(0,0));QVERIFY(bob.start(0,0));QVERIFY(carol.start(0,0));
    alice.addPeer("127.0.0.1",bob.port());alice.addPeer("127.0.0.1",carol.port());
    QTRY_COMPARE(alice.peers().size(),2);
    QWidget window;ChatNotifications notices(&window,false);
    connect(&bob,&Node::event,&notices,[&](QString kind,QJsonObject data){
      if(kind=="message" && data["direction"]=="in")
        notices.incoming(data["peer_id"].toString(),"Group",false);
    });
    const auto gid=alice.createGroup({bob.id(),carol.id()},"Long group");
    const QString conversation="group:"+gid;
    const QString text=QString::fromUtf8("中文😀\n").repeated(10000); // 110000 UTF-8 bytes.
    QVERIFY(text.toUtf8().size()<Node::MaxTextBytes);
    const auto mid=alice.sendMessage(conversation,text);QVERIFY(!mid.isEmpty());
    QTRY_COMPARE(alice.pendingMessages().size(),0);
    QCOMPARE(bob.history(conversation).first().toObject()["text"].toString(),text);
    QCOMPARE(carol.history(conversation).first().toObject()["text"].toString(),text);
    QCOMPARE(notices.unread(conversation),1);
    QJsonObject payload{{"type","group_message"},{"id",mid},{"text",text},{"group",alice.groups().first()}};
    bool ack=false;
    alice.request(bob.id(),payload,[&](auto reply,auto error){
       QVERIFY(error.isEmpty());QCOMPARE(reply["ack"].toString(),mid);ack=true;
    });
    QTRY_VERIFY(ack);
    QCOMPARE(bob.history(conversation).size(),1);
    QCOMPARE(notices.unread(conversation),1);
    notices.markRead(conversation);QCOMPARE(notices.totalUnread(),0);
 }
 void longOfflineRestartProducesOneNotification() {
    QTemporaryDir a,b;QString bobId;
    const QString text=QString::fromUtf8("离线😀").repeated(10000);
    {
      Node alice("Alice",a.path()),bob("Bob",b.path());
      QVERIFY(alice.start(0,0));QVERIFY(bob.start(0,0));bobId=bob.id();
      alice.addPeer("127.0.0.1",bob.port());QTRY_COMPARE(alice.peers().size(),1);
      bob.stop();QVERIFY(!alice.sendMessage(bobId,text).isEmpty());
      QCOMPARE(alice.pendingMessages().size(),1);
    }
    QWidget window;ChatNotifications notices(&window,false);
    Node alice("Alice",a.path()),bob("Bob",b.path());
    connect(&bob,&Node::event,&notices,[&](QString kind,QJsonObject data){
      if(kind=="message" && data["direction"]=="in")
        notices.incoming(data["peer_id"].toString(),"Alice",false);
    });
    QVERIFY(alice.start(0,0));QVERIFY(bob.start(0,0));
    alice.addPeer("127.0.0.1",bob.port());
    QTRY_COMPARE_WITH_TIMEOUT(alice.pendingMessages().size(),0,10000);
    QCOMPARE(bob.history(alice.id()).size(),1);
    QCOMPARE(bob.history(alice.id()).first().toObject()["text"].toString(),text);
    QCOMPARE(notices.totalUnread(),1);
 }
 void disabledAlertsStillTrackConversationUnread() {
    QWidget window;ChatNotifications notices(&window,false);notices.setEnabled(false);
    notices.incoming("alice","Alice",false);notices.incoming("bob","Bob",false);
    notices.incoming("alice","Alice",true);
    QCOMPARE(notices.unread("alice"),0);QCOMPARE(notices.unread("bob"),1);
    notices.incoming("","Invalid",false);QCOMPARE(notices.totalUnread(),1);
 }
#ifdef LANLINK_FULL_TELEGRAM_UI
 void actualFullWindowVisibilityAndSwitching() {
    QTemporaryDir a,b,c;
    MainWindow window(a.path(),0,false);window.show();window.activateWindow();
    Node bob("Bob",b.path()),carol("Carol",c.path());
    QVERIFY(bob.start(0,0));QVERIFY(carol.start(0,0));
    auto *node=window.node();
    node->addPeer("127.0.0.1",bob.port());node->addPeer("127.0.0.1",carol.port());
    QTRY_COMPARE(node->peers().size(),2);
    auto *list=window.findChild<QListWidget *>("contacts");
    auto *dialogs=dynamic_cast<TelegramLan::DialogsView *>(window.findChild<QWidget *>("telegramDialogs"));
    QVERIFY(dialogs);
    QVERIFY(!list->isVisible()); // QListWidget is storage only, never the visible UI.
    auto clickRow=[&](QListWidgetItem *item){
      const auto index=list->model()->index(list->row(item),0);
      const auto rect=dialogs->rowRect(index);
      QVERIFY(!rect.isEmpty());
      QVERIFY(dialogs->viewport()->rect().intersects(rect));
      QTest::mouseClick(dialogs->innerWidget(),Qt::LeftButton,Qt::NoModifier,
        dialogs->innerWidget()->mapFrom(dialogs->viewport(),rect.center()));
    };
    auto *tabs=window.findChild<QTabBar *>("rosterTabs");
    auto *notices=window.findChild<ChatNotifications *>();
    QVERIFY(list);QVERIFY(tabs);QVERIFY(notices);
    tabs->setCurrentIndex(1);
    auto select=[&](QString id){
      for(int n=0;n<list->count();++n)if(list->item(n)->data(Qt::UserRole)==id)list->setCurrentRow(n);
      QCoreApplication::processEvents();
    };
    select(bob.id());
    window.showMinimized();QTest::qWait(30);
    QVERIFY(!bob.sendMessage(node->id(),"while minimized").isEmpty());
    QTRY_COMPARE(notices->unread(bob.id()),1);
    window.showNormal();window.activateWindow();QTest::qWait(30);
    QTRY_VERIFY(window.isActiveWindow());
    QVERIFY(!window.isMinimized());
    QVERIFY(window.findChild<QDialog *>("chatWindow")->isVisible());
    QTRY_COMPARE(notices->unread(bob.id()),0);
    // The real settings action calls editSettings() without private access or
    // synthetic overlay flags. A same-window original layer hides the chat.
    auto *menu=window.findChild<QMenu *>("mainNavigationMenu");QVERIFY(menu);
    QAction *settings=nullptr;
    for(auto *action:menu->actions())if(action->text()=="个人资料与设置…")settings=action;
    QVERIFY(settings);settings->trigger();
    QPointer<QWidget> settingsOverlay=window.findChild<QWidget *>("telegramSettingsOverlay");
    QVERIFY(settingsOverlay);QTRY_VERIFY(settingsOverlay->isVisible());
    QTRY_VERIFY(window.isActiveWindow()); // Active window alone is not reading.
    QVERIFY(!bob.sendMessage(node->id(),"while original settings covers chat").isEmpty());
    QTRY_COMPARE(notices->unread(bob.id()),1);
    // QTest mouse events alone do not activate a native window. Model the
    // foreground activation a real user click performs before pressing Cancel.
    window.activateWindow();QTRY_VERIFY(window.isActiveWindow());
    QCOMPARE(notices->unread(bob.id()),1); // Still obscured, even while active.
    auto *cancel=settingsOverlay->findChild<QWidget *>("telegramProfileCancel");QVERIFY(cancel);
    QTest::mouseClick(cancel,Qt::LeftButton);
    QTRY_VERIFY(!settingsOverlay || !settingsOverlay->isVisible());
    QTRY_VERIFY(window.findChild<QDialog *>("chatWindow")->isVisible());
    QTRY_COMPARE(notices->unread(bob.id()),0);
    // Closing a layer while another application window is active must NOT
    // claim the conversation was read or steal native focus.
    settings->trigger();
    settingsOverlay=window.findChild<QWidget *>("telegramSettingsOverlay");
    QVERIFY(settingsOverlay);QTRY_VERIFY(settingsOverlay->isVisible());
    QWidget foreground;foreground.show();foreground.activateWindow();
    QTRY_VERIFY(!window.isActiveWindow());
    QVERIFY(!bob.sendMessage(node->id(),"background overlay close stays unread").isEmpty());
    QTRY_COMPARE(notices->unread(bob.id()),1);
    cancel=settingsOverlay->findChild<QWidget *>("telegramProfileCancel");QVERIFY(cancel);
    QTest::mouseClick(cancel,Qt::LeftButton);
    QTRY_VERIFY(!settingsOverlay || !settingsOverlay->isVisible());
    QVERIFY(!window.isActiveWindow());QCOMPARE(notices->unread(bob.id()),1);
    foreground.hide();window.activateWindow();QTRY_VERIFY(window.isActiveWindow());
    QTRY_COMPARE(notices->unread(bob.id()),0);
    window.resize(400,700);QTest::qWait(30);
    auto *back=window.findChild<QWidget *>("backToConversations");QVERIFY(back);
    QTest::mouseClick(back,Qt::LeftButton);QTest::qWait(30);QVERIFY(dialogs->isVisible());
    QVERIFY(!bob.sendMessage(node->id(),"while viewing narrow list").isEmpty());
    QTRY_COMPARE(notices->unread(bob.id()),1);
    // Returning to the selected row must work with one click, not only a
    // currentItemChanged signal (which does not fire for the same row).
    auto *selected=list->currentItem();QVERIFY(selected);
    clickRow(selected);
    QTRY_VERIFY(!dialogs->isVisible());
    QTRY_COMPARE(notices->unread(bob.id()),0);
    QTest::mouseClick(back,Qt::LeftButton);QTest::qWait(20);
    QVERIFY(!bob.sendMessage(node->id(),"again in list").isEmpty());
    QTRY_COMPARE(notices->unread(bob.id()),1);
    select(carol.id()); // A different visible conversation must not clear Bob.
    QCOMPARE(notices->unread(bob.id()),1);
    QVERIFY(!bob.sendMessage(node->id(),"while viewing Carol").isEmpty());
    QTRY_COMPARE(notices->unread(bob.id()),2);
    QWidget otherWindow;
    otherWindow.show();otherWindow.activateWindow();
    QTRY_VERIFY(!window.isActiveWindow());
    select(bob.id());
    QCOMPARE(notices->unread(bob.id()),2); // Background selection is not reading.
    select(carol.id());
    QCOMPARE(notices->unread(bob.id()),2);
    otherWindow.hide();
    // Programmatic selection in a hidden list does not activate the native
    // window. Model a user returning to the foreground before clicking Bob.
    // Inactive-window incoming messages must stay unread until this point.
    QCOMPARE(notices->unread(bob.id()),2);
    QTest::mouseClick(back,Qt::LeftButton);QTest::qWait(20);QVERIFY(dialogs->isVisible());
    window.activateWindow();QTRY_VERIFY(window.isActiveWindow());
    auto *chat=window.findChild<QDialog *>("chatWindow");QVERIFY(chat);
    qInfo()<<"before switch active"<<window.isActiveWindow()<<"min"<<window.isMinimized()
           <<"chat visible"<<chat->isVisible()<<"row"<<list->currentItem()->data(Qt::UserRole)
           <<"title"<<chat->windowTitle()<<"expected before"<<carol.id();
    QVERIFY(window.isActiveWindow());QVERIFY(!window.isMinimized());QVERIFY(!chat->isVisible());
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toString(),carol.id());
    QCOMPARE(chat->windowTitle(),QString("Carol"));
    QListWidgetItem *bobItem=nullptr;
    for(int n=0;n<list->count();++n)
      if(list->item(n)->data(Qt::UserRole).toString()==bob.id()) bobItem=list->item(n);
    QVERIFY(bobItem);
    clickRow(bobItem);
    QCoreApplication::processEvents();
    qInfo()<<"after switch active"<<window.isActiveWindow()<<"min"<<window.isMinimized()
           <<"chat visible"<<chat->isVisible()<<"row"<<list->currentItem()->data(Qt::UserRole)
           <<"title"<<chat->windowTitle()<<"expected after"<<bob.id();
    QVERIFY(window.isActiveWindow());QVERIFY(!window.isMinimized());QVERIFY(chat->isVisible());
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toString(),bob.id());
    QCOMPARE(chat->windowTitle(),QString("Bob"));
    QTRY_COMPARE(notices->unread(bob.id()),0);
 }
#endif
};
QTEST_MAIN(NotificationTransportTest)
#include "test_notification_transport.moc"
