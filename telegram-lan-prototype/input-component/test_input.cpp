#include "telegram_input_adapter.h"
#include <QtTest>
#include <QInputMethodEvent>
class InputTest:public QObject{
 Q_OBJECT
private slots:
 void submit(){TelegramInputAdapter w;QSignalSpy spy(&w,&TelegramInputAdapter::submitted);w.show();
  QTest::keyClicks(&w,"hello");QTest::keyClick(&w,Qt::Key_Return);QCOMPARE(spy.size(),1);QCOMPARE(w.toPlainText(),QString("hello"));
  QTest::keyClick(&w,Qt::Key_Return,Qt::ShiftModifier);QCOMPARE(spy.size(),1);QVERIFY(w.toPlainText().contains('\n'));
  w.setSubmitSettings(TelegramInputLogic::SubmitSettings::CtrlEnter);QTest::keyClick(&w,Qt::Key_Return);QCOMPARE(spy.size(),1);
  QTest::keyClick(&w,Qt::Key_Return,Qt::ControlModifier);QCOMPARE(spy.size(),2);
 }
 void replacements(){TelegramInputAdapter w;w.setInstantReplacesEnabled(true);w.show();
  QTest::keyClicks(&w,"--");QCOMPARE(w.toPlainText(),QString(QChar(8212)));
  w.clear();QTest::keyClicks(&w,":shrug:");QVERIFY(w.toPlainText().contains(QChar(12484)));
  w.clear();QTest::keyClicks(&w,"`--");QCOMPARE(w.toPlainText(),QString("`--"));
  w.clear();w.setInstantReplacesEnabled(false);QTest::keyClicks(&w,"--");QCOMPARE(w.toPlainText(),QString("--"));
 }
 void ime(){TelegramInputAdapter w;QSignalSpy spy(&w,&TelegramInputAdapter::submitted);w.show();
  QInputMethodEvent pre(QString::fromUtf8("中文"),{});QApplication::sendEvent(&w,&pre);
  QTest::keyClick(&w,Qt::Key_Return);QCOMPARE(spy.size(),0);
  QInputMethodEvent commit;commit.setCommitString(QString::fromUtf8("中文"));QApplication::sendEvent(&w,&commit);
  QTest::keyClick(&w,Qt::Key_Return);QCOMPARE(spy.size(),1);
 }
 void sizing(){TelegramInputAdapter w;w.setHeightRange(64,160);w.resize(400,64);w.show();
  w.setPlainText(QString("long line\n").repeated(100));QTRY_VERIFY(w.height()<=160);QVERIFY(w.height()>=64);
  w.clear();QTRY_COMPARE(w.height(),64);
 }
};
QTEST_MAIN(InputTest)
#include "test_input.moc"
