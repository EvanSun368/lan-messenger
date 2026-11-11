#include "transfercard.h"
#include <QtTest>
#include <QtWidgets>
class TransferCardTest : public QObject {
 Q_OBJECT
private slots:
 void actionsReflectRealState() {
  TransferCard card;QJsonObject data{{"id","ticket"},{"direction","in"},{"name","firmware.bin"},{"total",1024},{"done",512},{"state","failed"},{"status","校验失败"}};
  card.setSnapshot(data);QCOMPARE(card.findChild<QProgressBar*>("transferProgress")->value(),500);
  QVERIFY(!card.findChild<QPushButton*>("transferRetry")->isHidden());
  QVERIFY(card.findChild<QPushButton*>("transferOpen")->isHidden());
  QSignalSpy retry(&card,&TransferCard::retryRequested);card.findChild<QPushButton*>("transferRetry")->click();QCOMPARE(retry.size(),1);QCOMPARE(retry.first()[0].toString(),QString("ticket"));
  data["state"]="waiting";card.setSnapshot(data);QSignalSpy accept(&card,&TransferCard::acceptRequested);
  card.findChild<QPushButton*>("transferAccept")->click();QCOMPARE(accept.size(),1);
  data["state"]="running";card.setSnapshot(data);QSignalSpy pause(&card,&TransferCard::pauseRequested);
  card.findChild<QPushButton*>("transferPause")->click();QCOMPARE(pause.size(),1);
  data["state"]="sent";data["status"]="已发送，等待对方校验";card.setSnapshot(data);
  QVERIFY(card.findChild<QPushButton*>("transferOpen")->isHidden());
 }
 void openRequiresVerifiedStateAndExistingFile() {
  QTemporaryDir dir;QFile file(dir.filePath("file.bin"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("data");file.close();
  TransferCard card;QJsonObject data{{"id","ticket"},{"direction","in"},{"name","file.bin"},{"total",4},{"done",4},{"path",file.fileName()},{"state","paused"},{"status","完成"}};
  card.setSnapshot(data);auto *open=card.findChild<QPushButton*>("transferOpen");QVERIFY(!open->isEnabled());
  data["state"]="completed";card.setSnapshot(data);QVERIFY(open->isEnabled());
  QSignalSpy opened(&card,&TransferCard::openRequested);open->click();QCOMPARE(opened.size(),1);
  QVERIFY(QFile::remove(file.fileName()));open->click();QCOMPARE(opened.size(),1);QVERIFY(!open->isEnabled());
  QVERIFY(card.findChild<QLabel*>("transferStatus")->text().contains("丢失"));
  QCOMPARE(card.findChild<QProgressBar*>("transferProgress")->value(),1000);
 }
};
QTEST_MAIN(TransferCardTest)
#include "test_transfercard.moc"
