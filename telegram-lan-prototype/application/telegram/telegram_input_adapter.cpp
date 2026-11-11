// New wrapper; source-derived editing predicates are in telegram_input_logic.cpp.
#include "telegram_input_adapter.h"
#include <QKeyEvent>
#include <QInputMethodEvent>
#include <QTextDocument>
#include <QScrollBar>
#include <cmath>
#include <algorithm>
TelegramInputAdapter::TelegramInputAdapter(QWidget *parent):QPlainTextEdit(parent){
 setTabChangesFocus(true);
 setPlaceholderText(tr("输入消息…"));
 setStyleSheet("QPlainTextEdit {border:1px solid #dce3e9;border-radius:10px;padding:8px;background:white;color:#202b36;}");
 connect(document(),&QTextDocument::contentsChanged,this,&TelegramInputAdapter::updateHeight);
 updateHeight();
}
void TelegramInputAdapter::setHeightRange(int min,int max){
 m_autoHeight=true;m_minHeight=std::max(32,min);m_maxHeight=std::max(m_minHeight,max);updateHeight();
}
void TelegramInputAdapter::updateHeight(){
 if(m_updatingHeight||!m_autoHeight)return;m_updatingHeight=true;
 // Upstream heightAutoupdated clamps document height. This wrapper substitutes
 // native QPlainTextEdit frame/padding for lib_ui style margins and event helper.
 auto wanted=int(std::ceil(document()->size().height()))*fontMetrics().lineSpacing()+20+frameWidth()*2;
 setFixedHeight(std::clamp(wanted,m_minHeight,m_maxHeight));
 setVerticalScrollBarPolicy(wanted>m_maxHeight?Qt::ScrollBarAsNeeded:Qt::ScrollBarAlwaysOff);
 m_updatingHeight=false;
}
void TelegramInputAdapter::resizeEvent(QResizeEvent *event){QPlainTextEdit::resizeEvent(event);updateHeight();}
void TelegramInputAdapter::inputMethodEvent(QInputMethodEvent *event){
 m_preedit=!event->preeditString().isEmpty();QPlainTextEdit::inputMethodEvent(event);
}
void TelegramInputAdapter::keyPressEvent(QKeyEvent *event){
 const auto enter=event->key()==Qt::Key_Return||event->key()==Qt::Key_Enter;
 if(!m_preedit&&enter&&TelegramInputLogic::ShouldSubmit(m_submit,event->modifiers())){
  event->accept();emit submitted();return;
 }
 if(!m_preedit&&event->key()==Qt::Key_Escape){event->ignore();emit cancelled();return;}
 const bool typed=event->text().size()==1
  && !(event->modifiers()&(Qt::ControlModifier|Qt::MetaModifier|Qt::AltModifier))
  && event->key()!=Qt::Key_Backspace&&event->key()!=Qt::Key_Delete&&!enter;
 QPlainTextEdit::keyPressEvent(event);
 if(typed&&!m_preedit&&m_replaces)processReplace(event->text());
}
void TelegramInputAdapter::processReplace(const QString &appended){
 auto cursor=textCursor();if(cursor.hasSelection()||cursor.position()<1)return;
 auto before=toPlainText().left(cursor.position()-1);
 // Do not change common inline code/log fragments. Full Markdown tags are not ported.
 auto line=before.mid(before.lastIndexOf('\n')+1);
 if(line.contains("://")||(line.count('`')%2))return;
 auto replacement=TelegramInputLogic::FindReplacement(TelegramInputLogic::InstantReplaces::TextOnly(),before,appended);
 if(!replacement.length)return;
 cursor.beginEditBlock();cursor.setPosition(cursor.position()-replacement.length,QTextCursor::KeepAnchor);
 cursor.insertText(replacement.text);cursor.endEditBlock();setTextCursor(cursor);
}
