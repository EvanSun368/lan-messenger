#include "bubble.h"
#include "node.h"
#include <QtTest>
#include <QTemporaryDir>
class SpikeTest:public QObject{
 Q_OBJECT
private slots:
 void genuineGeometry(){
  using namespace TelegramBubble;
  Bubble b; b.geometry={10,10,200,80};
  int backgrounds=0,corners=0;
  PaintBubbleGeneric(b,[&](QRect r){QVERIFY(b.geometry.contains(r));++backgrounds;},
   [](QRect){},[&](int,int,int,Corner){++corners;},[](QPoint){return 0;});
  QCOMPARE(corners,4);QVERIFY(backgrounds>=3);
 }
 void render(){
  QImage image(240,120,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
  QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);
  PaintTelegramBubble(p,{10,10,200,80},QColor("#d9ecff"),true);p.end();
  QCOMPARE(image.pixelColor(100,40),QColor("#d9ecff"));
  QCOMPARE(image.pixelColor(10,10).alpha(),0);
  QCOMPARE(image.pixelColor(9,50).alpha(),0);
  QVERIFY(image.save("bubble-test.png"));
 }
 void actualLanRoundTrip(){
  QTemporaryDir a,b; Node alice("Alice",a.path()),bob("Bob",b.path());
  QVERIFY(alice.start(0,0));QVERIFY(bob.start(0,0));alice.addPeer("127.0.0.1",bob.port());
  QTRY_COMPARE(alice.peers().size(),1);
  QString text=QString::fromUtf8("Telegram-source renderer / LAN backend 中文");
  QVERIFY(!alice.sendMessage(bob.id(),text).isEmpty());
  QTRY_COMPARE(bob.history(alice.id()).size(),1);
  QCOMPARE(bob.history(alice.id()).first().toObject()["text"].toString(),text);
  QTRY_COMPARE(alice.pendingMessages().size(),0);
 }
};
QTEST_MAIN(SpikeTest)
#include "test_spike.moc"
