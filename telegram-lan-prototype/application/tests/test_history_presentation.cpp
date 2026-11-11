#include <QtTest>
#include <QTimeZone>
#include "telegram_history_presentation.h"
using namespace TelegramLan;
class HistoryPresentationTest:public QObject {
 Q_OBJECT
 static HistoryMessage message(QString sender,QDateTime time){HistoryMessage m;m.senderId=sender;m.timestamp=time;return m;}
private slots:
 void ordinaryChatIdentityAndBubbleGrouping() {
  auto incoming=message("alice",QDateTime::currentDateTime());auto outgoing=incoming;outgoing.outgoing=true;
  const HistoryDecoration single;
  for(const auto &m:{incoming,outgoing}) {
   const auto p=PresentHistoryMessage(m,single,false);
   QVERIFY(!p.hasFromPhoto);QVERIFY(!p.displayFromPhoto);QVERIFY(!p.displayFromName);
   QCOMPARE(p.rightAligned,m.outgoing);
  }
  const auto d=DecorateHistory({incoming,incoming,incoming});
  const auto first=PresentHistoryMessage(incoming,d[0],true);
  const auto middle=PresentHistoryMessage(incoming,d[1],true);
  const auto last=PresentHistoryMessage(incoming,d[2],true);
  QVERIFY(first.hasFromPhoto);QVERIFY(first.displayFromName);QVERIFY(!first.displayFromPhoto);
  QVERIFY(!middle.displayFromName);QVERIFY(!middle.displayFromPhoto);
  QVERIFY(!last.displayFromName);QVERIFY(last.displayFromPhoto);
  using C=TelegramBubble::Corner;
  QCOMPARE(first.rounding.topLeft,C::Large);QCOMPARE(first.rounding.bottomLeft,C::Small);
  QCOMPARE(middle.rounding.topLeft,C::Small);QCOMPARE(middle.rounding.bottomLeft,C::Small);
  QCOMPARE(last.rounding.topLeft,C::Small);QCOMPARE(last.rounding.bottomLeft,C::Tail);
  QCOMPARE(last.rounding.bottomRight,C::Large);
  const auto own=PresentHistoryMessage(outgoing,single,true);
  QVERIFY(!own.hasFromPhoto);QVERIFY(!own.displayFromName);QCOMPARE(own.rounding.bottomRight,C::Tail);
  auto media=incoming;media.mediaHidesFromName=true;media.mediaSkipsTail=true;
  const auto mediaView=PresentHistoryMessage(media,single,true);
  QVERIFY(!mediaView.displayFromName);QCOMPARE(mediaView.rounding.bottomLeft,C::Large);
  auto unwrapped=incoming;unwrapped.unwrapped=true;
  const auto mixed=DecorateHistory({incoming,unwrapped,incoming});
  QVERIFY(mixed[0].attachNext);QVERIFY(!mixed[0].bubbleAttachNext);
  QVERIFY(mixed[2].attachPrevious);QVERIFY(!mixed[2].bubbleAttachPrevious);
  QCOMPARE(PresentHistoryMessage(incoming,mixed[2],true).rounding.topLeft,C::Large);
  auto keyboard=incoming;keyboard.inlineMarkupVisible=true;
  const auto kb=PresentHistoryMessage(keyboard,single,true);
  QCOMPARE(kb.rounding.bottomLeft,C::Small);QCOMPARE(kb.rounding.bottomRight,C::Small);
 }
 void groupingTimeAndSender() {
  auto a=message("alice",QDateTime::fromSecsSinceEpoch(1762855200,QTimeZone::UTC));auto b=a;b.timestamp=a.timestamp.addSecs(899);
  QVERIFY(DecorateHistoryMessage(b,&a).attachPrevious);
  b.timestamp=a.timestamp.addSecs(900);QVERIFY(!DecorateHistoryMessage(b,&a).attachPrevious);
  b.timestamp=a.timestamp.addSecs(-30);QVERIFY(DecorateHistoryMessage(b,&a).attachPrevious); // Upstream abs() gap, not invented monotonic ordering.
  b.senderId="bob";QVERIFY(!DecorateHistoryMessage(b,&a).attachPrevious);
  b=a;b.outgoing=true;QVERIFY(!DecorateHistoryMessage(b,&a).attachPrevious);
  b=a;b.senderId.clear();a.senderId.clear();QVERIFY(!DecorateHistoryMessage(b,&a).attachPrevious);
 }
 void displayTimezoneAndDates() {
  auto a=message("a",QDateTime(QDate(2026,11,10),QTime(15,59),QTimeZone::UTC));auto b=a;b.timestamp=a.timestamp.addSecs(120);
  QVERIFY(!DecorateHistoryMessage(b,&a).showDate);QVERIFY(DecorateHistoryMessage(b,&a).attachPrevious);
  a.timestamp=a.timestamp.toTimeZone(QTimeZone::fromSecondsAheadOfUtc(8*3600));
  b.timestamp=b.timestamp.toTimeZone(QTimeZone::fromSecondsAheadOfUtc(8*3600));
  QVERIFY(DecorateHistoryMessage(b,&a).showDate);QVERIFY(!DecorateHistoryMessage(b,&a).attachPrevious);
  QVERIFY(DecorateHistoryMessage(a,nullptr).showDate);
  a.timestamp={};QVERIFY(!DecorateHistoryMessage(a,nullptr).showDate);
 }
 void boundariesAndSourceFlags() {
  const auto a=message("a",QDateTime::currentDateTime());
  for(int flag=0;flag<9;++flag){auto b=a;
   switch(flag){case 0:b.service=true;break;case 1:b.unreadBoundary=true;break;case 2:b.serviceBefore=true;break;case 3:b.threadBoundary=true;break;case 4:b.sponsored=true;break;case 5:b.hideAuthor=true;break;case 6:b.guestBot=true;break;case 7:b.threadId="other";break;case 8:b.hidden=true;break;}
   QVERIFY(!DecorateHistoryMessage(b,&a).attachPrevious);
  }
  auto previous=a;previous.inlineMarkupVisible=true;QVERIFY(!DecorateHistoryMessage(a,&previous).attachPrevious);
  previous=a;previous.hideDisplayDate=true;QVERIFY(DecorateHistoryMessage(a,&previous).showDate);
  auto current=a;current.hideDisplayDate=true;QVERIFY(!DecorateHistoryMessage(current,nullptr).showDate);
 }
 void sequenceAndBadgeGeometry() {
  auto a=message("a",QDateTime::currentDateTime());auto hidden=a;hidden.hidden=true;
  const auto d=DecorateHistory({a,hidden,a});QVERIFY(d[0].showDate);QVERIFY(d[0].attachNext);QVERIFY(d[2].attachPrevious);QVERIFY(!d[2].showDate);
  TelegramDateBadge badge("2026年11月10日");badge.resize(500,badge.height());
  QVERIFY(qAbs(badge.badgeRect().center().x()-badge.rect().center().x())<=1);
  QCOMPARE(badge.badgeRect().top(),10);QVERIFY(badge.height()>badge.badgeRect().height());
  QVERIFY(!badge.grab().isNull());
 }
};
QTEST_MAIN(HistoryPresentationTest)
#include "test_history_presentation.moc"
