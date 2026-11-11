#include "telegram_input_adapter.h"
#include <QtTest>
#include <QApplication>
class InputLifecycleTest : public QObject {
 Q_OBJECT
private slots:
 void replacingConversationDraftCancelsPreedit() {
#ifdef LANLINK_FULL_TELEGRAM_UI
  TelegramInputAdapter editor;
  editor.setSubmitSettings(TelegramInputLogic::SubmitSettings::CtrlEnter);
  editor.setPlainText("draft A");
  QSignalSpy submitted(&editor,&TelegramInputAdapter::submitted);
  QInputMethodEvent pre(QString::fromUtf8("候选"),{});
  QApplication::sendEvent(editor.rawEditor(),&pre);
  QTest::keyClick(editor.rawEditor(),Qt::Key_Return,Qt::ControlModifier);
  QCOMPARE(submitted.size(),0);
  editor.setPlainText("draft B");
  QTest::keyClick(editor.rawEditor(),Qt::Key_Return,Qt::ControlModifier);
  QCOMPARE(submitted.size(),1);
  QCOMPARE(editor.toPlainText(),QString("draft B"));
#else
  QSKIP("Original InputField lifecycle test applies to FULL mode.");
#endif
 }
 void clearCancelsPreedit() {
#ifdef LANLINK_FULL_TELEGRAM_UI
  TelegramInputAdapter editor;
  editor.setSubmitSettings(TelegramInputLogic::SubmitSettings::Enter);
  QInputMethodEvent pre(QString::fromUtf8("候选"),{});
  QApplication::sendEvent(editor.rawEditor(),&pre);
  editor.clear(); editor.insertPlainText("new draft");
  QSignalSpy submitted(&editor,&TelegramInputAdapter::submitted);
  QTest::keyClick(editor.rawEditor(),Qt::Key_Return);
  QCOMPARE(submitted.size(),1);
  QCOMPARE(editor.toPlainText(),QString("new draft"));
#else
  QSKIP("Original InputField lifecycle test applies to FULL mode.");
#endif
 }
 void focusOutCancelsPreeditAndNextCommitWorks() {
#ifdef LANLINK_FULL_TELEGRAM_UI
  TelegramInputAdapter editor;
  editor.setSubmitSettings(TelegramInputLogic::SubmitSettings::Enter);
  editor.setPlainText("confirmed ");
  QInputMethodEvent pre(QString::fromUtf8("旧候选"),{});
  QApplication::sendEvent(editor.rawEditor(),&pre);
  QFocusEvent out(QEvent::FocusOut,Qt::MouseFocusReason);
  QApplication::sendEvent(editor.rawEditor(),&out);
  QFocusEvent in(QEvent::FocusIn,Qt::MouseFocusReason);
  QApplication::sendEvent(editor.rawEditor(),&in);
  QInputMethodEvent next(QString::fromUtf8("新"),{});
  QApplication::sendEvent(editor.rawEditor(),&next);
  QInputMethodEvent commit;commit.setCommitString(QString::fromUtf8("新"));
  QApplication::sendEvent(editor.rawEditor(),&commit);
  QSignalSpy submitted(&editor,&TelegramInputAdapter::submitted);
  QTest::keyClick(editor.rawEditor(),Qt::Key_Return);
  QCOMPARE(submitted.size(),1);
  QVERIFY(!editor.toPlainText().contains(QString::fromUtf8("旧候选")));
  QVERIFY(editor.toPlainText().contains(QString::fromUtf8("新")));
#else
  QSKIP("Original InputField lifecycle test applies to FULL mode.");
#endif
 }
};
QTEST_MAIN(InputLifecycleTest)
#include "test_telegram_input_lifecycle.moc"
