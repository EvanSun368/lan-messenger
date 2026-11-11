#pragma once
// Original lib_ui layers and controls, hosted by a small LAN-specific overlay.
// No Telegram account/session/network implementation is involved.
#include "telegram_original_controls.h"
#include "ui/layers/generic_box.h"
#include "ui/layers/layer_manager.h"
#include "ui/layers/box_layer_widget.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/checkbox.h"
#include "styles/style_layers.h"
#include <QApplication>
#include <QJsonObject>
#include <QPointer>
#include <QEvent>
#include <map>

struct TelegramSettingsActions {
 bool notifications=true;
 Fn<void()> chooseFont;
 Fn<void(bool)> setNotifications;
 QString versionNotice;
};

class TelegramProfileOverlay final : public Ui::RpWidget {
public:
 TelegramProfileOverlay(QWidget *parent,QJsonObject profile,
     Fn<bool(QJsonObject)> save,TelegramSettingsActions actions={})
 :Ui::RpWidget(parent),previousFocus_(QApplication::focusWidget()),layers_(this) {
  setObjectName("telegramSettingsOverlay");
  parent->installEventFilter(this);setGeometry(parent->rect());show();raise();
  layers_.setHideByBackgroundClick(false);
  auto box=object_ptr<Ui::GenericBox>(this,
   [boxWidth=availableBoxWidth(parent->width()),profile=std::move(profile),save=std::move(save),actions=std::move(actions)](not_null<Ui::GenericBox*> box) mutable {
    box->setTitle(rpl::single(QString("设置")));box->setWidth(boxWidth);
    if(actions.chooseFont) {
     auto button=box->addRow(object_ptr<Ui::SettingsButton>(box,rpl::single(QString("聊天字体和字号…"))));
     button->setObjectName("telegramChooseFont");
     button->setClickedCallback(std::move(actions.chooseFont));
    }
    if(actions.setNotifications) {
     auto check=box->addRow(object_ptr<Ui::Checkbox>(box,QString("新消息桌面提醒和提示音"),actions.notifications));
     check->setObjectName("telegramNotifications");
     check->checkedChanges() | rpl::on_next(std::move(actions.setNotifications),check->lifetime());
    }
    if(!actions.versionNotice.isEmpty())
     box->addRow(object_ptr<Ui::FlatLabel>(box,actions.versionNotice,st::boxLabel));
    std::map<QString,QPointer<Ui::InputField>> fields;
    const QList<QPair<QString,QString>> names={{"nickname","昵称"},{"signature","签名"},
      {"organization","组织"},{"department","部门"},{"location","所在地"}};
    for(const auto &[key,title]:names) {
     auto field=box->addRow(object_ptr<Ui::InputField>(box,st::defaultInputField,
       Ui::InputField::Mode::SingleLine,rpl::single(title),profile[key].toString()));
     field->setObjectName("telegramProfile_"+key);
     field->setMaxLength(key=="signature"?200:80);
     fields[key]=field;
    }
    box->addRow(object_ptr<Ui::FlatLabel>(box,QString(
      "昵称用于局域网发现，以上资料可由局域网联系人请求查看。不要填写敏感信息。"),
      st::boxLabel));
    QPointer<Ui::GenericBox> weak(box.get());
    auto saveButton=box->addButton(rpl::single(QString("保存")),[fields,save=std::move(save),weak]{
     if(!weak)return;
     QJsonObject result;for(const auto &[key,field]:fields){if(!field)return;result[key]=field->getLastText();}
     // The caller performs the existing Node validation and persistence.
     if(save && save(result) && weak)weak->closeBox();
    });
    saveButton->setObjectName("telegramProfileSave");
    auto cancelButton=box->addButton(rpl::single(QString("取消")),[weak]{if(weak)weak->closeBox();});
    cancelButton->setObjectName("telegramProfileCancel");
    const auto first=fields["nickname"];
    box->setFocusCallback([first]{if(first)first->setFocus();});
   });
  box->setObjectName("telegramSettingsBox");
  box_=box.data();
  layers_.showBox(std::move(box),Ui::LayerOption::KeepOther,anim::type::instant);
  // Subscribe after showBox, so the initial false state cannot delete this host.
  layers_.layerShownValue() | rpl::on_next([this](bool shown){
   if(!shown){hide();if(previousFocus_ && previousFocus_->isVisible())previousFocus_->setFocus();deleteLater();}
  },lifetime());
  layers_.setFocus();
 }
 ~TelegramProfileOverlay() override = default;
protected:
 bool eventFilter(QObject *watched,QEvent *event) override {
  if(watched==parentWidget() && event->type()==QEvent::Resize) {
   setGeometry(parentWidget()->rect());resizeBox();
  }
  return Ui::RpWidget::eventFilter(watched,event);
 }
private:
 static int availableBoxWidth(int parentWidth) {return qMax(1,qMin(440,parentWidth-32));}
 void resizeBox() {
  if(!box_ || !box_->hasDelegate())return;
  const auto target=availableBoxWidth(width());
  if(box_->width()==target)return;
  box_->setWidth(target);
  // Resize the original GenericBox content wrapper so VerticalLayout and
  // FlatLabel recalculate their own heights, then ask the original delegate
  // to center/clamp the box. No custom positioning of rows or buttons.
  auto layout=box_->verticalLayout();
  auto *wrapper=dynamic_cast<Ui::RpWidget*>(layout->parentWidget());
  if(wrapper)wrapper->resizeToWidth(target);
  else layout->resizeToWidth(target);
  box_->getDelegate()->setDimensions(target,wrapper?wrapper->height():layout->height(),true);
  // setDimensions recenters vertically only; the earlier host resize used
  // the old width. Let the original layer recompute horizontal centering.
  if(auto *layer=dynamic_cast<Ui::BoxLayerWidget*>(box_->parentWidget()))layer->parentResized();
 }
 QPointer<Ui::GenericBox> box_;
 QPointer<QWidget> previousFocus_;
 Ui::LayerManager layers_;
};

inline TelegramProfileOverlay *telegramShowProfileBox(QWidget *parent,
    QJsonObject profile,Fn<bool(QJsonObject)> save,TelegramSettingsActions actions={}) {
 ensureTelegramUiRuntime();
 for(auto *child:parent->findChildren<QWidget*>(QString("telegramSettingsOverlay"),Qt::FindDirectChildrenOnly))
  if(auto *existing=dynamic_cast<TelegramProfileOverlay*>(child);existing && existing->isVisible()) {
   existing->raise();return existing;
  }
 return new TelegramProfileOverlay(parent,std::move(profile),std::move(save),std::move(actions));
}
