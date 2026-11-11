#include <QtTest>
#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_original_controls.h"
#include "telegram_bubble_frame.h"
#include "styles/palette.h"
class HistoryBubbleTest:public QObject {
 Q_OBJECT
private slots:
 void initTestCase(){ensureTelegramUiRuntime();}
 void actualOriginalTailMask(){
  for(bool outgoing:{false,true})for(qreal dpr:{1.0,2.0}) {
   const auto &tail=outgoing?st::lanHistoryBubbleTailOutRight:st::lanHistoryBubbleTailInLeft;
   QImage actual(QSize(120,80)*dpr,QImage::Format_ARGB32_Premultiplied);actual.setDevicePixelRatio(dpr);actual.fill(Qt::transparent);
   const auto color=(outgoing?st::msgOutBg:st::msgInBg)->c;
   {QPainter p(&actual);p.setRenderHint(QPainter::Antialiasing);PaintTelegramBubble(p,QRect(0,0,120,80),color,outgoing);}
   QImage expected(actual.size(),actual.format());expected.setDevicePixelRatio(dpr);expected.fill(Qt::transparent);
   const int left=outgoing?120-tail.width():0;
   {QPainter p(&expected);tail.paint(p,QPoint(left,80-tail.height()),120);}
   const QRect band(qRound(left*dpr),qRound((80-tail.height())*dpr),qRound(tail.width()*dpr),qRound(tail.height()*dpr));
   QCOMPARE(actual.copy(band),expected.copy(band));
   bool ink=false;for(int y=band.top();y<=band.bottom();++y)for(int x=band.left();x<=band.right();++x)ink|=actual.pixelColor(x,y).alpha()>0;
   QVERIFY(ink);
  }
 }
 void groupedBodiesAndContentStayAligned(){
  for(bool outgoing:{false,true}) {
   TelegramBubbleFrame frame(outgoing);frame.resize(120,80);
   const auto padding=frame.layoutPadding();const int inset=frame.tailInset();QVERIFY(inset>0);
   QCOMPARE(padding.left(),11+(outgoing?0:inset));
   QCOMPARE(padding.right(),11+(outgoing?inset:0));
   auto render=[&](TelegramBubble::Rounding rounding){
    QImage image(120,80,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
    QPainter painter(&image);painter.setRenderHint(QPainter::Antialiasing);
    PaintTelegramBubble(painter,QRect(0,0,120,80),(outgoing?st::msgOutBg:st::msgInBg)->c,outgoing,rounding);
    return image;
   };
   TelegramBubble::Rounding last;
   (outgoing?last.bottomRight:last.bottomLeft)=TelegramBubble::Corner::Tail;
   auto middle=last;(outgoing?middle.bottomRight:middle.bottomLeft)=TelegramBubble::Corner::Small;
   const auto a=render(last),b=render(middle);
   // Mid-body scanline must be identical across group members, with the same
   // transparent outside column, not a six-pixel jump only on the final row.
   for(int x=0;x<120;++x)QCOMPARE(a.pixelColor(x,40),b.pixelColor(x,40));
   QCOMPARE(a.pixelColor(outgoing?119:0,40).alpha(),0);
   QVERIFY(a.pixelColor(outgoing?119-inset:inset,40).alpha()>0);
   frame.setRounding(middle);QCOMPARE(frame.layoutPadding(),padding);
   frame.setRounding(last);QCOMPARE(frame.layoutPadding(),padding);
  }
 }
 void frameRoundingActuallyChangesPaint(){
  TelegramBubbleFrame frame(false);frame.resize(120,80);
  const auto tail=frame.grab().toImage();
  TelegramBubble::Rounding r;r.topLeft=r.bottomLeft=TelegramBubble::Corner::Small;
  frame.setRounding(r);const auto attached=frame.grab().toImage();
  QVERIFY(tail!=attached);QCOMPARE(frame.rounding().bottomLeft,TelegramBubble::Corner::Small);
  r.bottomLeft=TelegramBubble::Corner::Tail;frame.setRounding(r);
  QVERIFY(frame.grab().toImage()!=attached);
 }
};
#else
class HistoryBubbleTest:public QObject {
 Q_OBJECT
private slots:void requiresOriginalUi(){QSKIP("Original tail masks require FULL UI");}
};
#endif
QTEST_MAIN(HistoryBubbleTest)
#include "test_history_bubble.moc"
