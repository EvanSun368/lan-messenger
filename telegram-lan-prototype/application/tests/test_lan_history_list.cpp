#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_lan_history_list.h"
#include "telegram_original_controls.h"
#include <QtTest>
#include <QLabel>
using namespace TelegramLan;
namespace {
class PaintedElement final:public HistoryElement {
public:
 PaintedElement(QString id,int height):HistoryElement(std::move(id)),h_(height){}
 void draw(QPainter &p,const QRect &)override {p.fillRect(QRect(0,0,200,h_),Qt::red);}
protected:int measure(int)override{return h_;}
private:int h_;
};
std::vector<std::unique_ptr<HistoryElement>> makeItems(int extra=0) {
 std::vector<std::unique_ptr<HistoryElement>> result;
 if(extra)result.push_back(std::make_unique<PaintedElement>("older",extra));
 for(int i=0;i<100;++i)result.push_back(std::make_unique<PaintedElement>(QString::number(i),20));
 return result;
}
}
class HistoryListTest:public QObject {
 Q_OBJECT
private slots:
 void initTestCase(){ensureTelegramUiRuntime();}
 void geometryClipAndStableAnchor(){
  LanHistoryList list;list.resize(200,1);QVERIFY(list.replace(makeItems()));
  QCOMPARE(list.contentHeight(),2000);
  QCOMPARE(list.findItemIndexByY(-1),0);QCOMPARE(list.findItemIndexByY(20),1);
  QCOMPARE(list.findItemIndexByY(99999),99);QVERIFY(!list.strictFindItemByY(-1));
  QVERIFY(!list.strictFindItemByY(2000));
  QCOMPARE(list.visibleIds(QRect(0,20,200,20)),std::vector<QString>{"1"});
  list.setVisibleRange(205,260);const auto anchor=list.captureAnchor();
  QCOMPARE(anchor.id,QString("10"));QCOMPARE(anchor.offset,5);
  QVERIFY(list.replace(makeItems(40)));QCOMPARE(list.restoreAnchor(anchor),245);
  auto duplicate=makeItems();duplicate.push_back(std::make_unique<PaintedElement>("0",20));
  QVERIFY(!list.replace(std::move(duplicate)));QCOMPARE(list.restoreAnchor(anchor),245);
  list.show();QTest::qWait(10);const auto image=list.grab(QRect(0,20,100,20)).toImage();
  QCOMPARE(image.pixelColor(50,10),QColor(Qt::red));
 }
 void actualQtRowsUseOriginalGeometry(){
  LanHistoryList list;auto *layout=new LanHistoryLayout(&list);
  layout->setContentsMargins(24,20,24,24);layout->setSpacing(0);
  auto *first=new QLabel("first");first->setFixedHeight(35);first->setProperty("historyMessageId","first");
  auto *second=new QLabel("second");second->setFixedHeight(45);second->setProperty("historyMessageId","second");
  layout->addWidget(first);layout->addWidget(second);layout->addStretch();
  list.resize(400,400);list.show();layout->setGeometry(list.rect());
  QCOMPARE(first->geometry(),QRect(24,296,352,35));
  QCOMPARE(second->geometry(),QRect(24,331,352,45));
  QCOMPARE(layout->heightForWidth(400),124);
  QCOMPARE(list.strictFindItemByY(331)->id(),QString("second"));
  list.setVisibleRange(336,380);const auto anchor=list.captureAnchor();
  auto *older=new QLabel("older");older->setFixedHeight(25);older->setProperty("historyMessageId","older");
  layout->insertWidget(0,older);layout->setGeometry(list.rect());
  QCOMPARE(list.restoreAnchor(anchor),336);
  delete layout->takeAt(0);delete older;
  // A queued RpWidget relayout may arrive before QLayout has rebuilt adapters.
  // Deleted child ownership must be guarded, not a borrowed QLayoutItem pointer.
  list.layoutElements(352,QPoint(24,20));
  layout->setGeometry(list.rect());
  QCOMPARE(list.restoreAnchor(anchor),336);
  list.resize(250,300);layout->setGeometry(list.rect());
  QCOMPARE(first->width(),202);QVERIFY(first->isVisible());QVERIFY(second->isVisible());
  QCOMPARE(second->geometry().bottom()+1,300-24);
  QCOMPARE(layout->heightForWidth(250),124); // no artificial scroll range
  // Overflow content remains top-aligned, and a new older row shifts the stable
  // anchor by exactly its height instead of retaining the short-list bottom gap.
  list.resize(250,100);layout->setGeometry(list.rect());
  QCOMPARE(first->y(),20);QCOMPARE(second->y(),55);
  list.setVisibleRange(60,100);const auto overflowAnchor=list.captureAnchor();
  auto *prepended=new QLabel("older again");prepended->setFixedHeight(30);
  prepended->setProperty("historyMessageId","older-again");layout->insertWidget(0,prepended);
  layout->setGeometry(list.rect());QCOMPARE(list.restoreAnchor(overflowAnchor),90);
  QCOMPARE(second->y(),85);
 }
 void emptyAndSingleMessageBottomPadding(){
  LanHistoryList list;auto *layout=new LanHistoryLayout(&list);
  layout->setContentsMargins(24,20,24,8);layout->setSpacing(0);layout->addStretch();
  list.resize(400,400);layout->setGeometry(list.rect());
  QCOMPARE(list.contentHeight(),0);QVERIFY(!list.strictFindItemByY(380));
  auto *row=new QLabel("one");row->setFixedHeight(40);row->setProperty("historyMessageId","one");
  layout->insertWidget(0,row);layout->setGeometry(list.rect());
  QCOMPARE(row->y(),352);QCOMPARE(row->geometry().bottom()+1,392);
  QVERIFY(!list.strictFindItemByY(351));QCOMPARE(list.strictFindItemByY(352)->id(),QString("one"));
 }
};


#else
#include <QtTest>
class HistoryListTest:public QObject {
 Q_OBJECT
private slots:
 void requiresOriginalUi(){QSKIP("Original history list requires LANLINK_FULL_TELEGRAM_UI");}
};
#endif
QTEST_MAIN(HistoryListTest)
#include "test_lan_history_list.moc"
