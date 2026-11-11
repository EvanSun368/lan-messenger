#include "telegram_transfer_list.h"
#include "transfercard.h"
#include "telegram_contact_boxes.h"
#include "ui/widgets/labels.h"
#include <QFileInfo>
#include <QPainter>
#include <QResizeEvent>
#include <QPointer>
namespace TelegramLan {
class TransferListRow final:public Ui::RpWidget {
public:
 TransferListRow(QWidget *parent,TransferList *owner):Ui::RpWidget(parent),owner_(owner),style_(st::defaultFlatLabel) {
  style_.minWidth=1;style_.maxHeight=0;
  for(auto **label:{&name_,&bytes_,&status_})*label=new Ui::FlatLabel(this,QString(),style_);
  name_->setObjectName("originalTransferName");bytes_->setObjectName("originalTransferBytes");status_->setObjectName("originalTransferStatus");
  for(const auto &pair:QList<QPair<QString,QString>>{{"accept","接收"},{"reject","拒绝"},{"pause","暂停"},{"retry","重试"},{"open","打开文件"}}) {
   auto *button=new Ui::LinkButton(this,pair.second);
   button->setObjectName("originalTransfer_"+pair.first);buttons_[pair.first]=button;
   button->setClickedCallback([this,action=pair.first]{dispatch(action);});
  }
 }
 ~TransferListRow(){delete name_;delete bytes_;delete status_;}
 void setSnapshot(QJsonObject data) {
  data_=std::move(data);setProperty("transferId",data_["id"].toString());setProperty("transferState",data_["state"].toString());
  const auto state=data_["state"].toString();const bool incoming=data_["direction"]=="in";
  total_=qMax<qint64>(0,qint64(data_["total"].toDouble()));done_=qBound<qint64>(0,qint64(data_["done"].toDouble()),total_);
  name_->setText(data_["name"].toString());bytes_->setText(QString("%1 / %2 字节").arg(done_).arg(total_));
  status_->setText(TransferCard::statusText(data_));
  buttons_["accept"]->setVisible(incoming&&state=="waiting");buttons_["reject"]->setVisible(incoming&&state=="waiting");
  buttons_["pause"]->setVisible(state=="running"||state=="verifying");
  const bool remoteRetry=!incoming&&(state=="remote_failed"||state=="remote_paused");
  buttons_["retry"]->setVisible(((state=="paused"||state=="failed")&&(incoming||state=="failed"))||remoteRetry);
  buttons_["retry"]->setText(remoteRetry?QStringLiteral("重新发送"):QStringLiteral("重试"));
  buttons_["retry"]->setEnabled(data_["can_retry"].toBool(!remoteRetry));
  buttons_["open"]->setVisible(incoming&&(state=="completed"||state=="missing"));
  buttons_["open"]->setEnabled(canOpen());
  if(incoming&&state=="completed"&&!canOpen())status_->setText("本地文件已丢失或变更");
  layoutRow();update();
 }
 void layoutRow() {
  const int available=qMax(1,width()-32);int y=12;
  for(auto *label:{name_,bytes_}){label->resizeToWidth(available);label->move(16,y);y+=label->height()+6;}
  progressTop_=y;y+=10;
  status_->resizeToWidth(available);status_->move(16,y);y+=status_->height()+8;
  int x=16;bool any=false;
  for(const auto &key:{"accept","reject","pause","retry","open"}) {
   auto *button=buttons_[key];if(button->isHidden())continue;
   const int bw=qMin(available,qMax(50,button->width()));
   if(x>16&&x+bw>width()-16){x=16;y+=30;}
   button->resize(bw,24);button->move(x,y);x+=bw+18;any=true;
  }
  setFixedHeight(y+(any?30:0)+10);
 }
private:
 bool canOpen()const {
  const QFileInfo file(data_["path"].toString());
  return data_["direction"]=="in"&&data_["state"]=="completed"&&file.isFile()&&!file.isSymLink()&&file.size()==total_;
 }
 void dispatch(const QString &action) {
  auto *button=buttons_.value(action);if(!button||button->isHidden()||!button->isEnabled())return;
  QString path;
  if(action=="open") {
   if(!canOpen()){buttons_["open"]->setEnabled(false);status_->setText("本地文件已丢失或变更");layoutRow();return;}
   path=QFileInfo(data_["path"].toString()).absoluteFilePath();
  }
  const auto callback=owner_->actionRequested;
  if(callback)callback(action,data_["id"].toString(),path);
 }
 void resizeEvent(QResizeEvent*)override{layoutRow();}
 void paintEvent(QPaintEvent*)override {
  QPainter p(this);p.fillRect(rect(),Qt::white);p.fillRect(16,progressTop_,qMax(0,width()-32),3,QColor("#e8edf1"));
  const auto ratio=total_?double(done_)/total_:(data_["state"]=="completed"?1.:0.);
  p.fillRect(16,progressTop_,int(qMax(0,width()-32)*ratio),3,QColor("#419fd9"));
  p.fillRect(16,height()-1,qMax(0,width()-32),1,QColor("#eef0f2"));
 }
 TransferList *owner_;style::FlatLabel style_;Ui::FlatLabel *name_,*bytes_,*status_;
 QHash<QString,Ui::LinkButton*> buttons_;QJsonObject data_;qint64 total_=0,done_=0;int progressTop_=0;
};
TransferList::TransferList(QWidget *parent):Ui::RpWidget(parent) {
 ensureTelegramUiRuntime();setObjectName("telegramTransferList");
 scroll_=new Ui::ScrollArea(this);scroll_->setWidgetResizable(true);
 content_=new Ui::RpWidget(scroll_);scroll_->setOwnedWidget(object_ptr<Ui::RpWidget>::fromRaw(content_));
 scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}
void TransferList::upsert(QJsonObject snapshot) {
 const auto id=snapshot["id"].toString();if(id.isEmpty())return;
 auto *row=rows_.value(id);
 if(!row){row=new TransferListRow(content_,this);rows_.insert(id,row);order_.append(id);row->show();}
 row->resize(qMax(1,scroll_->viewport()->width()),row->height());row->setSnapshot(snapshot);relayout();
}
void TransferList::relayout(){int y=0;for(const auto &id:order_){auto *row=rows_[id];row->resize(qMax(1,scroll_->viewport()->width()),row->height());row->layoutRow();row->move(0,y);y+=row->height();}content_->setMinimumHeight(y);content_->resize(scroll_->viewport()->width(),y);}
void TransferList::resizeEvent(QResizeEvent*){scroll_->setGeometry(rect());relayout();}
QWidget *TransferList::rowWidget(const QString &id)const{return rows_.value(id);}
QPointer<TransferList> ShowTransferBox(QWidget *parent,QJsonArray snapshots,
 std::function<void(QString,QString,QString)> action) {
 ensureTelegramUiRuntime();
 for(auto *widget:parent->findChildren<QWidget*>("telegramTransferOverlay",Qt::FindDirectChildrenOnly)) {
  if(auto *host=dynamic_cast<TelegramContactBoxHost*>(widget);host&&host->isVisible()) {
   host->raise();
   for(auto *child:host->findChildren<QWidget*>("telegramTransferList"))
    if(auto *list=dynamic_cast<TransferList*>(child)) {
     list->actionRequested=std::move(action);
     for(const auto &snapshot:snapshots)list->upsert(snapshot.toObject());
     return list;
    }
  }
 }
 auto result=std::make_shared<QPointer<TransferList>>();
 new TelegramContactBoxHost(parent,"telegramTransferOverlay",
  [snapshots=std::move(snapshots),action=std::move(action),result](not_null<Ui::GenericBox*> box) mutable {
   box->setTitle(rpl::single(QString("文件传输")));
   box->addRow(object_ptr<Ui::FlatLabel>(box,QString("接收前需确认。文件仅在接收端校验完成后可打开；只在可信局域网使用。"),st::boxLabel));
   auto list=box->addRow(object_ptr<TransferList>(box));
   list->setFixedHeight(340);list->actionRequested=std::move(action);*result=list;
   for(const auto &snapshot:snapshots)list->upsert(snapshot.toObject());
   QPointer<Ui::GenericBox> weak(box.get());
   auto close=box->addButton(rpl::single(QString("关闭")),[weak]{if(weak)weak->closeBox();});
   close->setObjectName("telegramTransferClose");
  });
 return *result;
}
}
