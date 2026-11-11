#include "conversation_list.h"
#include "telegram_row_painter.h"
#include <QtTest>
class DialogsTest:public QObject {
 Q_OBJECT
private slots:
 void dateRotation(){TelegramLan::DialogsModel m;
  m.upsert({"a","A","","",100});m.upsert({"b","B","","",300});m.upsert({"c","C","","",200});
  QCOMPARE(m.rows()[0]->id,QString("b"));QCOMPARE(m.rows()[1]->id,QString("c"));
  m.upsert({"a","A","","",400});QCOMPARE(m.rows()[0]->id,QString("a"));
  m.upsert({"a","A","","",50});QCOMPARE(m.rows()[2]->id,QString("a"));
  for(int i=0;i<3;++i){QCOMPARE(m.rows()[i]->index,i);QCOMPARE(m.rows()[i]->top,i*76);}
 }
 void nameRotation(){TelegramLan::DialogsModel m(TelegramLan::DialogsModel::SortMode::Name);
  m.upsert({"a","Zulu"});m.upsert({"b","alpha"});m.upsert({"c","Middle"});QCOMPARE(m.rows()[0]->id,QString("b"));
  m.upsert({"a","Aardvark"});QCOMPARE(m.rows()[0]->id,QString("a"));
  m.upsert({"a","zzzz"});QCOMPARE(m.rows()[2]->id,QString("a"));
 }
 void hitTestingAndHeight(){TelegramLan::DialogsModel m;
  auto a=TelegramLan::DialogRow{"a","A","","",300};a.height=40;m.upsert(a);
  auto b=TelegramLan::DialogRow{"b","B","","",200};b.height=90;m.upsert(b);
  QVERIFY(m.rowAtY(-1)==nullptr);QCOMPARE(m.rowAtY(39)->id,QString("a"));QCOMPARE(m.rowAtY(40)->id,QString("b"));QVERIFY(m.rowAtY(130)==nullptr);
  a.height=60;m.upsert(a);QCOMPARE(m.find("b")->top,60);m.remove("a");QCOMPARE(m.find("b")->top,0);
 }
 void unicodeAndCounters(){using namespace TelegramLan;QCOMPARE(FormatUnreadCounter(0,false),QString());QCOMPARE(FormatUnreadCounter(1000,true,true),QString("99+"));QCOMPARE(FormatUnreadCounterShort(1200),QString("1K"));QCOMPARE(FormatUnreadCounterShort(2000000),QString("2M"));}
 void originalRowPainterLayout(){
 using namespace TelegramLan;
 QImage image(360,TelegramDialogRowHeight,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter painter(&image);
 DialogPresentation row;row.name="<b>Plain name</b>";row.preview="Last message";row.date="11:28";row.unread=12;
 auto layout=PaintTelegramDialogRow(painter,row,{360,false,false,false});painter.end();
 QCOMPARE(layout.avatar,QRect(10,8,46,46));QCOMPARE(layout.name.left(),68);QCOMPARE(layout.name.top(),10);QCOMPARE(layout.preview.top(),34);
 QVERIFY(layout.name.right()<layout.date.left());QVERIFY(layout.preview.right()<layout.badge.left());
 QCOMPARE(image.pixelColor(359,0),QColor(Qt::white));
 QImage active(360,TelegramDialogRowHeight,QImage::Format_ARGB32_Premultiplied);active.fill(Qt::transparent);QPainter a(&active);PaintTelegramDialogRow(a,row,{360,true,false,false});a.end();QCOMPARE(active.pixelColor(359,0),QColor("#419fd9"));
 QVERIFY(image.save("telegram-row-painter.png"));
 }
 void narrowRenderer(){
 using namespace TelegramLan;QImage image(70,TelegramDialogRowHeight,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter p(&image);DialogPresentation row;row.name="A";row.unread=1000;
 auto r=PaintTelegramDialogRow(p,row,{70,false,false,true});p.end();QVERIFY(r.name.isNull());QVERIFY(!r.badge.isNull());QVERIFY(image.save("telegram-row-narrow.png"));
 }
 void widgetSelection(){TelegramLan::ConversationList list;list.resize(320,400);list.upsert({"a","Alice","hello","10:01",2});list.upsert({"b","Bob","hi","10:00",1});list.show();QTest::qWait(30);
 QSignalSpy spy(&list,&TelegramLan::ConversationList::conversationActivated);QTest::mouseClick(list.viewport(),Qt::LeftButton,Qt::NoModifier,{80,30});QCOMPARE(list.selected(),QString("a"));QCOMPARE(spy.size(),1);
 list.setFilter("Bob");QTest::mouseClick(list.viewport(),Qt::LeftButton,Qt::NoModifier,{80,30});QCOMPARE(list.selected(),QString("b"));
 }
};
QTEST_MAIN(DialogsTest)
#include "test_dialogs.moc"
