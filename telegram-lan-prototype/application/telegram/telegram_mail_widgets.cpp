#include "telegram_mail_widgets.h"
#include "telegram_original_controls.h"
#include "ui/widgets/fields/input_field.h"
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QGraphicsOpacityEffect>
namespace {
class Mirror final : public QWidget {
public:
 explicit Mirror(QWidget *state):QWidget(state->parentWidget()),state_(state) {
  setObjectName("telegram_"+state->objectName());state->hide();state->installEventFilter(this);opacity_=new QGraphicsOpacityEffect(this);setGraphicsEffect(opacity_);
 }
 void use(QWidget *view) {view_=view;setEnabled(state_->isEnabled());opacity_->setOpacity(state_->isEnabled()?1.0:0.45);setFocusProxy(view);}
protected:
 bool eventFilter(QObject *object,QEvent *event) override {
  if(object==state_&&event->type()==QEvent::EnabledChange){setEnabled(state_->isEnabled());opacity_->setOpacity(state_->isEnabled()?1.0:0.45);}
  return QWidget::eventFilter(object,event);
 }
 void resizeEvent(QResizeEvent *event)override{
  QWidget::resizeEvent(event);if(view_)view_->setGeometry(rect());
  if(auto *field=dynamic_cast<Ui::InputField*>(view_)){field->setMinHeight(height());field->setMaxHeight(height());}
 }
private:QWidget *state_,*view_=nullptr;QGraphicsOpacityEffect *opacity_=nullptr;
};
}
QWidget *telegramMailAction(QPushButton *state,bool primary) {
 ensureTelegramUiRuntime();auto *host=new Mirror(state);
 auto *button=new Ui::RoundButton(host,rpl::single(state->text()),primary?st::defaultActiveButton:st::defaultLightButton);
 button->setObjectName("original_"+state->objectName());button->setTextTransform(Ui::RoundButtonTextTransform::NoTransform);
 button->setClickedCallback([state]{state->click();});host->use(button);
 host->setFixedHeight(34);host->setMinimumWidth(button->contentWidth()+34);host->setSizePolicy(QSizePolicy::Minimum,QSizePolicy::Fixed);
 return host;
}
QWidget *telegramMailInput(QLineEdit *state) {
 ensureTelegramUiRuntime();auto *host=new Mirror(state);
 auto *field=new Ui::InputField(host,st::defaultInputField,Ui::InputField::Mode::SingleLine,rpl::single(state->placeholderText()),state->text());
 field->setObjectName("original_"+state->objectName());field->setMaxLength(state->maxLength());field->setMarkdownReplacesEnabled(false);
 const_cast<QTextEdit*>(field->rawTextEdit().get())->setReadOnly(state->isReadOnly());
 QObject::connect(state,&QLineEdit::textChanged,field,[field](const QString &value){if(field->getLastText()!=value)field->setText(value);});
 field->changes() | rpl::on_next([field,state]{if(state->text()!=field->getLastText())state->setText(field->getLastText());},field->lifetime());
 host->use(field);host->setFixedHeight(48);return host;
}
QWidget *telegramMailText(QPlainTextEdit *state) {
 ensureTelegramUiRuntime();auto *host=new Mirror(state);
 auto *field=new Ui::InputField(host,st::defaultInputField,Ui::InputField::Mode::MultiLine,rpl::single(state->placeholderText()),state->toPlainText());
 field->setObjectName("original_"+state->objectName());field->setMaxLength(20*1024*1024);field->setMarkdownReplacesEnabled(false);
 auto *editor=const_cast<QTextEdit*>(field->rawTextEdit().get());editor->setReadOnly(state->isReadOnly());editor->setAcceptRichText(false);
 QObject::connect(state,&QPlainTextEdit::textChanged,field,[field,state]{if(field->getLastText()!=state->toPlainText())field->setText(state->toPlainText());});
 field->changes() | rpl::on_next([field,state]{if(!state->isReadOnly()&&state->toPlainText()!=field->getLastText())state->setPlainText(field->getLastText());},field->lifetime());
 host->use(field);host->setMinimumHeight(160);host->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);return host;
}
