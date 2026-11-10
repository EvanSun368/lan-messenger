#include "chatnotifications.h"
#include "emojipicker.h"
#include <QtTest>
#include <QtWidgets>
class ChatFeaturesTest:public QObject {
 Q_OBJECT
private slots:
 void unreadLifecycle(){
   QWidget window; ChatNotifications n(&window,false); QSignalSpy changes(&n,&ChatNotifications::unreadChanged);
   n.incoming("peer","Alice",false); n.incoming("peer","Alice",false); n.incoming("group:x","Team",false);
   QCOMPARE(n.unread("peer"),2); QCOMPARE(n.totalUnread(),3);
   n.markRead("peer"); QCOMPARE(n.totalUnread(),1);
   n.incoming("group:x","Team",true); QCOMPARE(n.totalUnread(),0);
   n.setEnabled(false); n.incoming("peer","Alice",false); QCOMPARE(n.totalUnread(),1);
   QVERIFY(changes.count()>=5);
 }
 void emojiCatalogAndSelection(){
   QTemporaryDir settings; QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::NativeFormat,QSettings::UserScope,settings.path()); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
   auto categories=EmojiPicker::categories(); QSet<QString> unique;
   for(auto c:categories) for(auto e:c.second) unique.insert(e);
   QVERIFY(unique.size()>250);
   EmojiPicker picker; picker.show(); QSignalSpy selected(&picker,&EmojiPicker::selected);
   auto *tabs=picker.findChild<QTabBar*>("emojiCategories"); auto *list=picker.findChild<QListWidget*>("emojiList");
   QVERIFY(tabs); QVERIFY(list); QCOMPARE(tabs->count(),6); QVERIFY(list->count()>70);
   auto e=list->item(0)->data(Qt::UserRole).toString();
   QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(list->item(0)).center());
   QCOMPARE(selected.count(),1); QCOMPARE(selected.first().first().toString(),e);
   EmojiPicker second; second.findChild<QTabBar*>("emojiCategories")->setCurrentIndex(0);
   QVERIFY(second.findChild<QListWidget*>("emojiList")->count()>0);
   QCOMPARE(second.findChild<QListWidget*>("emojiList")->item(0)->text(),e);
 }
};
QTEST_MAIN(ChatFeaturesTest)
#include "test_chatfeatures.moc"
