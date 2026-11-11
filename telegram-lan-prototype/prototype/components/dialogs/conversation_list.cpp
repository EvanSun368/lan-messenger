#include "conversation_list.h"
#include "telegram_row_painter.h"
#include <QPainter>
#include <QScrollBar>
#include <QMouseEvent>
#include <QKeyEvent>
namespace TelegramLan {
ConversationList::ConversationList(QWidget*p):QAbstractScrollArea(p){
 setFocusPolicy(Qt::StrongFocus);setFrameShape(QFrame::NoFrame);setMinimumWidth(220);
 setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
 connect(verticalScrollBar(),&QScrollBar::valueChanged,viewport(),qOverload<>(&QWidget::update));
}
void ConversationList::upsert(DialogRow r){data.upsert(std::move(r));refresh();}
void ConversationList::removeConversation(QString id){data.remove(id);if(selectedId==id)selectedId.clear();refresh();}
void ConversationList::setSelected(QString id){selectedId=std::move(id);viewport()->update();}
void ConversationList::setFilter(QString text){filter=std::move(text);refresh();}
void ConversationList::setSortMode(DialogsModel::SortMode m){data.setSortMode(m);refresh();}
void ConversationList::refresh(){visible.clear();for(auto r:data.rows())if(filter.isEmpty()||r->name.contains(filter,Qt::CaseInsensitive)||r->preview.contains(filter,Qt::CaseInsensitive))visible.push_back(r);verticalScrollBar()->setRange(0,std::max(0,int(visible.size())*TelegramDialogRowHeight-viewport()->height()));verticalScrollBar()->setPageStep(viewport()->height());viewport()->update();}
void ConversationList::resizeEvent(QResizeEvent*e){QAbstractScrollArea::resizeEvent(e);refresh();}
void ConversationList::paintEvent(QPaintEvent*){
 QPainter p(viewport());p.fillRect(viewport()->rect(),Qt::white);
 for(int i=0;i<int(visible.size());++i){
  auto r=visible[i];int y=i*TelegramDialogRowHeight-verticalScrollBar()->value();
  if(y+TelegramDialogRowHeight<0||y>viewport()->height())continue;
  DialogPresentation row;row.id=r->id;row.name=r->name;row.preview=r->preview;row.date=r->timeLabel;row.unread=r->unread;row.online=r->online;
  row.avatarColor=QColor::fromHsv(int(qHash(r->id)%360),105,190);
  p.save();p.translate(0,y);PaintTelegramDialogRow(p,row,{viewport()->width(),r->id==selectedId,false,false});p.restore();
 }
}
void ConversationList::activate(DialogRow*r){if(!r)return;selectedId=r->id;viewport()->update();emit conversationActivated(r->id);}
void ConversationList::mousePressEvent(QMouseEvent*e){if(e->button()!=Qt::LeftButton)return;int i=(int(e->position().y())+verticalScrollBar()->value())/TelegramDialogRowHeight;if(i>=0&&i<int(visible.size()))activate(visible[i]);}
void ConversationList::ensureVisible(int i){int top=i*TelegramDialogRowHeight,bottom=top+TelegramDialogRowHeight;if(top<verticalScrollBar()->value())verticalScrollBar()->setValue(top);else if(bottom>verticalScrollBar()->value()+viewport()->height())verticalScrollBar()->setValue(bottom-viewport()->height());}
void ConversationList::keyPressEvent(QKeyEvent*e){
 int i=0;for(int j=0;j<int(visible.size());++j)if(visible[j]->id==selectedId)i=j;
 if(e->key()==Qt::Key_Down)i++;else if(e->key()==Qt::Key_Up)i--;else return QAbstractScrollArea::keyPressEvent(e);
 if(visible.empty())return;i=std::clamp(i,0,int(visible.size())-1);activate(visible[i]);ensureVisible(i);
}
}
