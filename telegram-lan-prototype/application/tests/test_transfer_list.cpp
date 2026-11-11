#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_transfer_list.h"
#include "ui/widgets/labels.h"
#endif
class TransferListTest:public QObject {
 Q_OBJECT
private slots:
 void actionsAreSnapshotDrivenAndRemoteCompletionCannotOpen() {
#ifndef LANLINK_FULL_TELEGRAM_UI
  QSKIP("Original transfer list requires FULL");
#else
  ensureTelegramUiRuntime();QWidget parent;parent.resize(640,600);parent.show();
  QJsonObject data{{"id","one"},{"name","firmware.bin"},{"direction","in"},{"state","waiting"},{"status","等待接收"},{"total",100},{"done",0}};
  QStringList actions;
  auto list=TelegramLan::ShowTransferBox(&parent,QJsonArray{data},[&](QString action,QString id,QString){QCOMPARE(id,QString("one"));actions<<action;});
  QVERIFY(list);QTest::qWait(30);auto *row=list->rowWidget("one");QVERIFY(row);
  auto button=[&](const QString &action){return row->findChild<QWidget*>("originalTransfer_"+action);};
  QVERIFY(dynamic_cast<Ui::LinkButton*>(button("accept")));
  QVERIFY(dynamic_cast<Ui::FlatLabel*>(row->findChild<QWidget*>("originalTransferBytes")));
  QCOMPARE(actions.size(),0);QVERIFY(button("accept")->isVisible());QVERIFY(button("reject")->isVisible());
  QTest::mouseClick(button("accept"),Qt::LeftButton);QCOMPARE(actions,QStringList{"accept"});
  data["state"]="failed";data["done"]=44;data["status"]="失败";list->upsert(data);
  QCOMPARE(dynamic_cast<Ui::FlatLabel*>(row->findChild<QWidget*>("originalTransferBytes"))->accessibilityName(),QString("44 / 100 字节"));
  QVERIFY(button("retry")->isVisible());QVERIFY(!button("accept")->isVisible());
  data["direction"]="out";data["state"]="remote_completed";data["done"]=100;list->upsert(data);
  QVERIFY(!button("open")->isVisible());QVERIFY(!button("retry")->isVisible());
  QCOMPARE(dynamic_cast<Ui::FlatLabel*>(row->findChild<QWidget*>("originalTransferStatus"))->accessibilityName(),QString("对方已接收并校验"));
  QTemporaryDir dir;QFile file(dir.filePath("received.bin"));QVERIFY(file.open(QIODevice::WriteOnly));file.write(QByteArray(100,'a'));file.close();
  data["direction"]="in";data["state"]="completed";data["path"]=file.fileName();list->upsert(data);
  QVERIFY(button("open")->isVisible());QVERIFY(button("open")->isEnabled());
  QVERIFY(file.remove());QTest::mouseClick(button("open"),Qt::LeftButton);QVERIFY(!actions.contains("open"));QVERIFY(!button("open")->isEnabled());
  auto *close=parent.findChild<QWidget*>("telegramTransferClose");QVERIFY(close);QTest::mouseClick(close,Qt::LeftButton);
  QTRY_VERIFY(list.isNull());
#endif
 }
};
QTEST_MAIN(TransferListTest)
#include "test_transfer_list.moc"
