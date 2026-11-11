#pragma once
// Compatibility surface only. Editing, layout, IME, selection, emoji and submit
// handling are the ORIGINAL complete Ui::InputField, linked from lib_ui.
#include "ui/ui_pch.h"
#include "ui/widgets/fields/input_field.h"
#include "telegram_input_logic.h"
#include <QTextEdit>
#include "styles/style_widgets.h"
class TelegramInputAdapter final : public Ui::InputField {
 Q_OBJECT
public:
 explicit TelegramInputAdapter(QWidget *parent=nullptr);
 QString toPlainText() const {return getLastText();}
 void setPlainText(const QString &s);
 void insertPlainText(const QString &s) {rawEditor()->insertPlainText(s);}
 void clear() {setPlainText(QString());}
 QTextDocument *document() {return rawEditor()->document();}
 void setPlaceholderText(const QString &s) {setPlaceholder(rpl::single(s));}
 void setFont(const QFont &font);
 void setSubmitSettings(TelegramInputLogic::SubmitSettings settings);
 void setInstantReplacesEnabled(bool enabled) {Ui::InputField::setInstantReplacesEnabled(rpl::single(enabled));}
 void setHeightRange(int minimum,int maximum) {setMinHeight(minimum);setMaxHeight(maximum);}
 QTextEdit *rawEditor() const {return const_cast<QTextEdit*>(rawTextEdit().get());}
signals:
 void submitted();
 void textChanged();
private:
 TelegramInputAdapter(QWidget *parent,std::unique_ptr<style::InputField> style);
 style::InputField *ownedStyle=nullptr;
 void cancelPreedit();
 bool forwarding=false,preedit=false,resettingPreedit=false;
protected:
 bool eventFilter(QObject *watched,QEvent *event) override;
};
