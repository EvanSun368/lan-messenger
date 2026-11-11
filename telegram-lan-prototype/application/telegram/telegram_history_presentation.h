#pragma once
// Adapted from Telegram Desktop history_view_element.cpp and
// history_view_service_message.cpp at c4bd064bcc84a359179e191fa6dfd216acda504f.
// Copyright Telegram Desktop Authors. GPL-3.0-or-later; see LICENSE/LEGAL.
#include <QDateTime>
#include <QString>
#include <QVector>
#include <QWidget>
#include "telegram_bubble_geometry.h"
namespace TelegramLan {
struct HistoryMessage {
 QString id,senderId;
 QDateTime timestamp; // Caller supplies actual timestamp, converted to display timezone.
 bool outgoing=false;
 bool hidden=false,empty=false,service=false;
 bool sponsored=false,hideAuthor=false,guestBot=false;
 bool hideDisplayDate=false,unreadBoundary=false,serviceBefore=false,threadBoundary=false;
 bool inlineMarkupVisible=false;
 bool unwrapped=false,mediaHidesFromName=false,mediaSkipsTail=false;
 QString threadId;
};
struct HistoryDecoration {
 bool showDate=false,attachPrevious=false,attachNext=false;
 bool bubbleAttachPrevious=false,bubbleAttachNext=false;
};
struct MessagePresentation {
 bool hasFromPhoto=false,displayFromPhoto=false,displayFromName=false,rightAligned=false;
 TelegramBubble::Rounding rounding;
};
// Ordinary Context::History / ElementChatMode::Default, no cloud special peers.
MessagePresentation PresentHistoryMessage(const HistoryMessage &message,
 const HistoryDecoration &decoration,bool groupConversation);
HistoryDecoration DecorateHistoryMessage(const HistoryMessage &current,const HistoryMessage *previous);
QVector<HistoryDecoration> DecorateHistory(const QVector<HistoryMessage> &messages);
// New QWidget host; actual date-badge geometry derived from PaintPreparedDate.
class TelegramDateBadge final:public QWidget {
public:
 explicit TelegramDateBadge(QString text,QWidget *parent=nullptr);
 QRect badgeRect() const;
protected:
 void paintEvent(QPaintEvent*) override;
private:
 QString text_;
};
}
