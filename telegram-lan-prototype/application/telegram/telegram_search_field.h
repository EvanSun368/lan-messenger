#pragma once
#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_original_controls.h"
#include "ui/widgets/fields/input_field.h"
#include <QPointer>
#include <QResizeEvent>
#include <QTextEdit>
#include <memory>

// Actual upstream InputField and CrossButton. The host only exposes the old
// search API; no QLineEdit signals, fake slots or custom painting.
class TelegramSearchField final : public Ui::InputField {
public:
 explicit TelegramSearchField(QWidget *parent=nullptr)
 :TelegramSearchField(parent,makeStyle()) {}
 QString text() const {return getLastText();}
 void setPlaceholderText(const QString &text) {setPlaceholder(rpl::single(text));}
 void setClearButtonEnabled(bool enabled) {clearEnabled_=enabled;updateClear();}
private:
 static std::unique_ptr<style::InputField> makeStyle() {
  ensureTelegramUiRuntime();
  // Generated verbatim upstream dialogsFilter declaration and palette.
  auto s=std::make_unique<style::InputField>(st::lanDialogsFilter);
  s->heightMax=s->heightMin; // Single-line host stays one row.
  return s;
 }
 TelegramSearchField(QWidget *parent,std::unique_ptr<style::InputField> style)
 :Ui::InputField(parent,*style,Mode::SingleLine) {
  auto *owned=style.release();
  QObject::connect(this,&QObject::destroyed,[owned]{delete owned;});
  setFixedHeight(35);
  setFocusProxy(const_cast<QTextEdit*>(rawTextEdit().get()));
  auto *crossStyle=new style::CrossButton(st::defaultMultiSelectSearchCancel);
  // Original dialogsCancelSearch geometry; lib_ui supplies the real animation.
  crossStyle->width=35;crossStyle->height=35;
  crossStyle->cross.size=35;crossStyle->cross.skip=12;
  crossStyle->crossPosition=QPoint(0,0);
  cross_=new Ui::CrossButton(this,*crossStyle);
  QObject::connect(cross_,&QObject::destroyed,[crossStyle]{delete crossStyle;});
  cross_->setAccessibleName("清除搜索");
  cross_->setClickedCallback([this]{clear();});
  changes() | rpl::on_next([this]{updateClear();},lifetime());
  updateClear();
 }
 void updateClear() {cross_->toggle(clearEnabled_&&!text().isEmpty(),anim::type::instant);}
 void resizeEvent(QResizeEvent *event) override {
  Ui::InputField::resizeEvent(event);
  if(cross_)cross_->move(width()-cross_->width(),0);
 }
 Ui::CrossButton *cross_=nullptr;
 bool clearEnabled_=false;
};
template <typename Callback>
void telegramSearchChanged(TelegramSearchField *field,QObject *context,Callback callback) {
 QPointer<QObject> owner(context);
 field->changes() | rpl::on_next([field,owner,callback=std::move(callback)] {
  if(owner)callback(field->text());
 },field->lifetime());
}
#else
#include <QLineEdit>
using TelegramSearchField=QLineEdit;
template <typename Callback>
void telegramSearchChanged(TelegramSearchField *field,QObject *context,Callback callback) {
 QObject::connect(field,&QLineEdit::textChanged,context,std::move(callback));
}
#endif
