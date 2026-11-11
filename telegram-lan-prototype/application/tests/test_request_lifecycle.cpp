#include "node.h"
#include <QtTest>
class RequestLifecycleTest:public QObject {
 Q_OBJECT
private slots:
 void callbackMayDestroyRequestOwner() {
  QTemporaryDir a,b;Node peer("Peer",b.path());QVERIFY(peer.start(0,0));
  QPointer<Node> owner=new Node("Owner",a.path());QVERIFY(owner->start(0,0));
  bool completed=false;
  owner->requestAddress("127.0.0.1",peer.port(),{{"type","hello"}},[&](QJsonObject response,QString error){
   QVERIFY(error.isEmpty());QVERIFY(!response["id"].toString().isEmpty());completed=true;delete owner.data();
  });
  QTRY_VERIFY(completed);QVERIFY(owner.isNull());QTest::qWait(20);
 }
};
QTEST_MAIN(RequestLifecycleTest)
#include "test_request_lifecycle.moc"
