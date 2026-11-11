#pragma once
#include "telegram_dialogs_model.h"
#include <QAbstractScrollArea>
namespace TelegramLan {
// New Qt adapter around Telegram-derived model/formatting algorithms.
class ConversationList final:public QAbstractScrollArea {
 Q_OBJECT
public:
 explicit ConversationList(QWidget *parent=nullptr);
 void upsert(DialogRow row);
 void removeConversation(QString id);
 void setSelected(QString id);
 QString selected() const {return selectedId;}
 void setFilter(QString text);
 void setSortMode(DialogsModel::SortMode mode);
 const DialogsModel& model() const{return data;}
 signals:void conversationActivated(QString id);
protected:
 void paintEvent(QPaintEvent*)override;
 void resizeEvent(QResizeEvent*)override;
 void mousePressEvent(QMouseEvent*)override;
 void keyPressEvent(QKeyEvent*)override;
private:
 DialogsModel data;QString selectedId,filter;
 std::vector<DialogRow*> visible;
 void refresh();void activate(DialogRow* row);void ensureVisible(int index);
};
}
