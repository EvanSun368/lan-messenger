/* Telegram Desktop source-derived list mechanics; see accompanying ledger. */
#include "telegram_lan_history_list.h"
#include <QPaintEvent>
#include <QSet>
#include <QPointer>
#include <algorithm>
namespace TelegramLan {
bool LanHistoryList::replace(std::vector<std::unique_ptr<HistoryElement>> items,bool resize) {
 QSet<QString> ids;
 for(const auto &item:items) {
  if(!item || item->id().isEmpty() || ids.contains(item->id()))return false;
  ids.insert(item->id());
 }
 items_=std::move(items);itemsWidth_=-1;if(resize)resizeToWidth(width());update();return true;
}
int LanHistoryList::resizeGetHeight(int newWidth) {
 // Original ListWidget::resizeGetHeight ordinary-message loop. Reveal effects,
 // cloud about-view and sponsored padding are intentionally not constructed.
 update();
 const auto resizeAllItems=(itemsWidth_!=newWidth);
 auto newHeight=0;
 for(const auto &view:items_) {
  view->setY(newHeight);
  if(view->pendingResize() || resizeAllItems)newHeight+=view->resizeGetHeight(newWidth);
  else newHeight+=view->height();
  view->place(QRect(origin_.x(),origin_.y()+view->y(),newWidth,view->height()));
 }
 itemsWidth_=newWidth;itemsHeight_=newHeight;return itemsHeight_;
}
int LanHistoryList::findItemIndexByY(int y)const {
 // Original lower_bound and endpoint clamp, without cloud collapse gaps.
 if(items_.empty())return -1;
 if(y<origin_.y())return 0;
 const auto i=std::lower_bound(begin(items_),end(items_),y,[this](const auto &elem,int top){
  return itemTop(elem.get())+elem->height()<=top;
 });
 return std::min(int(i-begin(items_)),int(items_.size()-1));
}
HistoryElement *LanHistoryList::strictFindItemByY(int y)const {
 return !items_.empty() && y>=origin_.y() && y<origin_.y()+itemsHeight_?items_[findItemIndexByY(y)].get():nullptr;
}
HistoryScrollAnchor LanHistoryList::captureAnchor()const {
 // Original countScrollState position + relative Y, with LAN id replacing
 // Data::MessagePosition; no remote read markers or sponsored records.
 if(items_.empty())return {};
 const auto *view=items_[findItemIndexByY(visibleTop_)].get();
 return {view->id(),visibleTop_-itemTop(view)};
}
int LanHistoryList::restoreAnchor(const HistoryScrollAnchor &anchor)const {
 for(const auto &view:items_)if(view->id()==anchor.id)return itemTop(view.get())+anchor.offset;
 return -1; // Explicit missing anchor, never silently jump to another message.
}
std::vector<QString> LanHistoryList::visibleIds(const QRect &clip)const {
 const auto from=std::lower_bound(begin(items_),end(items_),clip.top(),[this](const auto &elem,int top){
  return itemTop(elem.get())+elem->height()<=top;
 });
 const auto to=std::lower_bound(begin(items_),end(items_),clip.top()+clip.height(),[this](const auto &elem,int bottom){
  return itemTop(elem.get())<bottom;
 });
 std::vector<QString> ids;
 for(auto i=from;i!=to;++i)ids.push_back((*i)->id());
 return ids;
}
void LanHistoryList::paintEvent(QPaintEvent *event) {
 // Original paintEvent visible-item search + per-item translated drawing.
 // For painted Elements this selects O(log N + visible items). The app currently
 // uses QWidget Elements; their children still paint through Qt, not virtualization.
 const auto clip=event->rect();
 const auto from=std::lower_bound(begin(items_),end(items_),clip.top(),[this](const auto &elem,int top){
  return itemTop(elem.get())+elem->height()<=top;
 });
 const auto to=std::lower_bound(begin(items_),end(items_),clip.top()+clip.height(),[this](const auto &elem,int bottom){
  return itemTop(elem.get())<bottom;
 });
 QPainter p(this);
 for(auto i=from;i!=to;++i) {
  auto *view=i->get();const auto top=itemTop(view);
  p.save();p.translate(0,top);view->draw(p,clip.translated(0,-top));p.restore();
 }
}

namespace {
class WidgetElement final:public HistoryElement {
public:
 WidgetElement(QString id,QWidget *widget):HistoryElement(std::move(id)),widget_(widget){}
 void draw(QPainter &,const QRect &)override {} // QWidget paints itself once, not twice.
 void place(const QRect &rect)override {if(widget_)widget_->setGeometry(rect);}
protected:
 int measure(int width)override {
  if(!widget_)return 1;
  const int proposed=widget_->hasHeightForWidth()?widget_->heightForWidth(width):widget_->sizeHint().height();
  return std::clamp(proposed,widget_->minimumHeight(),widget_->maximumHeight());
 }
private:QPointer<QWidget> widget_; // takeAt/delete cannot leave a dangling layout item.
};
}
int LanHistoryLayout::heightForWidth(int width)const {
 const auto margins=contentsMargins();int height=margins.top()+margins.bottom();
 const auto inner=std::max(1,width-margins.left()-margins.right());
 int rows=0;
 for(int i=0;i<count();++i) {
  auto *item=itemAt(i);if(item->spacerItem())continue;
  height+=std::max(1,item->hasHeightForWidth()?item->heightForWidth(inner):item->sizeHint().height());
  ++rows;
 }
 return height+std::max(0,rows-1)*std::max(0,spacing());
}
QSize LanHistoryLayout::sizeHint()const {
 const int width=list_->width()>0?list_->width():380;
 return QSize(width,heightForWidth(width));
}
QSize LanHistoryLayout::minimumSize()const {
 const auto m=contentsMargins();return QSize(m.left()+m.right()+1,m.top()+m.bottom()+1);
}
void LanHistoryLayout::setGeometry(const QRect &rect) {
 if(layingOut_)return;
 layingOut_=true;
 // Do not call QVBoxLayout::setGeometry: that would let a second engine assign
 // row positions. QLayout retains ownership, insertion and removal semantics.
 QLayout::setGeometry(rect);
 const auto m=contentsMargins();const auto inner=std::max(1,rect.width()-m.left()-m.right());
 std::vector<std::unique_ptr<HistoryElement>> elements;
 for(int i=0;i<count();++i) {
  auto *item=itemAt(i);
  if(item->spacerItem())continue; // terminal stretch is remaining viewport space
  auto *widget=item->widget();
  const auto stable=widget?widget->property("historyMessageId").toString():QString();
  const auto id=stable.isEmpty()?QString("layout:%1").arg(quintptr(item),0,16):stable;
  elements.push_back(std::make_unique<WidgetElement>(id,widget));
 }
 if(list_->replace(std::move(elements),false)) {
  // Original ListWidget::countItemsTop: minHeight - (itemsHeight + bottomPadding),
  // floored at zero. Host margins remain outside that ordinary-message area.
  const int content=list_->layoutElements(inner,rect.topLeft()+QPoint(m.left(),m.top()));
  const int available=std::max(0,rect.height()-m.top()-m.bottom());
  const int itemsTop=m.top()+std::max(0,available-content);
  list_->layoutElements(inner,rect.topLeft()+QPoint(m.left(),itemsTop));
 }
 layingOut_=false;
}
}
