#include <QtTest>
#include "deliverypresentation.h"
class DeliveryPresentationTest:public QObject {
 Q_OBJECT
private slots:
 void neverInventsReadOrMissingEvidence() {
  QCOMPARE(DeliveryPresentation::summary({}),QString("送达状态未知"));
  QCOMPARE(DeliveryPresentation::summary({{"known",true},{"total",1},{"received",1},{"rejected",0}}),QString("对方已接收"));
  auto text=DeliveryPresentation::summary({{"known",true},{"total",3},{"received",1},{"rejected",1}});
  QVERIFY(text.contains("1/3"));QVERIFY(text.contains("1 台拒收"));QVERIFY(!text.contains("已读"));
  QCOMPARE(DeliveryPresentation::targetState("queued"),QString("等待发送／确认"));
 }
};
QTEST_MAIN(DeliveryPresentationTest)
#include "test_delivery_presentation.moc"
