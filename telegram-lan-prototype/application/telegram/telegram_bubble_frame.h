#pragma once
#include "bubble.h"
#include <QFrame>
#ifdef LANLINK_FULL_TELEGRAM_UI
#include "styles/palette.h"
#endif
class TelegramBubbleFrame final : public QFrame {
public:
 explicit TelegramBubbleFrame(bool outgoing,QWidget *parent=nullptr):QFrame(parent),_outgoing(outgoing){
  (_outgoing?_rounding.bottomRight:_rounding.bottomLeft)=TelegramBubble::Corner::Tail;
 }
 void setRounding(TelegramBubble::Rounding rounding){_rounding=rounding;update();}
 TelegramBubble::Rounding rounding()const{return _rounding;}
 int tailInset()const{return TelegramBubbleTailInset(_outgoing);}
 QMargins layoutPadding()const {
  // Original msgPadding plus the host-only reserved outside tail column.
  return QMargins(11+(_outgoing?0:tailInset()),8,
                  11+(_outgoing?tailInset():0),8);
 }
protected:
 void paintEvent(QPaintEvent*)override {
  QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
  #ifdef LANLINK_FULL_TELEGRAM_UI
  const auto color = (_outgoing ? st::msgOutBg : st::msgInBg)->c;
#else
  // Default Telegram palette values; full mode consumes the actual palette.
  const auto color = _outgoing ? QColor("#effdde") : QColor(Qt::white);
#endif
  PaintTelegramBubble(p,rect(),color,_outgoing,_rounding);
 }
private:
 bool _outgoing;
 TelegramBubble::Rounding _rounding;
};
