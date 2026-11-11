#include <QtTest>
#include <QStandardItemModel>
#include <QScrollBar>
#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_dialogs_inner.h"
using namespace TelegramLan;
#endif
class DialogsInnerTest:public QObject {
 Q_OBJECT
private slots:
 void actualPointerKeyboardFilteringAndScroll() {
#ifndef LANLINK_FULL_TELEGRAM_UI
  QSKIP("Original dialog list requires FULL Telegram lib_ui");
#else
  ensureTelegramUiRuntime();QStandardItemModel model;QItemSelectionModel selection(&model);
  for(int i=0;i<60;++i){auto *item=new QStandardItem(QString("Peer %1 中文 👋").arg(i));model.appendRow(item);}
  bool evenOnly=false;
  DialogsView view(nullptr,&model,&selection,[&](const QModelIndex &i)->std::optional<DialogPresentation>{
   if(evenOnly&&i.row()%2)return {};
   DialogPresentation r;r.id=QString::number(i.row());r.name=i.data().toString();r.preview="真实原版文本预览";r.unread=i.row();return r;
  });
  view.resize(320,260);view.show();QTest::qWait(30);
  int activated=0;QModelIndex last;
  view.activated=[&](QModelIndex i){++activated;last=i;};
  auto *inner=view.innerWidget();
  QTest::mouseClick(inner,Qt::LeftButton,Qt::NoModifier,QPoint(70,80));
  QCOMPARE(activated,1);QCOMPARE(last.row(),1);QCOMPARE(view.current().row(),1);
  QTest::mousePress(inner,Qt::LeftButton,Qt::NoModifier,QPoint(70,80));
  QTest::mouseRelease(inner,Qt::LeftButton,Qt::NoModifier,QPoint(70,150));
  QCOMPARE(activated,1);
  QTest::keyClick(inner,Qt::Key_Down);QTest::keyClick(inner,Qt::Key_Return);
  QCOMPARE(activated,2);QCOMPARE(last.row(),3);
  QTest::keyClick(inner,Qt::Key_End);QTest::keyClick(inner,Qt::Key_Return);
  QCOMPARE(last.row(),59);QVERIFY(view.verticalScrollBar()->value()>0);
  QTest::keyClick(inner,Qt::Key_Home);QTest::keyClick(inner,Qt::Key_Return);
  QCOMPARE(last.row(),0);
  view.verticalScrollBar()->setValue(0);
  QWheelEvent wheel(QPointF(100,100),QPointF(view.viewport()->mapToGlobal(QPoint(100,100))),
    QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
  QApplication::sendEvent(view.viewport(),&wheel);QTest::qWait(50);
  QVERIFY(view.verticalScrollBar()->value()>0);
  view.verticalScrollBar()->setValue(0);
  evenOnly=true;view.refresh();QVERIFY(view.rowRect(model.index(1,0)).isEmpty());
  QTest::mouseClick(inner,Qt::LeftButton,Qt::NoModifier,QPoint(70,80));QCOMPARE(last.row(),2);
  const int before=activated;
  QTest::mousePress(inner,Qt::LeftButton,Qt::NoModifier,QPoint(70,80));
  model.removeRow(2);
  QTest::mouseRelease(inner,Qt::LeftButton,Qt::NoModifier,QPoint(70,80));
  QCOMPARE(activated,before);
  view.grab().save("/tmp/telegram-dialogs-inner.png");
#endif
 }
};
QTEST_MAIN(DialogsInnerTest)
#include "test_dialogs_inner.moc"
