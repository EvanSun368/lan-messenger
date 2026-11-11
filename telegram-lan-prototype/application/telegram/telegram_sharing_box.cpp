#include "telegram_sharing_box.h"
#include "telegram_contact_boxes.h"
#include "ui/widgets/scroll_area.h"
#include <QResizeEvent>
namespace TelegramLan {
namespace {
Ui::LinkButton *button(QWidget *parent,const QString &name,const QString &text,std::function<void()> action) {
 auto *result=new Ui::LinkButton(parent,text);result->setObjectName(name);
 result->setFixedHeight(28);result->setMinimumWidth(qMax(50,result->width()));
 result->setClickedCallback(std::move(action));return result;
}
Ui::FlatLabel *label(QWidget *parent,QString text) {
 auto *result=new Ui::FlatLabel(parent,text,st::boxLabel);result->setMinimumHeight(22);
 return result;
}
// The original ScrollArea owns each original VerticalLayout. Their resize-to-
// width flow performs actual FlatLabel wrapping and keeps long paths bounded.
Ui::VerticalLayout *list(Ui::ScrollArea *scroll) {
 auto *content=new Ui::VerticalLayout(scroll);
 scroll->setOwnedWidget(object_ptr<Ui::VerticalLayout>::fromRaw(content));
 scroll->setWidgetResizable(true);return content;
}
}
SharingPane::SharingPane(QWidget *parent,SharingActions actions):Ui::RpWidget(parent),actions_(std::move(actions)) {
 setObjectName("telegramSharingPane");
 layout_=new Ui::VerticalLayout(this);
 auto add=[this](Ui::RpWidget *widget){layout_->add(object_ptr<Ui::RpWidget>::fromRaw(widget),style::margins(0,0,0,6));};
 // ScrollArea wraps QScrollArea, not RpWidget. The original VerticalLayout
 // receives a fixed-height RpWidget host whose size drives the real scroller.
 auto addScroll=[this,&add](int height) {
  auto *host=new Ui::RpWidget(this);host->setFixedHeight(height);
  auto *scroll=new Ui::ScrollArea(host);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  host->sizeValue() | rpl::on_next([host,scroll](QSize){
   scroll->setGeometry(host->rect());
   if(auto *content=dynamic_cast<Ui::VerticalLayout*>(scroll->widget()))
    content->resizeToWidth(qMax(1,scroll->viewport()->width()));
  },host->lifetime());
  list(scroll);add(host);return scroll;
 };
 add(label(this,"我发布的目录（本次运行）"));
 add(button(this,"originalSharePublish","发布目录…",[this]{const auto cb=actions_.publish;if(cb)cb();}));
 rootsScroll_=addScroll(100);
 add(label(this,"浏览当前联系人共享"));
 token_=new Ui::InputField(this,st::defaultInputField,Ui::InputField::Mode::SingleLine,rpl::single(QString("对方提供的访问码")));
 token_->setObjectName("originalShareToken");token_->setMaxLength(256);token_->setFixedHeight(56);add(token_);
 directory_=new Ui::InputField(this,st::defaultInputField,Ui::InputField::Mode::SingleLine,rpl::single(QString("相对目录（留空为根目录）")));
 directory_->setObjectName("originalShareDirectory");directory_->setMaxLength(4096);directory_->setFixedHeight(56);add(directory_);
 auto changed=[this]{const auto callback=actions_.inputsChanged;if(callback)callback(tokenText(),directoryText());};
 token_->changes() | rpl::on_next(changed,lifetime());
 directory_->changes() | rpl::on_next(changed,lifetime());
 add(button(this,"originalShareBrowse","浏览当前联系人",[this]{const auto cb=actions_.browse;if(cb)cb(tokenText().trimmed(),directoryText().trimmed());}));
 location_=label(this,"尚未读取共享目录");location_->setObjectName("originalShareLocation");add(location_);
 remoteScroll_=addScroll(145);
 layout_->heightValue() | rpl::on_next([this](int height){resize(width(),height);},lifetime());
 resizeToWidth(qMax(1,parent->width()));
}
int SharingPane::resizeGetHeight(int width) {
 layout_->resizeToWidth(qMax(1,width));layout_->move(0,0);return layout_->height();
}
QString SharingPane::tokenText()const{return token_->getLastText();}
QString SharingPane::directoryText()const{return directory_->getLastText();}
void SharingPane::setInputs(QString token,QString directory){token_->setText(token);directory_->setText(directory);}
void SharingPane::setRoots(QJsonArray roots) {
 auto *content=list(rootsScroll_);
 for(const auto &value:roots) {
  const auto root=value.toObject();const auto token=root["token"].toString();if(token.isEmpty())continue;
  content->add(object_ptr<Ui::FlatLabel>(content,root["path"].toString()+"\n访问码："+token,st::boxLabel));
  auto *copy=button(content,"originalShareCopy","复制访问码",[this,token]{const auto cb=actions_.copy;if(cb)cb(token);});
  copy->setProperty("shareToken",token);content->add(object_ptr<Ui::LinkButton>::fromRaw(copy));
  auto *revoke=button(content,"originalShareRevoke","撤销共享",[this,token]{const auto cb=actions_.revoke;if(cb)cb(token);});
  revoke->setProperty("shareToken",token);content->add(object_ptr<Ui::LinkButton>::fromRaw(revoke));
 }
 content->resizeToWidth(qMax(1,rootsScroll_->viewport()->width()));
}
void SharingPane::setLoading(QString peer,QString token,QString directory) {
 peer_=std::move(peer);requestToken_=std::move(token);requestDirectory_=std::move(directory);
 location_->setText("正在读取共享目录…");list(remoteScroll_);
}
bool SharingPane::setRemote(QString peer,QString token,QString directory,QJsonArray entries,QString caption) {
 if(peer!=peer_||token!=requestToken_||directory!=requestDirectory_)return false;
 location_->setText(caption);auto *content=list(remoteScroll_);
 for(const auto &value:entries) {
  const auto entry=value.toObject();const auto name=entry["name"].toString();
  const bool isDir=entry["directory"].toBool()||entry["is_dir"].toBool()||entry["type"]=="directory";
  const auto relative=directory.isEmpty()?name:directory+"/"+name;
  content->add(object_ptr<Ui::FlatLabel>(content,(isDir?QString("[目录] "):QString())+name,st::boxLabel));
  // Freeze the accepted response context in each action. Edited inputs and
  // subsequent main-window conversation changes cannot retarget this row.
  auto *action=button(content,isDir?"originalShareEnter":"originalShareDownload",isDir?"进入目录":"下载文件…",[this,peer,token,relative,isDir]{
   const auto cb=isDir?actions_.browseFrozen:actions_.downloadFrozen;if(cb)cb(peer,token,relative);
  });
  action->setProperty("relativePath",relative);content->add(object_ptr<Ui::LinkButton>::fromRaw(action));
 }
 content->resizeToWidth(qMax(1,remoteScroll_->viewport()->width()));return true;
}
QPointer<SharingPane> ShowSharingBox(QWidget *parent,QJsonArray roots,SharingActions actions) {
 ensureTelegramUiRuntime();
 for(auto *widget:parent->findChildren<QWidget*>("telegramSharingOverlay",Qt::FindDirectChildrenOnly)) {
  if(auto *host=dynamic_cast<TelegramContactBoxHost*>(widget);host&&host->isVisible()) {
   host->raise();for(auto *child:host->findChildren<QWidget*>("telegramSharingPane"))
    if(auto *pane=dynamic_cast<SharingPane*>(child)){pane->setRoots(roots);return pane;}
  }
 }
 auto result=std::make_shared<QPointer<SharingPane>>();
 new TelegramContactBoxHost(parent,"telegramSharingOverlay",[roots=std::move(roots),actions=std::move(actions),result](not_null<Ui::GenericBox*> box)mutable{
  box->setTitle(rpl::single(QString("只读文件共享")));
  box->addRow(object_ptr<Ui::FlatLabel>(box,QString("访问码持有者可读取已发布目录。仅限可信局域网，撤销共享将终止相关下载。"),st::boxLabel));
  auto pane=box->addRow(object_ptr<SharingPane>(box,std::move(actions)));*result=pane;pane->setRoots(roots);
  QPointer<Ui::GenericBox> weak(box.get());auto close=box->addButton(rpl::single(QString("关闭")),[weak]{if(weak)weak->closeBox();});close->setObjectName("originalShareClose");
 });
 return *result;
}
}
