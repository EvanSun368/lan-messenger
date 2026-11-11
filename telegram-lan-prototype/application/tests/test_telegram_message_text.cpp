#include <QtTest>
#include <QClipboard>
#include <QApplication>
#include <QToolButton>
#include <QFile>
#include <QTemporaryDir>
#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_input_adapter.h"
#include "telegram_message_text.h"
#include "ui/widgets/labels.h"
#endif
class TelegramMessageTextTest:public QObject {
 Q_OBJECT
private slots:
 void originalEngineAndLongText() {
#ifdef LANLINK_FULL_TELEGRAM_UI
  TelegramInputAdapter runtime;
  for(const QString &text:{QString::fromUtf8("中文😀 ").repeated(18000),QString(131072,'x'),QString::fromUtf8("A👨‍👩‍👧‍👦é\n").repeated(2200)}) {
   const auto parts=TelegramMessageText::splitForSourceEngine(text);
   QCOMPARE(parts.join(QString()),text);
   for(const auto &p:parts){QVERIFY(p.size()<=16000);QVERIFY(p.isEmpty()||!p.front().isLowSurrogate());QVERIFY(p.isEmpty()||!p.back().isHighSurrogate());}
   TelegramMessageText widget(text,QFont("DejaVu Sans",12));
   QCOMPARE(widget.toPlainText(),text);QVERIFY(widget.width()<=320);QVERIFY(widget.height()>20);
   QCOMPARE(widget.sourceLabels().size(),parts.size());
   // Real original labels expose parsed content, checking against silent 32K truncation.
   for(int i=0;i<parts.size();++i) {
    auto *source=widget.sourceLabels()[i];
    QCOMPARE(source->accessibilityName(),parts[i].trimmed());
    if(parts[i].size()>1000) QVERIFY(source->height()>500);
   }
   widget.copyAll();QCOMPARE(QApplication::clipboard()->text(),text);
   QApplication::clipboard()->clear();QTest::keyClick(&widget,Qt::Key_C,Qt::ControlModifier);
   QCOMPARE(QApplication::clipboard()->text(),text);
   QApplication::clipboard()->clear();QTest::keyClick(widget.sourceLabels().first(),Qt::Key_C,Qt::ControlModifier);
   QCOMPARE(QApplication::clipboard()->text(),text);
  }
#else
  QSKIP("Original lib_ui text renderer requires FULL build");
#endif
 }
 void longTextActuallyPaintsNearBottom() {
#ifdef LANLINK_FULL_TELEGRAM_UI
  TelegramInputAdapter runtime;
  TelegramMessageText widget(QString(16000,'x'),QFont("DejaVu Sans",12));
  auto *label=widget.sourceLabels().first();QVERIFY(label->height()>1000);
  const auto bottom=label->grab(QRect(0,label->height()-40,label->width(),40)).toImage();
  int ink=0;for(int y=0;y<bottom.height();++y)for(int x=0;x<bottom.width();++x){
   const auto c=bottom.pixelColor(x,y);if(c.alpha()>0 && c.value()<180)++ink;
  }
  QVERIFY(ink>20);
#else
  QSKIP("Original lib_ui text renderer requires FULL build");
#endif
 }
 void originalSelectionCopy() {
#ifdef LANLINK_FULL_TELEGRAM_UI
  TelegramInputAdapter runtime;
  TelegramMessageText widget("hello world",QFont("DejaVu Sans",16));widget.show();
  auto *label=widget.sourceLabels().first();label->setDoubleClickSelectsParagraph(false);QTest::qWait(30);
  QTest::mouseDClick(label,Qt::LeftButton,Qt::NoModifier,QPoint(12,label->height()/2));
  QTest::keyClick(label,Qt::Key_C,Qt::ControlModifier);
  QCOMPARE(QApplication::clipboard()->text(),QString("hello"));
#else
  QSKIP("Original lib_ui text renderer requires FULL build");
#endif
 }
 void collapsePreservesFullSourceAndState() {
#ifdef LANLINK_FULL_TELEGRAM_UI
  TelegramInputAdapter runtime;
  const auto text=QString::fromUtf8("完整中文😀 内容\n").repeated(300);
  TelegramMessageText widget(text,QFont("DejaVu Sans",12));
  const int fullHeight=widget.height();QVERIFY(fullHeight>1000);
  widget.setCollapseLongMessages(true);QVERIFY(widget.isCollapsible());QVERIFY(!widget.isExpanded());
  QVERIFY(widget.height()<fullHeight);QVERIFY(widget.height()<400);
  QCOMPARE(widget.toPlainText(),text);QVERIFY(widget.sourceLabels().first()->height()>1000);
  widget.copyAll();QCOMPARE(QApplication::clipboard()->text(),text);
  QTemporaryDir dir;const auto path=dir.filePath("full.txt");QVERIFY(widget.saveFullTextToFile(path));
  QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(QString::fromUtf8(file.readAll()),text);
  auto *button=widget.findChild<QToolButton*>("expandMessageText");QVERIFY(button);button->click();
  QVERIFY(widget.isExpanded());QVERIFY(widget.height()>=fullHeight);
  widget.setTextWidthLimit(180);QVERIFY(widget.isExpanded());QCOMPARE(widget.toPlainText(),text);
  widget.applyFont(QFont("DejaVu Sans",16));QVERIFY(widget.isExpanded());
  widget.setExpanded(false);QVERIFY(!widget.isExpanded());QVERIFY(widget.height()<500);
  widget.setTextWidthLimit(220);widget.applyFont(QFont("DejaVu Sans",14));QVERIFY(!widget.isExpanded());
  QCOMPARE(widget.toPlainText(),text);
  widget.setCollapseLongMessages(false);QVERIFY(!widget.isCollapsible());QVERIFY(widget.height()>1000);
  TelegramMessageText shortText("short",QFont("DejaVu Sans",12));shortText.setCollapseLongMessages(true);
  QVERIFY(!shortText.isCollapsible());QVERIFY(shortText.findChild<QToolButton*>("expandMessageText")->isHidden());
#else
  QSKIP("Original lib_ui text renderer requires FULL build");
#endif
 }
 void responsiveWidthPreservesContent() {
#ifdef LANLINK_FULL_TELEGRAM_UI
  TelegramInputAdapter runtime;
  const QString text=QString::fromUtf8("中文😀 ").repeated(120);
  TelegramMessageText widget(text,QFont("DejaVu Sans",12));
  const int wideHeight=widget.height();widget.setTextWidthLimit(140);
  QVERIFY(widget.width()<=140);QVERIFY(widget.height()>wideHeight);
  QCOMPARE(widget.toPlainText(),text);
  widget.applyFont(QFont("DejaVu Sans",18));QVERIFY(widget.width()<=140);
  widget.setTextWidthLimit(320);QVERIFY(widget.width()<=320);
  QCOMPARE(widget.toPlainText(),text);
#else
  QSKIP("Original lib_ui text renderer requires FULL build");
#endif
 }
 void fontAndLinksAreSafe() {
#ifdef LANLINK_FULL_TELEGRAM_UI
  TelegramInputAdapter runtime;
  TelegramMessageText widget(QString::fromUtf8("中文 😀 https://example.com\n")+QString(300,'x'),QFont("DejaVu Sans",10));
  const int before=widget.height();widget.applyFont(QFont("DejaVu Sans",20));
  QVERIFY(widget.height()>before);QCOMPARE(widget.sourceLabels().size(),1);
  QVERIFY(widget.toPlainText().contains("https://example.com"));
#else
  QSKIP("Original lib_ui text renderer requires FULL build");
#endif
 }
};
QTEST_MAIN(TelegramMessageTextTest)
#include "test_telegram_message_text.moc"
