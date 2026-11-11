// New Qt integration adapter around explicitly attributed Telegram input logic.
// This is not the complete upstream Ui::InputField class.
#pragma once
#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_full_input_adapter.h"
#else
#include "telegram_input_logic.h"
#include <QPlainTextEdit>
class TelegramInputAdapter : public QPlainTextEdit {
 Q_OBJECT
public:
 explicit TelegramInputAdapter(QWidget *parent=nullptr);
 void setSubmitSettings(TelegramInputLogic::SubmitSettings settings){m_submit=settings;}
 void setInstantReplacesEnabled(bool enabled){m_replaces=enabled;}
 bool instantReplacesEnabled()const{return m_replaces;}
 void setHeightRange(int minimum,int maximum);
signals:
 void submitted();
 void cancelled();
protected:
 void keyPressEvent(QKeyEvent*)override;
 void inputMethodEvent(QInputMethodEvent*)override;
 void resizeEvent(QResizeEvent*)override;
private:
 void updateHeight();
 void processReplace(const QString &appended);
 TelegramInputLogic::SubmitSettings m_submit=TelegramInputLogic::SubmitSettings::Enter;
 bool m_replaces=false; // Do not silently alter pasted source/logs by default.
 bool m_preedit=false,m_updatingHeight=false;
 int m_minHeight=64,m_maxHeight=180;
 bool m_autoHeight=false;
};

#endif
