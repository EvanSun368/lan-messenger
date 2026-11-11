#include <QtTest>
#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_contact_boxes.h"
#endif
class ContactBoxesTests : public QObject {
 Q_OBJECT
private slots:
 void createGroupValidatesAndPreservesDraft() {
#ifndef LANLINK_FULL_TELEGRAM_UI
  QSKIP("Requires original lib_ui FULL mode");
#else
  QWidget parent;parent.resize(800,700);parent.show();
  int commits=0;bool accept=false;QStringList selected;QString savedName;
  QJsonArray peers{QJsonObject{{"id","alice"},{"name","Alice"},{"unread",3}},
                   QJsonObject{{"id","bob"},{"name","Bob"}}};
  QPointer<TelegramContactBoxHost> host=telegramShowCreateGroup(&parent,peers,[&](QStringList ids,QString name){
   ++commits;selected=ids;savedName=name;return accept;
  });
  auto create=host->findChild<QWidget*>("telegramGroupCreate");QVERIFY(create);
  QTest::mouseClick(create,Qt::LeftButton);QCOMPARE(commits,0);QVERIFY(host);
  auto field=dynamic_cast<Ui::InputField*>(host->findChild<QWidget*>("telegramGroupName"));QVERIFY(field);
  field->setText("研发组");
  auto alice=host->findChild<QWidget*>("telegramGroupMember_alice");QVERIFY(alice);
  QCOMPARE(alice->property("unread").toInt(),3);QTest::mouseClick(alice,Qt::LeftButton);
  QTest::mouseClick(create,Qt::LeftButton);QCOMPARE(commits,1);QVERIFY(host && host->isVisible());
  QCOMPARE(selected,QStringList{"alice"});QCOMPARE(savedName,QString("研发组"));
  auto box=host->findChild<QWidget*>("telegramContactBox");QVERIFY(box);auto layer=box->parentWidget();
  parent.resize(400,700);QTRY_COMPARE(layer->width(),368);
  QTRY_VERIFY(parent.rect().contains(QRect(layer->mapTo(&parent,QPoint()),layer->size())));
  QCOMPARE(field->getLastText(),QString("研发组"));
  QVERIFY(parent.grab().save("/tmp/original-create-group-narrow.png"));
  parent.resize(800,700);QTRY_COMPARE(layer->width(),440);
  accept=true;QTest::mouseClick(create,Qt::LeftButton);QTRY_VERIFY(host.isNull());QCOMPARE(commits,2);
#endif
 }
 void profileUsesReturnedIdentityAndCloses() {
#ifndef LANLINK_FULL_TELEGRAM_UI
  QSKIP("Requires original lib_ui FULL mode");
#else
  QWidget parent;parent.resize(800,700);parent.show();
  QPointer<TelegramContactBoxHost> host=telegramShowContactProfile(&parent,"alice",
   {{"nickname","Alice"},{"signature","来自真实回调参数的资料"},{"organization","研发团队"}});
  QCOMPARE(host->property("peerId").toString(),QString("alice"));
  auto value=dynamic_cast<Ui::FlatLabel*>(host->findChild<QWidget*>("telegramContactProfile_nickname"));
  QVERIFY(value);QCOMPARE(value->accessibilityName(),QString("Alice"));
  auto layer=host->findChild<QWidget*>("telegramContactBox")->parentWidget();
  parent.resize(400,700);QTRY_COMPARE(layer->width(),368);
  QTRY_VERIFY(parent.rect().contains(QRect(layer->mapTo(&parent,QPoint()),layer->size())));
  QVERIFY(parent.grab().save("/tmp/original-contact-profile-narrow.png"));
  auto close=host->findChild<QWidget*>("telegramContactProfileClose");QVERIFY(close);
  QTest::mouseClick(close,Qt::LeftButton);QTRY_VERIFY(host.isNull());
#endif
 }
 void cancellationAndOwnerDestructionNeverCommit() {
#ifndef LANLINK_FULL_TELEGRAM_UI
  QSKIP("Requires original lib_ui FULL mode");
#else
  auto parent=std::make_unique<QWidget>();parent->resize(800,700);parent->show();
  int commits=0;QPointer<TelegramContactBoxHost> host=telegramShowCreateGroup(parent.get(),{},[&](auto,auto){++commits;return true;});
  auto cancel=host->findChild<QWidget*>("telegramGroupCancel");QVERIFY(cancel);
  QTest::mouseClick(cancel,Qt::LeftButton);QTRY_VERIFY(host.isNull());
  host=telegramShowCreateGroup(parent.get(),{},[&](auto,auto){++commits;return true;});
  parent.reset();QVERIFY(host.isNull());QCOMPARE(commits,0);
#endif
 }
};
QTEST_MAIN(ContactBoxesTests)
#include "test_telegram_contact_boxes.moc"
