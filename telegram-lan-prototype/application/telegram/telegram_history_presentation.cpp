#include "telegram_history_presentation.h"
#include <QFontMetrics>
#include <QPainter>
#include <algorithm>
#include <cmath>
namespace TelegramLan {
HistoryDecoration DecorateHistoryMessage(const HistoryMessage &current,const HistoryMessage *previous) {
 HistoryDecoration result;
 // Source recountDisplayDateInBlocks excludes hidden/empty/sponsored entries.
 if(current.hidden||current.empty||current.sponsored)return result;
 // Missing LAN timestamps cannot truthfully produce a date or grouping.
 result.showDate=!current.hideDisplayDate && current.timestamp.isValid() && (!previous || previous->hideDisplayDate
    || !previous->timestamp.isValid() || previous->timestamp.date()!=current.timestamp.date());
 const auto attachable=[](const HistoryMessage &m){return !m.service&&!m.empty&&!m.hidden&&!m.sponsored&&!m.hideAuthor&&!m.guestBot;};
 // Source computeIsAttachToPrevious: DateBadge, UnreadBar, ServicePreMessage,
 // ForumThreadBar preclude attachment. Ordinary LAN messages have no ephemeral,
 // Saved Messages/imported-forward or anonymous-megagroup branches.
 if(previous && !result.showDate && !current.unreadBoundary && !current.serviceBefore
   && !current.threadBoundary && current.timestamp.isValid() && previous->timestamp.isValid()
   && std::abs(previous->timestamp.secsTo(current.timestamp))<900
   && attachable(current) && attachable(*previous)
   && !previous->inlineMarkupVisible && current.threadId==previous->threadId
   && !current.senderId.isEmpty() && current.senderId==previous->senderId
   && current.outgoing==previous->outgoing) result.attachPrevious=true;
 result.bubbleAttachPrevious=result.attachPrevious && !previous->unwrapped;
 return result;
}
QVector<HistoryDecoration> DecorateHistory(const QVector<HistoryMessage> &messages) {
 QVector<HistoryDecoration> result(messages.size());int previous=-1;
 for(int i=0;i<messages.size();++i) {
  result[i]=DecorateHistoryMessage(messages[i],previous<0?nullptr:&messages[previous]);
  if(previous>=0) {
   result[previous].attachNext=result[i].attachPrevious;
   result[previous].bubbleAttachNext=result[i].attachPrevious && !messages[i].unwrapped;
  }
  if(!messages[i].hidden&&!messages[i].empty)previous=i;
 }
 return result;
}
MessagePresentation PresentHistoryMessage(const HistoryMessage &message,
 const HistoryDecoration &decoration,bool groupConversation) {
 MessagePresentation result;
 // Message::hasFromPhoto / displayFromPhoto and hasFromName / displayFromName,
 // reduced to real LAN ordinary-user history; no forged channel/self/bot state.
 const bool ordinary=!message.hidden && !message.empty && !message.service && !message.sponsored;
 result.rightAligned=message.outgoing;
 result.hasFromPhoto=ordinary && !message.outgoing && groupConversation;
 result.displayFromPhoto=result.hasFromPhoto && !decoration.attachNext;
 result.displayFromName=ordinary && !message.outgoing && groupConversation
   && !message.mediaHidesFromName && !decoration.attachPrevious;
 // Message::countMessageRounding and countBubbleRounding ordinary branches.
 const bool smallTop=decoration.bubbleAttachPrevious;
 const bool smallBottom=decoration.bubbleAttachNext;
 const bool skipTail=smallBottom || message.mediaSkipsTail || message.inlineMarkupVisible;
 const bool right=result.rightAligned;
 using Corner=TelegramBubble::Corner;
 result.rounding={
  (smallTop && !right)?Corner::Small:Corner::Large,
  (smallTop && right)?Corner::Small:Corner::Large,
  (smallBottom && !right)?Corner::Small:(!skipTail && !right)?Corner::Tail:Corner::Large,
  (smallBottom && right)?Corner::Small:(!skipTail && right)?Corner::Tail:Corner::Large
 };
 if(message.inlineMarkupVisible)
  result.rounding.bottomLeft=result.rounding.bottomRight=Corner::Small;
 return result;
}
TelegramDateBadge::TelegramDateBadge(QString text,QWidget *parent):QWidget(parent),text_(std::move(text)) {
 setObjectName("telegramDateBadge");setAccessibleName(text_);
 QFont f=font();f.setPixelSize(13);f.setWeight(QFont::DemiBold);setFont(f);
 // Source DateBadge::height and chat.style margin(10,10,10,2), padding(12,3,12,4).
 setFixedHeight(10+3+QFontMetrics(f).height()+4+2);
}
QRect TelegramDateBadge::badgeRect() const {
 const auto textWidth=QFontMetrics(font()).horizontalAdvance(text_);
 const auto content=std::max(0,width()-20);
 const int badgeWidth=textWidth+24;
 const int left=10+(content-badgeWidth)/2;
 return {left,10,badgeWidth,3+QFontMetrics(font()).height()+4};
}
void TelegramDateBadge::paintEvent(QPaintEvent*) {
 QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
 const auto rect=badgeRect();p.setPen(Qt::NoPen);p.setBrush(QColor("#a5b4c1"));
 // Cached source corner pixmaps are replaced by equivalent local rounded fill.
 p.drawRoundedRect(rect,rect.height()/2.,rect.height()/2.);
 p.setFont(font());p.setPen(Qt::white);
 p.drawText(rect.left()+12,rect.top()+3+QFontMetrics(font()).ascent(),text_);
}
}
