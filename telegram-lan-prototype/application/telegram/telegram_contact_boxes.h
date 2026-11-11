#pragma once
#include "telegram_profile_box.h"
#include <QJsonArray>
#include "contactmailbook.h"

// A local host of the original layer stack. Peer data and mutations remain
// with Node/MainWindow; this helper never requests data or invents success.
class TelegramContactBoxHost final : public Ui::RpWidget {
public:
 TelegramContactBoxHost(QWidget *parent,QString name,Fn<void(not_null<Ui::GenericBox*>)> init)
 :Ui::RpWidget(parent),previous_(QApplication::focusWidget()),layers_(this) {
  setObjectName(name);parent->installEventFilter(this);setGeometry(parent->rect());show();raise();
  layers_.setHideByBackgroundClick(false);
  auto content=object_ptr<Ui::GenericBox>(this,[this,init=std::move(init)](not_null<Ui::GenericBox*> box){
   box->setWidth(availableWidth());init(box);
  });
  content->setObjectName("telegramContactBox");box_=content.data();
  layers_.showBox(std::move(content),Ui::LayerOption::KeepOther,anim::type::instant);
  layers_.layerShownValue() | rpl::on_next([this](bool shown){
   if(!shown){hide();if(previous_ && previous_->isVisible())previous_->setFocus();deleteLater();}
  },lifetime());
  layers_.setFocus();
 }
 void closeBox() {if(box_)box_->closeBox();}
protected:
 bool eventFilter(QObject *watched,QEvent *event) override {
  if(watched==parentWidget() && event->type()==QEvent::Resize) {
   setGeometry(parentWidget()->rect());resizeBox();
  }
  return Ui::RpWidget::eventFilter(watched,event);
 }
private:
 int availableWidth() const {return qMax(1,qMin(440,width()-32));}
 void resizeBox() {
  if(!box_ || !box_->hasDelegate() || box_->width()==availableWidth())return;
  box_->setWidth(availableWidth());
  auto layout=box_->verticalLayout();
  auto *wrapper=dynamic_cast<Ui::RpWidget*>(layout->parentWidget());
  if(wrapper)wrapper->resizeToWidth(availableWidth());else layout->resizeToWidth(availableWidth());
  box_->getDelegate()->setDimensions(availableWidth(),wrapper?wrapper->height():layout->height(),true);
  if(auto *layer=dynamic_cast<Ui::BoxLayerWidget*>(box_->parentWidget()))layer->parentResized();
 }
 QPointer<Ui::GenericBox> box_;
 QPointer<QWidget> previous_;
 Ui::LayerManager layers_;
};

inline TelegramContactBoxHost *telegramShowCreateGroup(QWidget *parent,QJsonArray peers,
    Fn<bool(QStringList,QString)> commit) {
 ensureTelegramUiRuntime();
 for(auto *widget:parent->findChildren<QWidget*>("telegramCreateGroupOverlay",Qt::FindDirectChildrenOnly))
  if(auto *host=dynamic_cast<TelegramContactBoxHost*>(widget);host && host->isVisible()){host->raise();return host;}
 return new TelegramContactBoxHost(parent,"telegramCreateGroupOverlay",
  [peers=std::move(peers),commit=std::move(commit)](not_null<Ui::GenericBox*> box) {
   box->setTitle(rpl::single(QString("创建固定成员讨论组")));
   auto name=box->addRow(object_ptr<Ui::InputField>(box,st::defaultInputField,
      Ui::InputField::Mode::SingleLine,rpl::single(QString("讨论组名称"))));
   name->setObjectName("telegramGroupName");name->setMaxLength(80);
   QList<QPair<QString,QPointer<Ui::Checkbox>>> members;
   QSet<QString> seen;
   for(const auto &value:peers) {
    const auto peer=value.toObject();const auto id=peer["id"].toString();
    if(id.isEmpty() || seen.contains(id))continue;seen.insert(id);
    auto label=peer["name"].toString();const auto unread=peer["unread"].toInt();
    if(unread>0)label+=QString("  (%1)").arg(unread);
    auto check=box->addRow(object_ptr<Ui::Checkbox>(box,label,false));
    check->setObjectName("telegramGroupMember_"+id);check->setProperty("peerId",id);
    check->setProperty("unread",unread);members.push_back({id,check});
   }
   auto error=box->addRow(object_ptr<Ui::FlatLabel>(box,QString(),st::boxLabel));
   error->setObjectName("telegramGroupValidation");
   QPointer<Ui::GenericBox> weak(box.get());
   QPointer<Ui::InputField> field(name);QPointer<Ui::FlatLabel> validation(error);
   auto save=box->addButton(rpl::single(QString("创建")),[weak,field,validation,members,commit]{
    if(!weak || !field)return;
    QStringList ids;for(const auto &[id,check]:members)if(check && check->checked())ids.push_back(id);
    if(ids.isEmpty()){if(validation)validation->setText("至少选择一名成员");return;}
    if(commit && commit(ids,field->getLastText()) && weak)weak->closeBox();
   });
   save->setObjectName("telegramGroupCreate");
   auto cancel=box->addButton(rpl::single(QString("取消")),[weak]{if(weak)weak->closeBox();});
   cancel->setObjectName("telegramGroupCancel");
   box->setFocusCallback([field]{if(field)field->setFocus();});
  });
}

inline TelegramContactBoxHost *telegramShowContactProfile(QWidget *parent,QString peerId,QJsonObject profile,
    QString localEmail={},Fn<bool(QString)> saveLocalEmail=nullptr) {
 ensureTelegramUiRuntime();
 auto *host=new TelegramContactBoxHost(parent,"telegramContactProfileOverlay",
  [profile=std::move(profile),localEmail=std::move(localEmail),saveLocalEmail=std::move(saveLocalEmail)](not_null<Ui::GenericBox*> box) {
   box->setTitle(rpl::single(QString("联系人资料（未验证身份）")));
   for(const auto &[key,title]:QList<QPair<QString,QString>>{{"nickname","昵称"},{"signature","签名"},
      {"organization","组织"},{"department","部门"},{"location","所在地"}}) {
    box->addRow(object_ptr<Ui::FlatLabel>(box,title,st::boxLabel));
    auto value=box->addRow(object_ptr<Ui::FlatLabel>(box,profile[key].toString(),st::boxLabel));
    value->setObjectName("telegramContactProfile_"+key);value->setSelectable(true);
    value->setClickHandlerFilter([](const auto&,Qt::MouseButton){return false;});
   }
   if(saveLocalEmail) {
    auto input=box->addRow(object_ptr<Ui::InputField>(box,st::defaultInputField,
      Ui::InputField::Mode::SingleLine,rpl::single(QString("本机通讯录邮箱")),localEmail));
    input->setObjectName("telegramContactLocalEmail");input->setMaxLength(254);
    box->addRow(object_ptr<Ui::FlatLabel>(box,QString("仅保存在本机，不向其他设备共享。清空后保存可删除邮箱。"),st::boxLabel));
    auto status=box->addRow(object_ptr<Ui::FlatLabel>(box,QString(),st::boxLabel));
    status->setObjectName("telegramContactLocalEmailStatus");
    QPointer<Ui::InputField> field(input);QPointer<Ui::FlatLabel> feedback(status);
    auto save=box->addButton(rpl::single(QString("保存本机邮箱")),[field,feedback,saveLocalEmail]{
     if(!field || !feedback)return;
     const auto value=field->getLastText();
     if(!ContactMailBook::validEmail(value)){feedback->setText("邮箱格式无效");return;}
     const bool ok=saveLocalEmail(value);
     if(feedback)feedback->setText(ok?"已保存到本机":"保存失败，编辑内容已保留");
    });
    save->setObjectName("telegramContactLocalEmailSave");
   }
   QPointer<Ui::GenericBox> weak(box.get());
   auto close=box->addButton(rpl::single(QString("关闭")),[weak]{if(weak)weak->closeBox();});
   close->setObjectName("telegramContactProfileClose");
  });
 host->setProperty("peerId",peerId);return host;
}
