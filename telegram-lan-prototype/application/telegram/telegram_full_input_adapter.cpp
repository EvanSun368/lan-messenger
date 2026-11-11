#include "telegram_full_input_adapter.h"
#include "ui/integration.h"
#include "base/integration.h"
#include "ui/main_queue_processor.h"
#include "ui/style/style_core.h"
#include "styles/style_widgets.h"
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QThread>
#include <QInputMethodEvent>
#include <QInputMethod>
namespace { rpl::event_stream<> widgetUpdates; }
namespace crl { rpl::producer<> on_main_update_requests() {return widgetUpdates.events();} }
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

class TelegramRuntime final : public QObject {
 BaseHost baseHost;
 UiHost uiHost;
 std::unique_ptr<Ui::MainQueueProcessor> queue;
 std::unique_ptr<Ui::Animations::Manager> animations;
public:
 TelegramRuntime() : QObject(qApp),baseHost(0,nullptr) {
  base::Integration::Set(&baseHost); Ui::Integration::Set(&uiHost);
  queue=std::make_unique<Ui::MainQueueProcessor>();
  animations=std::make_unique<Ui::Animations::Manager>();
  style::StartManager(100); Ui::Emoji::Init(); qApp->installEventFilter(this);
 }
 ~TelegramRuntime() override {if(qApp) qApp->removeEventFilter(this); Ui::Emoji::Clear();style::StopManager();}
 bool eventFilter(QObject *watched,QEvent *event) override {
  if(QThread::currentThread()==thread() && event->type()==QEvent::UpdateRequest) {
   QPointer<QObject> weak(watched); widgetUpdates.fire({}); if(!weak) return true;
  }
  return false;
 }
};
void ensureTelegramUiRuntime() {
 static QPointer<TelegramRuntime> runtime;
 if(!runtime) runtime=new TelegramRuntime;
}
static std::unique_ptr<style::InputField> composerStyle() {
 ensureTelegramUiRuntime();
 auto result=std::make_unique<style::InputField>(st::defaultInputField);
 // Scalar geometry from upstream chat_helpers.style::historyComposeField.
 // Application-specific history palette/font resources are not linked here;
 // keep lib_ui palette and the user's configured font rather than fake them.
 result->textMargins=QMargins();
 result->placeholderMargins=QMargins(2,0,2,0);
 result->placeholderScale=0.;result->placeholderShift=-50;
 result->placeholderFont=result->style.font;
 result->border=0;result->borderActive=0;
 result->heightMin=36;result->heightMax=224;result->duration=100;
 return result;
}
TelegramInputAdapter::TelegramInputAdapter(QWidget *parent)
 :TelegramInputAdapter(parent,composerStyle()) {}
TelegramInputAdapter::TelegramInputAdapter(QWidget *parent,std::unique_ptr<style::InputField> style)
 :Ui::InputField(parent,*style,Mode::MultiLine),ownedStyle(style.release()) {
 // InputField stores a const style reference; release only after its base destructor.
 QObject::connect(this,&QObject::destroyed,[style=ownedStyle]{delete style;});
 setMinHeight(36); setMaxHeight(224);
 setInstantReplacesEnabled(false);
 changes() | rpl::on_next([this]{emit textChanged();},lifetime());
 submits() | rpl::on_next([this](Qt::KeyboardModifiers){emit submitted();},lifetime());
 setFocusProxy(rawEditor()); installEventFilter(this); rawEditor()->installEventFilter(this);
}
void TelegramInputAdapter::setSubmitSettings(TelegramInputLogic::SubmitSettings value) {
 Ui::InputField::setSubmitSettings(static_cast<Ui::InputSubmitSettings>(value));
}
bool TelegramInputAdapter::eventFilter(QObject *watched,QEvent *event) {
 if(watched==rawEditor()) {
  if(event->type()==QEvent::FocusOut && preedit) cancelPreedit();
  if(event->type()==QEvent::InputMethod) preedit=!static_cast<QInputMethodEvent*>(event)->preeditString().isEmpty();
  if(preedit && event->type()==QEvent::KeyPress) {
   const auto key=static_cast<QKeyEvent*>(event)->key();
   if(key==Qt::Key_Return || key==Qt::Key_Enter) return true;
  }
 }
 // Forward events addressed directly to the compatibility widget (including
 // automation) to the real QTextEdit owned by the original InputField.
 if(watched==this && !forwarding && (event->type()==QEvent::KeyPress || event->type()==QEvent::KeyRelease || event->type()==QEvent::InputMethod)) {
  QPointer<TelegramInputAdapter> weak(this); forwarding=true;
  const auto result=QApplication::sendEvent(rawEditor(),event);
  if(weak) forwarding=false; return result;
 }
 return Ui::InputField::eventFilter(watched,event);
}

void TelegramInputAdapter::setFont(const QFont &font) {
 QWidget::setFont(font);
 const int size=QFontInfo(font).pixelSize();
 auto flags=style::FontFlags();
 if(font.bold()) flags|=style::FontFlag::Bold;
 if(font.italic()) flags|=style::FontFlag::Italic;
 ownedStyle->style.font=style::font(size,flags,font.family());
 ownedStyle->placeholderFont=ownedStyle->style.font;
 rawEditor()->setFont(font);
 rawEditor()->document()->setDefaultFont(font);
 auto cursor=rawEditor()->textCursor();
 auto all=cursor; all.select(QTextCursor::Document);
 QTextCharFormat format;format.setFont(font);all.mergeCharFormat(format);
 rawEditor()->setTextCursor(cursor);
}

void TelegramInputAdapter::cancelPreedit() {
 if(resettingPreedit) return;
 resettingPreedit=true;
 const bool resetNative=preedit || rawEditor()->hasFocus();
 preedit=false;
 QPointer<TelegramInputAdapter> weak(this);
 // Cancel the native candidate session before replacing the conversation draft.
 // Reset does not commit the old candidate text into the newly selected chat.
 if(resetNative) {
  if(auto *method=QGuiApplication::inputMethod()) method->reset();
 }
 if(!weak) return;
 // Headless/automation platforms may not deliver a native cancellation event.
 // Explicitly clear the original InputField's QTextEdit preedit as well.
 QInputMethodEvent cancelled;
 QApplication::sendEvent(rawEditor(),&cancelled);
 if(weak) resettingPreedit=false;
}
void TelegramInputAdapter::setPlainText(const QString &text) {
 QPointer<TelegramInputAdapter> weak(this);
 cancelPreedit();
 if(weak) setText(text);
}
