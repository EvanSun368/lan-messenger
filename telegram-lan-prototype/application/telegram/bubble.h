#pragma once
#include "telegram_bubble_geometry.h"
#include <QPainter>
#ifdef LANLINK_FULL_TELEGRAM_UI
#include "ui/style/style_core_icon.h"
#include "styles/style_lan_original_dialogs.h"
#endif
inline int TelegramBubbleTailInset(bool outgoing) {
#ifdef LANLINK_FULL_TELEGRAM_UI
 return (outgoing?st::lanHistoryBubbleTailOutRight:st::lanHistoryBubbleTailInLeft).width();
#else
 Q_UNUSED(outgoing);return 0;
#endif
}
// Ordinary PaintSolidBubble tail placement from upstream message_bubble.cpp.
// The generic corner/body algorithm remains the existing source-derived port.
inline void PaintTelegramBubble(QPainter &p,QRect rect,QColor color,bool outgoing,
 TelegramBubble::Rounding rounding) {
 using namespace TelegramBubble;
 Bubble args;args.rounding=rounding;
#ifdef LANLINK_FULL_TELEGRAM_UI
 const bool rightTail=rounding.bottomRight==Corner::Tail;
 const auto &tail=outgoing
  ?(rightTail?st::lanHistoryBubbleTailOutRight:st::lanHistoryBubbleTailOutLeft)
  :(rightTail?st::lanHistoryBubbleTailInRight:st::lanHistoryBubbleTailInLeft);
 const auto tailShift=rightTail?QPoint(0,tail.height()):QPoint(tail.width(),tail.height());
 // Upstream tails extend beyond bubble geometry; reserve their actual width
 // inside this QWidget so clipping does not silently erase the tail.
 // Reserve the same outside column throughout a group, even when a preceding
 // bubble has a Small corner instead of a tail. Body and text must not shift.
 const int inset=TelegramBubbleTailInset(outgoing);
 if(outgoing)rect.adjust(0,0,-inset,0);
 else rect.adjust(inset,0,0,0);
#else
 // Lightweight build has no upstream icon runtime: explicitly use small corners
 // rather than pretend that a no-op callback rendered an original tail.
 if(args.rounding.bottomLeft==Corner::Tail)args.rounding.bottomLeft=Corner::Small;
 if(args.rounding.bottomRight==Corner::Tail)args.rounding.bottomRight=Corner::Small;
#endif
 args.geometry=rect;
 args.largeRadius=std::max(0,std::min({16,rect.width()/2,rect.height()/2}));
 args.smallRadius=std::min(6,args.largeRadius);
 PaintBubbleGeneric(args,
  [&](QRect r){p.fillRect(r,color);},
  [&](QRect r){p.fillRect(r,QColor(0,0,0,20));},
  [&](int x,int y,int index,Corner size){
   int d=(size==Corner::Large)?args.largeRadius:args.smallRadius;
   p.save();p.setClipRect(QRect(x,y,d,d));p.setPen(Qt::NoPen);p.setBrush(color);
   const bool right=(index==kTopRight||index==kBottomRight);
   const bool bottom=(index==kBottomLeft||index==kBottomRight);
   p.drawEllipse(QRectF(x-(right?d:0),y-(bottom?d:0),2*d,2*d));p.restore();
  },[&](QPoint bottomPosition){
#ifdef LANLINK_FULL_TELEGRAM_UI
   tail.paint(p,bottomPosition-tailShift,p.device()->width()/p.device()->devicePixelRatioF());
   return tail.width();
#else
   Q_UNUSED(bottomPosition);return 0;
#endif
  });
}
inline void PaintTelegramBubble(QPainter &p,QRect rect,QColor color,bool outgoing) {
 TelegramBubble::Rounding rounding;
 (outgoing?rounding.bottomRight:rounding.bottomLeft)=TelegramBubble::Corner::Tail;
 PaintTelegramBubble(p,rect,color,outgoing,rounding);
}
