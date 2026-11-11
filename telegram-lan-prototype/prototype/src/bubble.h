#pragma once
#include "telegram_bubble_geometry.h"
#include <QPainter>
#include <QPainterPath>
inline void PaintTelegramBubble(QPainter &p, QRect rect, QColor color, bool outgoing) {
 using namespace TelegramBubble;
 Bubble args; args.geometry=rect;
 args.largeRadius=std::min({14,rect.width()/2,rect.height()/2});
 args.smallRadius=std::min(5,args.largeRadius);
 (outgoing?args.rounding.bottomRight:args.rounding.bottomLeft)=Corner::Small;
 PaintBubbleGeneric(args,
  [&](QRect r){p.fillRect(r,color);},
  [&](QRect r){p.fillRect(r,QColor(0,0,0,20));},
  [&](int x,int y,int index,Corner size){
   int d=(size==Corner::Large)?args.largeRadius:args.smallRadius;
   p.save(); p.setClipRect(QRect(x,y,d,d));
   p.setPen(Qt::NoPen); p.setBrush(color);
   const bool right=(index==kTopRight||index==kBottomRight);
   const bool bottom=(index==kBottomLeft||index==kBottomRight);
   p.drawEllipse(QRectF(x-(right?d:0),y-(bottom?d:0),2*d,2*d)); p.restore();
  },[](QPoint){return 0;});
}
