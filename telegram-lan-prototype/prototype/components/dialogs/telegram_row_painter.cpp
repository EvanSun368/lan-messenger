#include "telegram_row_painter.h"
#include "telegram_dialogs_model.h"
#include <QFontMetrics>
#include <QPainterPath>
namespace TelegramLan {
namespace {
// Generated-style substitution for original st::defaultDialogRow and fonts.
constexpr int paddingLeft=10,paddingTop=8,paddingRight=10,photoSize=46;
constexpr int nameLeft=68,nameTop=10,textTop=34,dateSkip=5;
constexpr int unreadHeight=19,unreadPadding=5;
QFont NormalFont(){QFont f;f.setPixelSize(13);return f;}
QFont NameFont(){auto f=NormalFont();f.setWeight(QFont::DemiBold);return f;}
QFont UnreadFont(){QFont f;f.setPixelSize(12);f.setBold(true);return f;}
QColor Foreground(const DialogPaintContext&c){return c.active?QColor(Qt::white):QColor("#000000");}
QColor Secondary(const DialogPaintContext&c){return c.active?QColor(Qt::white):QColor("#999999");}

// Derived from PaintRowTopRight: reserve date width before drawing name.
QRect PaintRowTopRight(QPainter &p,const QString &text,QRect &rectForName,
 const DialogPaintContext &context) {
 auto dateFont=NormalFont();const auto width=QFontMetrics(dateFont).horizontalAdvance(text);
 rectForName.setWidth(std::max(0,rectForName.width()-width-dateSkip));
 p.setFont(dateFont);p.setPen(Secondary(context));
 // Original text uses a baseline shared with name rather than vertical centering.
 const int x=rectForName.left()+rectForName.width()+dateSkip;
 const int baseline=rectForName.top()+QFontMetrics(NameFont()).height()-QFontMetrics(NormalFont()).descent();
 p.drawText(x,baseline,text);
 return {x,rectForName.top(),width,QFontMetrics(dateFont).height()};
}
// Derived from PaintUnreadBadge/CountUnreadBadgeSize. The cached two-half
// background is replaced by equivalent Qt rounded drawing; geometry is retained.
QRect PaintUnreadBadge(QPainter &p,QString text,int x,int y,
 const DialogPaintContext &context,bool muted) {
 const auto font=UnreadFont();const QFontMetrics metrics(font);
 const auto unreadWidth=metrics.horizontalAdvance(text);
 const auto unreadRectWidth=std::max(unreadWidth+2*unreadPadding,unreadHeight);
 const auto unreadRectLeft=x-unreadRectWidth;
 const auto badge=QRect(unreadRectLeft,y,unreadRectWidth,unreadHeight);
 const QColor bg=context.active?(muted?QColor("#c6e1f7"):QColor(Qt::white))
  :(muted?QColor("#bbbbbb"):QColor("#40a7e3"));
 p.setPen(Qt::NoPen);p.setBrush(bg);p.drawRoundedRect(badge,unreadHeight/2.,unreadHeight/2.);
 const auto badgeTextTop=(unreadHeight-metrics.height())/2;
 p.setFont(font);p.setPen(context.active?QColor("#419fd9"):QColor(Qt::white));
 p.drawText(unreadRectLeft+(unreadRectWidth-unreadWidth)/2,y+badgeTextTop+metrics.ascent(),text);
 return badge;
}
// Derived from PaintWideCounter: subtract badge width and padding from preview.
int PaintWideCounter(QPainter&p,const DialogPaintContext&context,
 const DialogPresentation&row,int availableWidth,QRect &badge) {
 const auto top=textTop+QFontMetrics(NormalFont()).ascent()-QFontMetrics(UnreadFont()).ascent()
  -(unreadHeight-QFontMetrics(UnreadFont()).height())/2;
 if(row.unread<=0)return availableWidth;
 badge=PaintUnreadBadge(p,FormatUnreadCounter(row.unread,false,context.narrow),
  context.width-paddingRight,top,context,row.muted);
 return std::max(0,availableWidth-badge.width()-unreadPadding);
}
void PaintAvatar(QPainter&p,const DialogPresentation&row,QRect rect,
 const DialogPaintContext&context){
 p.save();QPainterPath circle;circle.addEllipse(rect);p.setClipPath(circle);
 if(!row.avatar.isNull())p.drawImage(rect,row.avatar);
 else {p.fillRect(rect,row.avatarColor);auto f=NameFont();f.setPixelSize(20);p.setFont(f);p.setPen(Qt::white);p.drawText(rect,Qt::AlignCenter,row.name.left(1).toUpper());}
 p.restore();
 if(row.online){QRect dot(rect.right()-9,rect.bottom()-9,10,10);p.setBrush(context.active?QColor(Qt::white):QColor("#4dc920"));p.setPen(QPen(context.active?QColor("#419fd9"):QColor(Qt::white),2));p.drawEllipse(dot);}
}
}
DialogPaintResult PaintTelegramDialogRow(QPainter&p,const DialogPresentation&row,
 const DialogPaintContext&context){
 DialogPaintResult result;p.save();p.setRenderHint(QPainter::Antialiasing);
 p.setClipRect(QRect(0,0,context.width,TelegramDialogRowHeight));
 // This sequence is retained from PaintRow: background, userpic, narrow badge,
 // name rectangle, date reservation, preview counter/text, then name.
 const auto bg=context.active?QColor("#419fd9"):context.selected?QColor("#f1f1f1"):QColor(Qt::white);
 p.fillRect(QRect(0,0,context.width,TelegramDialogRowHeight),bg);
 result.avatar={paddingLeft,paddingTop,photoSize,photoSize};PaintAvatar(p,row,result.avatar,context);
 if(context.narrow){if(row.unread>0)result.badge=PaintUnreadBadge(p,FormatUnreadCounter(row.unread,false,true),paddingLeft+photoSize,paddingTop+photoSize-unreadHeight,context,row.muted);p.restore();return result;}
 const auto namewidth=std::max(0,context.width-nameLeft-paddingRight);
 auto rectForName=QRect(nameLeft,nameTop,namewidth,QFontMetrics(NameFont()).height());
 if(!row.date.isEmpty())result.date=PaintRowTopRight(p,row.date,rectForName,context);
 const auto availableWidth=PaintWideCounter(p,context,row,namewidth,result.badge);
 result.preview={nameLeft,textTop,availableWidth,QFontMetrics(NormalFont()).height()};
 p.setFont(NormalFont());p.setPen(Secondary(context));
 p.drawText(result.preview.left(),result.preview.top()+QFontMetrics(NormalFont()).ascent(),QFontMetrics(NormalFont()).elidedText(row.preview.simplified(),Qt::ElideRight,availableWidth));
 p.setFont(NameFont());p.setPen(Foreground(context));result.name=rectForName;
 p.drawText(rectForName.left(),rectForName.top()+QFontMetrics(NameFont()).ascent(),QFontMetrics(NameFont()).elidedText(row.name,Qt::ElideRight,rectForName.width()));
 p.restore();return result;
}
}
