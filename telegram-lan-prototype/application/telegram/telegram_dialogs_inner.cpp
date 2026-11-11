/* Adapted from Telegram Desktop dialogs/dialogs_inner_widget.cpp:
 * paintEvent default ordinary rows, mousePressReleased ordinary row branch,
 * selectSkip and keyboard navigation. Copyright Telegram Desktop Authors.
 * GPL-3.0-or-later, upstream exception; see repository LICENSE/LEGAL.
 * Host replaces Session/Entry/Row with persistent Qt model indices. */
#include "telegram_dialogs_inner.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QScrollBar>
#include <QTimer>
#include <algorithm>
#include <utility>
#include "ui/painter.h"
#include "ui/text/text.h"

namespace TelegramLan {
class DialogsInner final : public Ui::RpWidget {
public:
 struct Row { QPersistentModelIndex index; DialogPresentation presentation; std::unique_ptr<Ui::Text::String> name,preview; };
 DialogsInner(DialogsView *view,QAbstractItemModel *model,QItemSelectionModel *selection,DialogsView::Present present)
 :Ui::RpWidget(view),view_(view),model_(model),selection_(selection),present_(std::move(present)) {
  setObjectName("telegramDialogsInner");setMouseTracking(true);setFocusPolicy(Qt::StrongFocus);
  auto reload=[this]{refresh();};
  connect(model,&QAbstractItemModel::dataChanged,this,reload);
  connect(model,&QAbstractItemModel::modelReset,this,reload);
  connect(model,&QAbstractItemModel::rowsInserted,this,reload);
  connect(model,&QAbstractItemModel::rowsRemoved,this,reload);
  connect(model,&QAbstractItemModel::layoutChanged,this,reload);
  connect(selection,&QItemSelectionModel::currentChanged,this,[this]{update();});
  refresh();
 }
 void refresh() {
  rows_.clear();
  if(model_)for(int i=0;i<model_->rowCount();++i) {
   auto index=model_->index(i,0);if(auto presentation=present_(index))rows_.push_back({index,*presentation,
    std::make_unique<Ui::Text::String>(st::semiboldTextStyle,presentation->name.left(4096)),
    std::make_unique<Ui::Text::String>(st::defaultTextStyle,presentation->preview.simplified().left(4096))});
  }
  pressed_=QPersistentModelIndex();selected_=QPersistentModelIndex();
  setMinimumHeight(rows_.size()*TelegramDialogRowHeight);
  resize(width(),rows_.size()*TelegramDialogRowHeight);update();
 }
 QRect rowRect(const QModelIndex &index) const {
  for(int i=0;i<int(rows_.size());++i)if(rows_[i].index==index)return {0,i*TelegramDialogRowHeight,width(),TelegramDialogRowHeight};
  return {};
 }
 QModelIndex current() const {return selection_?selection_->currentIndex():QModelIndex();}
private:
 // Port of ordinary visible-row iteration, upstream paintEvent lines1322-1350.
 // Fixed-height LAN rows replace list.findByY and Row::top/height. Cloud pinned
 // reordering/collapsed folders are not present in this model.
 void paintEvent(QPaintEvent *event) override {
  Painter p(this);p.fillRect(event->rect(),Qt::white);
  const auto clip=event->rect();
  auto i=std::max(0,clip.top()/TelegramDialogRowHeight);
  auto top=i*TelegramDialogRowHeight;
  p.translate(0,top);
  for(;i<int(rows_.size());++i) {
   if(top>=clip.top()+clip.height())break;
   const auto &row=rows_[i];
   const bool active=row.index==current();
   const auto geometry=PaintTelegramDialogRow(p,row.presentation,{width(),active,row.index==(pressed_.isValid()?pressed_:selected_),false},false);
   // Original Ui::Text::String, not QFontMetrics elision. Data-only names and
   // previews replace Entry/MessageView extraction; parser/emoji/bidi are lib_ui.
   p.setPen(active?Qt::white:Qt::black);
   if(geometry.name.width()>0)row.name->drawElided(p,geometry.name.x(),geometry.name.y(),geometry.name.width());
   p.setPen(active?QColor(Qt::white):QColor("#999999"));
   if(geometry.preview.width()>0)row.preview->drawElided(p,geometry.preview.x(),geometry.preview.y(),geometry.preview.width());
   p.translate(0,TelegramDialogRowHeight);top+=TelegramDialogRowHeight;
  }
 }
 QPersistentModelIndex at(QPoint position) const {
  const int i=position.y()/TelegramDialogRowHeight;
  return rect().contains(position)&&i>=0&&i<int(rows_.size())?rows_[i].index:QPersistentModelIndex();
 }
 // Upstream mouseMoveEvent guard: layout/scroll emits synthetic moves at the
 // same global position. They must not steal the keyboard-selected row.
 void mouseMoveEvent(QMouseEvent *e) override {
  const auto global=e->globalPosition().toPoint();
  if(!lastMousePosition_) {lastMousePosition_=global;return;}
  if(!mouseSelection_&&*lastMousePosition_==global)return;
  mouseSelection_=true;lastMousePosition_=global;selected_=at(e->pos());update();
 }
 void leaveEventHook(QEvent*) override {if(mouseSelection_){selected_=QPersistentModelIndex();update();}}
 void mousePressEvent(QMouseEvent *e) override {
  setFocus(Qt::MouseFocusReason);mouseSelection_=true;lastMousePosition_=e->globalPosition().toPoint();selected_=at(e->pos());pressButton_=e->button();pressed_=selected_;update();
 }
 // Retained ordinary chooseRow guard from mousePressReleased: same row at
 // press/release and left button only. Never activate a model row removed while
 // the pointer was down (QPersistentModelIndex is invalidated by the model).
 void mouseReleaseEvent(QMouseEvent *e) override {
  const auto button=std::exchange(pressButton_,Qt::NoButton);
  const auto pressed=pressed_;pressed_=QPersistentModelIndex();selected_=at(e->pos());update();
  if(button==Qt::LeftButton&&e->button()==Qt::LeftButton&&pressed.isValid()&&pressed==selected_)choose(pressed);
 }
 void choose(QModelIndex index) {
  QPointer<DialogsInner> guard(this);QPointer<DialogsView> view(view_);
  if(selection_)selection_->setCurrentIndex(index,QItemSelectionModel::ClearAndSelect);
  if(guard&&view&&view->activated) {const auto activate=view->activated;activate(index);}
 }
 // selectSkip default branch: first row if no keyboard selection, otherwise
 // clamp current+direction. Keep keyboard highlight separate from active chat.
 void selectSkip(int direction) {
  mouseSelection_=false;
  // Qt's scroll-area adapter synthesizes moves from the platform cursor, not
  // the last delivered mouse-event coordinate (these can differ under touch,
  // event replay and offscreen tests). Freeze the actual cursor baseline before
  // ensureVisible scrolls, so only subsequent physical movement takes over.
  lastMousePosition_=QCursor::pos();
  if(rows_.empty())return;
  auto found=std::find_if(rows_.begin(),rows_.end(),[&](const Row&r){return r.index==selected_;});
  int next=found==rows_.end()?0:std::clamp(int(found-rows_.begin())+direction,0,int(rows_.size())-1);
  selected_=rows_[next].index;update();
  view_->ensureVisible(0,next*TelegramDialogRowHeight+TelegramDialogRowHeight/2,0,TelegramDialogRowHeight/2);
 }
 void keyPressEvent(QKeyEvent *e) override {
  switch(e->key()) {
   case Qt::Key_Up:selectSkip(-1);break;
   case Qt::Key_Down:selectSkip(1);break;
   case Qt::Key_PageUp:selectSkip(-std::max(1,view_->viewport()->height()/TelegramDialogRowHeight));break;
   case Qt::Key_PageDown:selectSkip(std::max(1,view_->viewport()->height()/TelegramDialogRowHeight));break;
   case Qt::Key_Home:selected_=QPersistentModelIndex();selectSkip(0);break;
   case Qt::Key_End:if(!rows_.empty()){selected_=rows_.front().index;selectSkip(rows_.size());}break;
   case Qt::Key_Return:case Qt::Key_Enter:if(selected_.isValid())choose(selected_);break;
   default:Ui::RpWidget::keyPressEvent(e);return;
  }
  e->accept();
 }
 DialogsView *view_;QPointer<QAbstractItemModel> model_;QPointer<QItemSelectionModel> selection_;
 DialogsView::Present present_;std::vector<Row> rows_;
 QPersistentModelIndex selected_,pressed_;Qt::MouseButton pressButton_=Qt::NoButton;
 bool mouseSelection_=true;std::optional<QPoint> lastMousePosition_;
};
DialogsView::DialogsView(QWidget*parent,QAbstractItemModel*model,QItemSelectionModel*selection,Present present)
 :Ui::ScrollArea(parent),inner_(new DialogsInner(this,model,selection,std::move(present))) {
 setObjectName("telegramDialogsView");setWidgetResizable(true);setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
 setOwnedWidget(object_ptr<DialogsInner>::fromRaw(inner_));setFocusProxy(inner_);
}
DialogsView::~DialogsView()=default;
void DialogsView::refresh(){inner_->refresh();}
QRect DialogsView::rowRect(const QModelIndex&index)const {auto r=inner_->rowRect(index);r.moveTopLeft(inner_->mapTo(viewport(),r.topLeft()));return r;}
QModelIndex DialogsView::current()const{return inner_->current();}
QWidget*DialogsView::innerWidget()const{return inner_;}
}
