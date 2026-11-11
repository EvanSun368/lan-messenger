// Standalone host integration for the unmodified GPL Telegram lib_ui InputField.
#include "ui/ui_pch.h"
#include "ui/integration.h"
#include "base/integration.h"
#include "ui/main_queue_processor.h"
#include "ui/style/style_core.h"
#include "ui/emoji_config.h"
#include "ui/widgets/fields/input_field.h"
#include "styles/style_widgets.h"
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QThread>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QtTest/QTest>

// Host event bridge follows upstream core/sandbox.cpp notify UpdateRequest and
// crl::on_main_update_requests contract (not a never-emitting stand-in).
namespace { rpl::event_stream<> widgetUpdates; }
namespace crl { rpl::producer<> on_main_update_requests() { return widgetUpdates.events(); } }
class ProbeApplication final : public QApplication {
public:
 using QApplication::QApplication;
 bool notify(QObject *receiver, QEvent *event) override {
  if(QThread::currentThread()==thread() && event->type()==QEvent::UpdateRequest) {
   QPointer<QObject> weak(receiver); widgetUpdates.fire({}); if(!weak) return true;
  }
  return QApplication::notify(receiver,event);
 }
};
class BaseHost final : public base::Integration {
public:
 BaseHost(int argc,char **argv): Integration(argc,argv) {}
 void enterFromEventLoop(FnMut<void()> &&f) override { f(); }
 bool logSkipDebug() override { return false; }
 void logMessageDebug(const QString &s) override { qDebug().noquote()<<s; }
 void logMessage(const QString &s) override { qInfo().noquote()<<s; }
};
class UiHost final : public Ui::Integration {
 int touches=0;
 QString cache(const QString &name) { auto path=QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/"+name; QDir().mkpath(path); return path; }
public:
 void postponeCall(FnMut<void()> &&f) override { QTimer::singleShot(0,qApp,[f=std::move(f)]() mutable {f();}); }
 // Upstream Application's leave-subscription workaround is macOS-only.
 // This probe is Linux-only, matching upstream's no-op on this platform.
 void registerLeaveSubscription(not_null<QWidget*>) override {}
 void unregisterLeaveSubscription(not_null<QWidget*>) override {}
 QString emojiCacheFolder() override {return cache("emoji");}
 QString fontsCacheFolder() override {return cache("fonts");}
 QString openglCheckFilePath() override {return cache("graphics")+"/opengl";}
 QString angleBackendFilePath() override {return cache("graphics")+"/angle";}
 void touchCounterIncrement() override {++touches;}
 int touchCounterNow() override {return touches;}
};
int main(int argc,char **argv) {
 ProbeApplication app(argc,argv);
 app.setApplicationName("TelegramInputFieldProbe");
 BaseHost baseHost(argc,argv); base::Integration::Set(&baseHost);
 UiHost uiHost; Ui::Integration::Set(&uiHost);
 Ui::MainQueueProcessor queue;
 Ui::Animations::Manager animations;
 style::StartManager(100); Ui::Emoji::Init();
 int result=0;
 {
 QWidget window; window.setWindowTitle("Original Telegram Ui::InputField probe");
 auto layout=new QVBoxLayout(&window);
 layout->addWidget(new QLabel("Original upstream InputField: Enter submits, Shift+Enter inserts newline."));
 auto field=new Ui::InputField(&window,st::defaultInputField,Ui::InputField::Mode::MultiLine,rpl::single(QString("Type a message")));
 field->setMinimumHeight(140); layout->addWidget(field);
 auto label=new QLabel("No submissions yet"); layout->addWidget(label);
 int submits=0;
 field->submits() | rpl::on_next([&](Qt::KeyboardModifiers){++submits; label->setText(QString("Submitted %1: %2").arg(submits).arg(field->getLastText()));},field->lifetime());
 window.resize(700,300); window.show(); field->setFocus();
 if(app.arguments().contains("--self-test")) QTimer::singleShot(100,[&]{
  field->setText(QString::fromUtf8("原版输入组件 测试"));
  QTest::keyClick(const_cast<QTextEdit*>(field->rawTextEdit().get()),Qt::Key_End);
  QTest::keyClick(const_cast<QTextEdit*>(field->rawTextEdit().get()),Qt::Key_Return,Qt::ShiftModifier);
  const bool newline=field->getLastText().contains('\n') && submits==0;
  QTest::keyClick(const_cast<QTextEdit*>(field->rawTextEdit().get()),Qt::Key_Return);
  const bool sent=submits==1;
  field->setText(QString(20000,'x'));
  const bool longText=field->getLastText().size()==20000;
  field->setText(QString::fromUtf8("Telegram 原版输入组件已运行\n支持 Enter 发送、Shift+Enter 换行和长文本 😀"));
  const bool emoji=field->getLastText().endsWith(QString::fromUtf8("😀"));
  const bool png=window.grab().save("input-field-probe.png");
  qInfo()<<"ORIGINAL_INPUT_FIELD_TEST"<<"newline"<<newline<<"submit"<<sent<<"longText"<<longText<<"emoji"<<emoji<<"screenshot"<<png;
  result=(newline&&sent&&longText&&emoji&&png)?0:1; app.exit(result);
 });
 result=app.exec();
 }
 Ui::Emoji::Clear(); style::StopManager(); return result;
}
