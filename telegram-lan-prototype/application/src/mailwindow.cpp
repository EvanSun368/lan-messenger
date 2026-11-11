#include "mailwindow.h"
#include "mailmime.h"
#include <QtWidgets>
#include <algorithm>
#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_mail_widgets.h"
#include "telegram_dialogs_inner.h"
#endif

namespace {
QLabel *plainLabel(QString text={}) {auto *l=new QLabel(text);l->setTextFormat(Qt::PlainText);l->setWordWrap(true);return l;}
QString storageDirectory(QString supplied) {
 return supplied.isEmpty()?QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("mail"):supplied;
}
class AccountDialog final:public QDialog {
public:
 AccountDialog(MailAccounts *store,const MailAccount &old,QWidget *parent):QDialog(parent) {
  setObjectName("mailAccountDialog");setWindowTitle(old.id.isEmpty()?"添加邮箱账户":"编辑账户名称 / 凭据");resize(560,650);
  auto *layout=new QVBoxLayout(this);auto *form=new QFormLayout;layout->addLayout(form);
  auto *provider=new QComboBox;provider->setObjectName("mailProvider");const auto providers=mailProviderTemplates();
  for(const auto &p:providers){provider->addItem(p.available?p.label:p.label+"（不可用）",p.id);
   if(!p.available)if(auto *model=qobject_cast<QStandardItemModel*>(provider->model()))model->item(provider->count()-1)->setEnabled(false);}
  form->addRow("邮箱模板",provider);
  auto field=[&](QString name,QString label,QString value={}){auto *f=new QLineEdit(value);f->setObjectName(name);form->addRow(label,f);return f;};
  auto *label=field("mailAccountLabel","账户名称",old.label);
  auto *address=field("mailAccountAddress","邮箱地址",old.address);
  auto *user=field("mailAccountUser","登录用户名",old.userName);
  auto *ihost=field("mailImapHost","IMAP 服务器",old.imap.host);
  auto *iport=new QSpinBox;iport->setObjectName("mailImapPort");iport->setRange(1,65535);form->addRow("IMAP 端口",iport);
  auto security=[&](QString name,QString title){auto *c=new QComboBox;c->setObjectName(name);c->addItem("TLS（连接即加密）",0);c->addItem("STARTTLS（升级后认证）",1);form->addRow(title,c);return c;};
  auto *itls=security("mailImapTls","IMAP 加密");
  auto *shost=field("mailSmtpHost","SMTP 服务器",old.smtp.host);
  auto *sport=new QSpinBox;sport->setObjectName("mailSmtpPort");sport->setRange(1,65535);form->addRow("SMTP 端口",sport);
  auto *stls=security("mailSmtpTls","SMTP 加密");
  auto *client=field("mailOAuthClientId","OAuth 客户端 ID",old.oauthClientId);
  auto *tenant=field("mailOAuthTenant","Microsoft 租户",old.oauthTenant);
  auto *clientSecret=field("mailOAuthClientSecret","Google 客户端密钥（若要求）");clientSecret->setEchoMode(QLineEdit::Password);clientSecret->setPlaceholderText("仅存系统凭据库；Microsoft 公共客户端无需密钥");
  auto *secret=field("mailAccountSecret","授权码 / 密码");secret->setEchoMode(QLineEdit::Password);
  secret->setPlaceholderText(old.id.isEmpty()?"可稍后设置；不会写入账户配置文件":"留空保留已有系统凭据");
  secret->setEnabled(store->credentialStorageAvailable());
  auto *notice=plainLabel();notice->setObjectName("mailProviderNotice");layout->addWidget(notice);
  auto *storage=plainLabel(store->credentialStorageAvailable()?"凭据仅存入系统凭据库。勾选自动同步后，将在保存凭据或完成授权后连接收件箱。":store->credentialStorageProblem()+"。仅可保存非敏感配置，不能连接邮箱。");layout->addWidget(storage);
  auto *sync=new QCheckBox("配置完成后每 2 分钟同步收件箱并提醒新邮件");sync->setObjectName("mailAccountAutoSync");sync->setChecked(true);layout->addWidget(sync);
  auto *error=plainLabel();error->setObjectName("mailAccountError");error->setStyleSheet("color:#b42318;");layout->addWidget(error);
  auto apply=[=](int n){if(n<0||n>=providers.size())return;const auto &p=providers[n];notice->setText(p.notice);secret->clear();
   ihost->setText(p.imap.host);iport->setValue(p.imap.port);itls->setCurrentIndex(p.imap.tls==MailTls::StartTls?1:0);
   shost->setText(p.smtp.host);sport->setValue(p.smtp.port);stls->setCurrentIndex(p.smtp.tls==MailTls::StartTls?1:0);
   const bool oauth=p.auth==MailAuth::OAuth2;secret->setEnabled(!oauth&&store->credentialStorageAvailable());
   client->setEnabled(oauth);tenant->setEnabled(p.id=="outlook"||p.id=="outlook-personal");if(old.id.isEmpty())tenant->setText(p.id=="outlook-personal"?"consumers":"common");clientSecret->setEnabled(p.id=="gmail"&&store->credentialStorageAvailable());clientSecret->clear();
   ihost->setReadOnly(oauth);shost->setReadOnly(oauth);for(auto *w:QList<QWidget*>{iport,itls,sport,stls})w->setEnabled(!oauth);
  };
  connect(provider,&QComboBox::currentIndexChanged,this,apply);
  for(auto *f:{address,user,ihost,shost})connect(f,&QLineEdit::textEdited,secret,&QLineEdit::clear);
  if(old.id.isEmpty()){apply(0);connect(address,&QLineEdit::textEdited,this,[=](const QString &s){if(!user->isModified())user->setText(s);if(!label->isModified())label->setText(s);});}
  else {
   const int index=provider->findData(old.provider);provider->setCurrentIndex(index);apply(index);client->setReadOnly(true);tenant->setReadOnly(true);
   ihost->setText(old.imap.host);iport->setValue(old.imap.port);itls->setCurrentIndex(old.imap.tls==MailTls::StartTls?1:0);
   shost->setText(old.smtp.host);sport->setValue(old.smtp.port);stls->setCurrentIndex(old.smtp.tls==MailTls::StartTls?1:0);
   for(auto *f:{address,user,ihost,shost})f->setReadOnly(true);
   for(auto *w:QList<QWidget*>{provider,iport,itls,sport,stls})w->setEnabled(false);
   layout->addWidget(plainLabel("更换登录身份或服务器请添加新账户，避免将已有凭据误用于其他服务器。"));
  }
  auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Save)->setText("保存配置");buttons->button(QDialogButtonBox::Save)->setObjectName("mailAccountSave");layout->addWidget(buttons);
  connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
  connect(buttons,&QDialogButtonBox::accepted,this,[=]{
   MailAccount account=old.id.isEmpty()?mailAccountFromTemplate(provider->currentData().toString(),address->text().trimmed()):old;
   account.label=label->text().trimmed();account.address=address->text().trimmed();account.userName=user->text().trimmed();
   account.imap={ihost->text().trimmed(),iport->value(),itls->currentIndex()?MailTls::StartTls:MailTls::ImplicitTls};
   account.smtp={shost->text().trimmed(),sport->value(),stls->currentIndex()?MailTls::StartTls:MailTls::ImplicitTls};
   account.oauthClientId=client->text().trimmed();account.oauthTenant=tenant->text().trimmed();
   QString problem;if(!store->upsert(account,&problem)){error->setText(problem);return;}
   if(account.auth==MailAuth::OAuth2 && !clientSecret->text().isEmpty()){
    auto bytes=MailOAuth::credential(account,{},clientSecret->text());const bool saved=store->saveCredential(account.id,bytes,&problem);bytes.fill('\0');clientSecret->clear();
    if(!saved){error->setText("配置已保存，但客户端凭据保存失败："+problem);return;}
   }
   if(account.auth==MailAuth::Password && !secret->text().isEmpty()) {auto bytes=secret->text().toUtf8();const bool saved=store->saveCredential(account.id,bytes,&problem);bytes.fill('\0');secret->clear();
    if(!saved){error->setText("账户配置已保存，但凭据保存失败，尚不能连接："+problem);return;}}
   setProperty("savedAccountId",account.id);setProperty("autoSync",sync->isChecked());accept();
  });
 }
};
}

MailWindow::MailWindow(QString directory,QWidget *parent):MailWindow(std::move(directory),{},nullptr,parent) {}
MailWindow::MailWindow(QString directory,std::shared_ptr<MailCredentialStore> credentials,MailTransport *transport,QWidget *parent)
 :QDialog(parent),directory_(storageDirectory(std::move(directory))) {
 setObjectName("mailWindow");setWindowTitle("LanLink · 邮箱");resize(1120,760);setMinimumSize(820,600);
 const bool directoryReady=QDir().mkpath(directory_);
 accounts_=std::make_unique<MailAccounts>(QDir(directory_).filePath("accounts.json"),std::move(credentials));
 syncState_=std::make_unique<MailSyncState>(QDir(directory_).filePath("sync-cursors.json"));
 transport_=transport?transport:new MailTransport(this);oauth_=new MailOAuth(this);buildUi();
 connect(oauth_,&MailOAuth::authorizationRequired,this,[this](const QUrl &url){if(!QDesktopServices::openUrl(url)){oauth_->cancel();transportFailed("无法打开系统浏览器，授权已取消。请检查默认浏览器。");}});
 connect(oauth_,&MailOAuth::failed,this,[this](QString error){oauthReady_={};transportFailed(error);});
 connect(oauth_,&MailOAuth::authorized,this,[this](QString token,QByteArray credential){
  if(operation_==Operation::None){token.fill(QChar(0));credential.fill('\0');return;}
  QString error;const bool saved=accounts_->saveCredential(operationAccountId_,credential,&error);credential.fill('\0');
  if(!saved){token.fill(QChar(0));oauthReady_={};transportFailed("OAuth 刷新令牌无法安全保存，未连接邮件服务器："+error);return;}
  auto ready=std::move(oauthReady_);oauthReady_={};
  if(ready)ready(token);else{const auto id=operationAccountId_;finishOperation(true);status("浏览器授权完成。已启用的账户将自动同步收件箱。");nextSync_[id]=0;QTimer::singleShot(0,this,&MailWindow::pollDue);}
  token.fill(QChar(0));
 });
 connect(transport_,&MailTransport::finished,this,&MailWindow::transportFinished);
 connect(transport_,&MailTransport::failed,this,&MailWindow::transportFailed);
 connect(transport_,&MailTransport::messageFetched,this,&MailWindow::messageFetched);
 connect(transport_,&QObject::destroyed,this,[this]{operation_=Operation::None;oauthReady_={};oauth_->cancel();syncTimer_.stop();updateAvailability();status("邮件服务已停止",true);});
 QString error;const bool loaded=accounts_->load(&error);refreshAccounts();loadPreferences();
 QString cursorError;if(!syncState_->load(&cursorError))status("同步记录不可读取；本次首次同步将静默建立基线。",true);
 if(!directoryReady)status("无法创建邮件配置目录，不能保存账户",true);else if(!loaded)status(error,true);
 else if(cursorError.isEmpty())status("配置账户后可自动同步。首次同步不提醒历史邮件，邮件内容仅在本机安全显示。");
 connect(&syncTimer_,&QTimer::timeout,this,&MailWindow::pollDue);syncTimer_.start(1000);
 connect(qApp,&QCoreApplication::aboutToQuit,this,[this]{shuttingDown_=true;syncTimer_.stop();cancelOperation();});
 QTimer::singleShot(0,this,&MailWindow::pollDue);
}
MailWindow::~MailWindow(){shuttingDown_=true;syncTimer_.stop();oauthReady_={};if(oauth_){disconnect(oauth_,nullptr,this,nullptr);oauth_->cancel();}if(transport_){disconnect(transport_,nullptr,this,nullptr);transport_->cancel();}}
void MailWindow::reject(){close();}
bool MailWindow::confirmClose(){
 if(hasDraft()||(isBusy()&&!background_)){
  QMessageBox box(QMessageBox::Warning,"确认关闭邮箱","当前草稿和附件选择未保存。关闭将清除本次运行中的所有账户草稿。若正在发送，取消等待不代表未发送，结果可能未知，请核对服务器后再决定是否重发。是否继续？",QMessageBox::Yes|QMessageBox::No,this);
  box.setObjectName("mailCloseConfirmation");box.setTextFormat(Qt::PlainText);box.setDefaultButton(QMessageBox::No);return box.exec()==QMessageBox::Yes;
 }return true;
}
void MailWindow::closeEvent(QCloseEvent *event){
 if(!confirmClose()){event->ignore();return;}cancelOperation();drafts_.clear();restoreDraft({});clearReceived();
 // Closing the pane hides it; configured polling belongs to the running app.
 // shutdown()/aboutToQuit is the explicit network-lifecycle boundary.
 setResult(QDialog::Rejected);event->accept();
}
bool MailWindow::shutdown(){if(!confirmClose())return false;shuttingDown_=true;syncTimer_.stop();cancelOperation();if(tray_)tray_->hide();drafts_.clear();restoreDraft({});hide();return true;}
void MailWindow::buildUi(){
 setStyleSheet("QDialog#mailWindow{background:#ffffff;color:#222222;} QLabel#mailStatus{color:#82909c;padding:8px;} QLabel#mailCredentialNotice{color:#82909c;} QTabWidget::pane{border:0;} QTabBar::tab{padding:12px 20px;border:0;color:#82909c;} QTabBar::tab:selected{color:#3390ec;border-bottom:3px solid #3390ec;} QSplitter::handle{background:#e7e7e7;} QListWidget,QTreeWidget,QPlainTextEdit{border:0;background:white;selection-background-color:#419fd9;} QLineEdit{border:0;border-bottom:1px solid #dedede;padding:10px;} QPushButton{border:0;color:#3390ec;padding:8px;background:#f2f8fc;border-radius:4px;} QPushButton:disabled{color:#a0a8ae;} QCheckBox{color:#63758a;} QComboBox{padding:4px;border:1px solid #e7e7e7;}");
 auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(0);
 auto action=[this](QString name,QString text,bool primary=false){auto *button=new QPushButton(text,this);button->setObjectName(name);return button;};
 auto visibleAction=[](QPushButton *b,bool primary=false)->QWidget*{
#ifdef LANLINK_FULL_TELEGRAM_UI
  return telegramMailAction(b,primary);
#else
  Q_UNUSED(primary);return b;
#endif
 };
 auto visibleInput=[](QLineEdit *f)->QWidget*{
#ifdef LANLINK_FULL_TELEGRAM_UI
  return telegramMailInput(f);
#else
  return f;
#endif
 };
 auto visibleText=[](QPlainTextEdit *f)->QWidget*{
#ifdef LANLINK_FULL_TELEGRAM_UI
  return telegramMailText(f);
#else
  return f;
#endif
 };
 auto *split=new QSplitter(this);split->setHandleWidth(1);split->setChildrenCollapsible(false);layout->addWidget(split,1);
 auto *left=new QWidget;left->setMinimumWidth(235);left->setMaximumWidth(340);auto *ll=new QVBoxLayout(left);ll->setContentsMargins(14,18,14,12);
 auto *title=plainLabel("邮箱");QFont titleFont=font();titleFont.setPixelSize(21);titleFont.setBold(true);title->setFont(titleFont);ll->addWidget(title);
 ll->addWidget(plainLabel("账户与同步"));accountList_=new QListWidget(left);accountList_->setObjectName("mailAccounts");accountList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
#ifdef LANLINK_FULL_TELEGRAM_UI
 ensureTelegramUiRuntime();auto *accountsView=new TelegramLan::DialogsView(left,accountList_->model(),accountList_->selectionModel(),[this](const QModelIndex &i)->std::optional<TelegramLan::DialogPresentation>{
  const auto a=accountById(i.data(Qt::UserRole).toString());TelegramLan::DialogPresentation row;row.id=a.id;row.name=a.label;row.preview=a.address;row.unread=inboxes_.value(a.id).unread.size();row.avatarColor=QColor("#65aadd");return row;});
 accountsView->setObjectName("telegramMailAccounts");accountList_->hide();accountView_=accountsView;
#else
 accountView_=accountList_;
#endif
 ll->addWidget(accountView_,1);auto *ab=new QHBoxLayout;addAccount_=action("mailAddAccount","添加");editAccount_=action("mailEditAccount","设置");removeAccount_=action("mailRemoveAccount","移除");for(auto *b:{addAccount_,editAccount_,removeAccount_})ab->addWidget(visibleAction(b));ll->addLayout(ab);
 autoSync_=new QCheckBox("自动同步此账户",left);autoSync_->setObjectName("mailAutoSync");ll->addWidget(autoSync_);
 interval_=new QComboBox(left);interval_->setObjectName("mailSyncInterval");for(auto minutes:{1,2,5,15})interval_->addItem(QString("每 %1 分钟检查一次").arg(minutes),minutes*60000);interval_->setCurrentIndex(1);ll->addWidget(interval_);
 notify_=new QCheckBox("新邮件提醒（不显示正文）",left);notify_->setObjectName("mailNotifications");notify_->setChecked(true);ll->addWidget(notify_);
 credentialNotice_=plainLabel(accounts_->credentialStorageAvailable()?"凭据保存在系统凭据库。关闭此窗口后，已启用的同步仍在后台运行。":accounts_->credentialStorageProblem()+"。可配置账户，但不能连接邮箱。");credentialNotice_->setObjectName("mailCredentialNotice");ll->addWidget(credentialNotice_);split->addWidget(left);
 auto *right=new QWidget;auto *rl=new QVBoxLayout(right);rl->setContentsMargins(18,14,18,6);sender_=plainLabel("添加邮箱账户，即可收取和回复邮件");sender_->setObjectName("mailSelectedAccount");sender_->setMinimumHeight(32);rl->addWidget(sender_);
 auto *toolbar=new QHBoxLayout;refresh_=action("mailRefresh","同步");read_=action("mailRead","查看");reply_=action("mailReply","回复");cancel_=action("mailCancel","取消");authorize_=action("mailAuthorize","浏览器授权");for(auto *b:{refresh_,read_,reply_})toolbar->addWidget(visibleAction(b));toolbar->addStretch();toolbar->addWidget(visibleAction(authorize_));toolbar->addWidget(visibleAction(cancel_));rl->addLayout(toolbar);
 tabs_=new QTabWidget;tabs_->setObjectName("mailTabs");rl->addWidget(tabs_,1);auto *incoming=new QWidget;auto *il=new QVBoxLayout(incoming);il->setContentsMargins(0,6,0,0);
 inbox_=new QTreeWidget(incoming);inbox_->setObjectName("mailInbox");inbox_->setHeaderLabels({"发件人","主题"});inbox_->setRootIsDecorated(false);inbox_->setUniformRowHeights(true);
#ifdef LANLINK_FULL_TELEGRAM_UI
 auto *mailView=new TelegramLan::DialogsView(incoming,inbox_->model(),inbox_->selectionModel(),[this](const QModelIndex &i)->std::optional<TelegramLan::DialogPresentation>{
  const auto uid=i.data(Qt::UserRole).toString();const auto box=inboxes_.value(selectedId_);const auto s=box.summaries.value(uid);TelegramLan::DialogPresentation row;
  row.id=uid;row.name=s.from.isEmpty()?QString("邮件 %1").arg(uid):s.from;row.preview=s.subject.isEmpty()?QString("点击查看邮件"):s.subject;row.date=s.date.left(16);row.unread=box.unread.contains(uid)?1:0;row.avatarColor=QColor("#68b5ae");return row;});
 mailView->setObjectName("telegramMailInbox");mailView->activated=[this](const QModelIndex&){fetchSelected();};inbox_->hide();inboxView_=mailView;
#else
 inboxView_=inbox_;
#endif
 inboxView_->setMinimumHeight(140);inboxView_->setMaximumHeight(260);il->addWidget(inboxView_,2);
 messageInfo_=plainLabel("选择邮件，查看正文或回复。首次同步仅建立基线，不提醒历史邮件。");messageInfo_->setObjectName("mailMessageInfo");messageInfo_->setMinimumHeight(messageInfo_->fontMetrics().lineSpacing()*3+8);il->addWidget(messageInfo_);
 reader_=new QPlainTextEdit(incoming);reader_->setObjectName("mailReader");reader_->setReadOnly(true);reader_->setPlaceholderText("邮件安全纯文本预览，不加载外部图片、脚本或链接。");il->addWidget(visibleText(reader_),3);
 receivedAttachments_=new QListWidget;receivedAttachments_->setObjectName("mailReceivedAttachments");receivedAttachments_->setMaximumHeight(65);il->addWidget(receivedAttachments_);
 saveAttachment_=action("mailSaveAttachment","保存附件…");il->addWidget(visibleAction(saveAttachment_));tabs_->addTab(incoming,"收件箱");
 auto *compose=new QWidget;auto *cl=new QVBoxLayout(compose);auto *form=new QFormLayout;recipients_=new QLineEdit(compose);recipients_->setObjectName("mailRecipients");recipients_->setPlaceholderText("收件人邮箱，多个地址以分号分隔");subject_=new QLineEdit(compose);subject_->setObjectName("mailSubject");subject_->setPlaceholderText("主题");form->addRow("收件人",visibleInput(recipients_));form->addRow("主题",visibleInput(subject_));cl->addLayout(form);
 body_=new QPlainTextEdit(compose);body_->setObjectName("mailBody");body_->setPlaceholderText("撰写邮件…  发送前将确认账户、收件人与内容");cl->addWidget(visibleText(body_),1);
 attachmentList_=new QListWidget;attachmentList_->setObjectName("mailAttachments");attachmentList_->setMaximumHeight(90);cl->addWidget(attachmentList_);
 auto *files=new QHBoxLayout;attach_=action("mailAttach","添加附件");removeAttachment_=action("mailRemoveAttachment","移除附件");files->addWidget(visibleAction(attach_));files->addWidget(visibleAction(removeAttachment_));files->addStretch();send_=action("mailSend","发送");files->addWidget(visibleAction(send_,true));cl->addLayout(files);tabs_->addTab(compose,"写邮件");split->addWidget(right);split->setSizes({270,850});
 status_=plainLabel();status_->setObjectName("mailStatus");status_->setMinimumHeight(40);layout->addWidget(status_);
 connect(addAccount_,&QPushButton::clicked,this,[this]{editAccount(false);});connect(editAccount_,&QPushButton::clicked,this,[this]{editAccount(true);});connect(removeAccount_,&QPushButton::clicked,this,&MailWindow::removeAccount);
 connect(accountList_,&QListWidget::currentRowChanged,this,[this]{accountSelected();});connect(refresh_,&QPushButton::clicked,this,&MailWindow::refreshInbox);connect(read_,&QPushButton::clicked,this,&MailWindow::fetchSelected);connect(reply_,&QPushButton::clicked,this,&MailWindow::replySelected);
 connect(inbox_,&QTreeWidget::itemDoubleClicked,this,[this]{fetchSelected();});connect(inbox_,&QTreeWidget::itemSelectionChanged,this,&MailWindow::updateAvailability);connect(authorize_,&QPushButton::clicked,this,&MailWindow::authorizeAccount);
 connect(send_,&QPushButton::clicked,this,&MailWindow::sendDraft);connect(cancel_,&QPushButton::clicked,this,&MailWindow::cancelOperation);connect(attach_,&QPushButton::clicked,this,&MailWindow::addAttachments);connect(saveAttachment_,&QPushButton::clicked,this,&MailWindow::saveReceivedAttachment);connect(receivedAttachments_,&QListWidget::itemSelectionChanged,this,&MailWindow::updateAvailability);
 connect(removeAttachment_,&QPushButton::clicked,this,[this]{const int row=attachmentList_->currentRow();if(row<0)return;attachmentPaths_.removeAt(row);delete attachmentList_->takeItem(row);});
 connect(autoSync_,&QCheckBox::toggled,this,[this](bool enabled){if(preferencesLoading_||selectedId_.isEmpty())return;if(enabled)syncEnabled_.insert(selectedId_);else{syncEnabled_.remove(selectedId_);if(headerAccount_==selectedId_)headerQueue_.clear();if(background_&&operationAccountId_==selectedId_)cancelOperation();}nextSync_[selectedId_]=0;savePreferences();QTimer::singleShot(0,this,&MailWindow::pollDue);});
 connect(interval_,&QComboBox::currentIndexChanged,this,[this]{if(preferencesLoading_)return;pollIntervalMs_=interval_->currentData().toInt();nextSync_.clear();savePreferences();});
 connect(notify_,&QCheckBox::toggled,this,[this]{if(!preferencesLoading_)savePreferences();});
}
void MailWindow::status(QString text,bool error){status_->setText(text);status_->setStyleSheet(error?"color:#b42318;":"color:#63758a;");}
MailAccount MailWindow::accountById(QString id)const{for(const auto &a:accounts_->accounts())if(a.id==id)return a;return {};}
MailAccount MailWindow::currentAccount()const{return accountById(selectedId_);}
void MailWindow::refreshAccounts(QString selected){if(selected.isEmpty())selected=selectedId_;QSignalBlocker blocker(accountList_);accountList_->clear();int row=-1;for(const auto &a:accounts_->accounts()){auto *item=new QListWidgetItem(a.label+"\n"+a.address,accountList_);item->setData(Qt::UserRole,a.id);if(a.id==selected)row=accountList_->count()-1;}accountList_->setCurrentRow(row>=0?row:(accountList_->count()?0:-1));accountSelected();}
void MailWindow::saveDraft(QString id){drafts_[id]={recipients_->text(),subject_->text(),body_->toPlainText(),attachmentPaths_,draftReplyTo_,draftReferences_};}
void MailWindow::restoreDraft(QString id){const auto draft=drafts_.value(id);recipients_->setText(draft.recipients);subject_->setText(draft.subject);body_->setPlainText(draft.body);attachmentPaths_=draft.paths;draftReplyTo_=draft.inReplyTo;draftReferences_=draft.references;attachmentList_->clear();for(const auto &path:attachmentPaths_)attachmentList_->addItem(QFileInfo(path).fileName());}
void MailWindow::accountSelected(){
 const auto next=accountList_->currentItem()?accountList_->currentItem()->data(Qt::UserRole).toString():QString();
 if(next!=selectedId_){saveDraft(selectedId_);if(isBusy()&&!background_)cancelOperation();if(selectedId_.isEmpty()&&!next.isEmpty()&&!drafts_.contains(next))drafts_[next]=drafts_.take(QString());selectedId_=next;restoreDraft(next);clearReceived();}
 const auto a=currentAccount();sender_->setText(a.id.isEmpty()?"添加邮箱账户，即可收取和回复邮件":a.label+" · "+a.address);
 {QSignalBlocker b(autoSync_);autoSync_->setChecked(syncEnabled_.contains(selectedId_));}renderInbox();updateAvailability();
}
void MailWindow::renderInbox(){
 const auto selected=inbox_->currentItem()?inbox_->currentItem()->data(0,Qt::UserRole).toString():QString();QSignalBlocker block(inbox_);inbox_->clear();const auto box=inboxes_.value(selectedId_);
 for(const auto uid:box.uids){const auto id=QString::number(uid);const auto summary=box.summaries.value(id);auto *item=new QTreeWidgetItem(inbox_,{summary.from.isEmpty()?QString("邮件 %1").arg(id):summary.from,summary.subject.isEmpty()?QString("未读取"):summary.subject});item->setData(0,Qt::UserRole,id);item->setData(0,Qt::UserRole+1,box.validity);if(box.unread.contains(id)){QFont bold=item->font(0);bold.setBold(true);item->setFont(0,bold);item->setFont(1,bold);}if(id==selected)inbox_->setCurrentItem(item);}
 inbox_->resizeColumnToContents(0);updateAvailability();
}
void MailWindow::refreshBadges(){
 int total=0;for(int n=0;n<accountList_->count();++n){auto *item=accountList_->item(n);const auto a=accountById(item->data(Qt::UserRole).toString());const auto unread=inboxes_.value(a.id).unread.size();total+=unread;item->setText(a.label+(unread?QString("  (%1)").arg(unread):QString())+"\n"+a.address);}
 setWindowTitle(total?QString("LanLink · 邮箱 (%1 封新邮件)").arg(total):QString("LanLink · 邮箱"));
}
void MailWindow::updateAvailability(){
 const bool idle=operation_==Operation::None||background_;const auto a=currentAccount();const bool account=!a.id.isEmpty();const bool network=account&&transport_&&accounts_->credentialStorageAvailable()&&!shuttingDown_;
 accountList_->setEnabled(idle);accountView_->setEnabled(idle);inbox_->setEnabled(idle);inboxView_->setEnabled(idle);
 addAccount_->setEnabled(idle);editAccount_->setEnabled(idle&&account);removeAccount_->setEnabled(idle&&account);authorize_->setEnabled(idle&&network&&a.auth==MailAuth::OAuth2);refresh_->setEnabled(idle&&network);read_->setEnabled(idle&&network&&inbox_->currentItem());
 reply_->setEnabled(idle&&network&&parsedFetch_&&receivedAccountId_==selectedId_&&receivedValidity_==inboxes_.value(selectedId_).validity&&inbox_->currentItem()&&inbox_->currentItem()->data(0,Qt::UserRole).toString()==receivedUid_);send_->setEnabled(idle&&network);cancel_->setEnabled(operation_!=Operation::None);
 saveAttachment_->setEnabled(idle&&receivedAttachments_->currentRow()>=0&&receivedAttachments_->currentRow()<receivedFiles_.size());autoSync_->setEnabled(account&&accounts_->credentialStorageAvailable()&&idle);
 const bool editing=operation_!=Operation::Send;for(auto *w:QList<QWidget*>{recipients_,subject_,body_,attachmentList_,attach_,removeAttachment_})w->setEnabled(editing);
}
void MailWindow::editAccount(bool existing){
 if(background_)cancelOperation();if(isBusy())return;AccountDialog dialog(accounts_.get(),existing?currentAccount():MailAccount(),this);
 if(existing)dialog.findChild<QCheckBox*>("mailAccountAutoSync")->setChecked(syncEnabled_.contains(selectedId_));
 const bool accepted=dialog.exec()==QDialog::Accepted;const auto id=dialog.property("savedAccountId").toString();
 if(accepted&&!id.isEmpty()){if(dialog.property("autoSync").toBool())syncEnabled_.insert(id);else syncEnabled_.remove(id);nextSync_[id]=0;savePreferences();}
 refreshAccounts(id);if(accepted)QTimer::singleShot(0,this,&MailWindow::pollDue);
}
void MailWindow::removeAccount(){
 if(background_)cancelOperation();const auto a=currentAccount();if(a.id.isEmpty()||isBusy())return;QMessageBox box(QMessageBox::Question,"删除邮箱账户","删除账户配置、当前账户草稿、同步记录及其系统凭据？\n"+a.address,QMessageBox::Yes|QMessageBox::No,this);box.setObjectName("mailRemoveConfirmation");box.setTextFormat(Qt::PlainText);box.setDefaultButton(QMessageBox::No);if(box.exec()!=QMessageBox::Yes)return;
 QString error;if(!accounts_->remove(a.id,&error)){status(error,true);return;}syncEnabled_.remove(a.id);inboxes_.remove(a.id);drafts_.remove(a.id);nextSync_.remove(a.id);failures_.remove(a.id);syncState_->remove(a.id);const bool cursorSaved=syncState_->save(&error);const bool prefsSaved=savePreferences();selectedId_.clear();restoreDraft({});clearReceived();refreshAccounts();if(cursorSaved&&prefsSaved)status("账户配置、同步记录及其系统凭据已删除。");else status("账户和凭据已删除，但部分同步记录未能清理，请检查目录权限。",true);
}
void MailWindow::loadPreferences(){
 preferencesLoading_=true;QFile file(QDir(directory_).filePath("sync-preferences.json"));if(file.open(QIODevice::ReadOnly)&&file.size()<65536){const auto obj=QJsonDocument::fromJson(file.readAll()).object();const int interval=obj["intervalMs"].toInt(120000);if(interval==60000||interval==120000||interval==300000||interval==900000)pollIntervalMs_=interval;
 for(const auto &value:obj["accounts"].toArray()){const auto id=value.toString();if(!accountById(id).id.isEmpty())syncEnabled_.insert(id);}notify_->setChecked(obj["notifications"].toBool(true));}
 interval_->setCurrentIndex(interval_->findData(pollIntervalMs_));autoSync_->setChecked(syncEnabled_.contains(selectedId_));preferencesLoading_=false;
}
bool MailWindow::savePreferences(){
 QJsonArray ids;for(const auto &id:syncEnabled_)if(!accountById(id).id.isEmpty())ids.append(id);
 QSaveFile file(QDir(directory_).filePath("sync-preferences.json"));const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"accounts",ids},{"intervalMs",pollIntervalMs_},{"notifications",notify_->isChecked()}}).toJson();
 if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){status("同步设置未能保存；请检查目录权限。",true);return false;}return true;
}
void MailWindow::pollDue(){
 if(shuttingDown_||isBusy()||!transport_||QApplication::activeModalWidget()||!accounts_->credentialStorageAvailable())return;
 const auto now=QDateTime::currentMSecsSinceEpoch();
 for(const auto &a:accounts_->accounts())if(syncEnabled_.contains(a.id)&&nextSync_.value(a.id)<=now){
  // Missing credentials never opens a browser or prompts from a timer.
  QByteArray secret;QString ignored;const bool loaded=accounts_->loadCredential(a.id,&secret,&ignored);bool ready=loaded&&!secret.isEmpty();
  if(ready&&a.auth==MailAuth::OAuth2){QString refresh,client;ready=MailOAuth::readCredential(a,secret,&refresh,&client)&&!refresh.isEmpty();refresh.fill(QChar(0));client.fill(QChar(0));}secret.fill('\0');
  if(!ready){nextSync_[a.id]=now+pollIntervalMs_;continue;}startList(a,true);return;
 }
}
void MailWindow::authorizeAccount(){
 if(background_)cancelOperation();if(isBusy())return;const auto a=currentAccount();if(a.auth!=MailAuth::OAuth2||!accounts_->credentialStorageAvailable())return;
 QByteArray stored;QString ignored;accounts_->loadCredential(a.id,&stored,&ignored);operationAccountId_=a.id;operation_=Operation::Authorize;background_=false;oauthReady_={};updateAvailability();status("请在系统浏览器核对账户及授权范围，完成后回到此窗口。可随时取消。");oauth_->authorize(a,stored);stored.fill('\0');
}
void MailWindow::withEndpoint(const MailAccount &account,bool smtp,std::function<void(MailTransportEndpoint)> ready){
 if(account.id.isEmpty()){transportFailed("请先选择账户");return;}const auto errors=validateMailAccount(account);if(!errors.isEmpty()){transportFailed(errors.join("；"));return;}if(!accounts_->credentialStorageAvailable()){transportFailed(accounts_->credentialStorageProblem());return;}
 QByteArray secret;QString error;if(!accounts_->loadCredential(account.id,&secret,&error)||secret.isEmpty()){secret.fill('\0');transportFailed(account.auth==MailAuth::OAuth2?"请先点击浏览器授权；尚未连接邮件服务器。":error.isEmpty()?"请在设置中保存系统凭据后重试。":error);return;}
 const auto server=smtp?account.smtp:account.imap;MailTransportEndpoint endpoint;endpoint.host=server.host;endpoint.port=quint16(server.port);endpoint.username=account.userName;endpoint.security=server.tls==MailTls::StartTls?MailTransportEndpoint::Security::StartTls:MailTransportEndpoint::Security::ImplicitTls;
 if(account.auth==MailAuth::Password){endpoint.password=QString::fromUtf8(secret);secret.fill('\0');ready(endpoint);endpoint.password.fill(QChar(0));return;}
 endpoint.auth=MailTransportEndpoint::Auth::OAuth2;oauthReady_=[endpoint,ready=std::move(ready)](QString token)mutable{endpoint.password=token;ready(endpoint);endpoint.password.fill(QChar(0));};
 if(!background_)status("正在安全刷新 OAuth 令牌…");oauth_->refresh(account,secret);secret.fill('\0');
}
void MailWindow::refreshInbox(){if(background_)cancelOperation();if(isBusy()||!transport_)return;startList(currentAccount(),false);}
void MailWindow::startList(const MailAccount &account,bool background){
 operation_=Operation::List;operationAccountId_=account.id;background_=background;nextSync_[account.id]=QDateTime::currentMSecsSinceEpoch()+pollIntervalMs_;updateAvailability();
 withEndpoint(account,false,[this](MailTransportEndpoint endpoint){if(!transport_){transportFailed("邮件服务已停止");return;}status(background_?"正在后台检查新邮件…":"正在同步收件箱…");if(!transport_->imapList(endpoint)&&isBusy())transportFailed("无法开始同步收件箱");endpoint.password.fill(QChar(0));});
}
void MailWindow::fetchSelected(){
 if(background_)cancelOperation();if(isBusy()||!transport_||!inbox_->currentItem())return;bool valid=false;const auto uid=inbox_->currentItem()->data(0,Qt::UserRole).toString().toULongLong(&valid);const auto validity=inbox_->currentItem()->data(0,Qt::UserRole+1).toString();if(!valid||!uid||validity.isEmpty()){status("请先同步收件箱。",true);return;}
 operation_=Operation::Fetch;operationAccountId_=selectedId_;background_=false;fetchingUid_=QString::number(uid);fetchingValidity_=validity;pendingMime_.clear();clearReceived();messageInfo_->setText("正在安全读取邮件…");updateAvailability();
 withEndpoint(currentAccount(),false,[this,uid,validity](MailTransportEndpoint endpoint){if(!transport_){transportFailed("邮件服务已停止");return;}if(!transport_->imapFetch(endpoint,uid,"INBOX",validity)&&isBusy())transportFailed("无法开始读取邮件");endpoint.password.fill(QChar(0));});
}
void MailWindow::nextHeaders(){
 if(shuttingDown_||isBusy()||headerQueue_.isEmpty()||!transport_)return;
 if(QApplication::activeModalWidget()){QTimer::singleShot(250,this,&MailWindow::nextHeaders);return;}
 const auto a=accountById(headerAccount_);if(a.id.isEmpty()||inboxes_.value(a.id).validity!=headerValidity_){headerQueue_.clear();return;}
 const auto uid=headerQueue_.takeFirst();operation_=Operation::Headers;operationAccountId_=a.id;background_=true;fetchingUid_=QString::number(uid);fetchingValidity_=headerValidity_;pendingMime_.clear();updateAvailability();
 withEndpoint(a,false,[this,uid](MailTransportEndpoint endpoint){if(!transport_){transportFailed("邮件服务已停止");return;}if(!transport_->imapHeaders(endpoint,uid,"INBOX",fetchingValidity_)&&isBusy())transportFailed("无法读取邮件摘要");endpoint.password.fill(QChar(0));});
}
void MailWindow::finishOperation(bool success){
 if(success){failures_.remove(operationAccountId_);if(operation_==Operation::List||operation_==Operation::Authorize)nextSync_[operationAccountId_]=QDateTime::currentMSecsSinceEpoch()+pollIntervalMs_;}
 else if(background_){const int failure=qMin(6,failures_.value(operationAccountId_)+1);failures_[operationAccountId_]=failure;nextSync_[operationAccountId_]=QDateTime::currentMSecsSinceEpoch()+qMin(900000,pollIntervalMs_*(1<<failure));}
 operation_=Operation::None;background_=false;operationAccountId_.clear();oauthReady_={};pendingMime_.clear();updateAvailability();
}
void MailWindow::cancelOperation(){
 headerQueue_.clear();if(!isBusy())return;const bool sending=operation_==Operation::Send;if(operation_==Operation::Fetch)clearReceived();operation_=Operation::None;background_=false;operationAccountId_.clear();pendingMime_.clear();oauthReady_={};if(oauth_)oauth_->cancel();if(transport_)transport_->cancel();updateAvailability();status(sending?"已取消等待。发送结果可能未知，请核对服务器后再决定是否重发。":"操作已取消。");
}
void MailWindow::transportFailed(QString error){if(!isBusy())return;const bool background=background_;if(operation_==Operation::Fetch)clearReceived();headerQueue_.clear();finishOperation(false);status(background?"后台同步暂未完成，稍后自动重试。"+error:error,true);}
void MailWindow::messageFetched(QByteArray mime){if(operation_==Operation::Fetch||operation_==Operation::Headers)pendingMime_=std::move(mime);}
void MailWindow::transportFinished(QJsonObject result){
 const auto operation=operation_;if(operation==Operation::None)return;const auto account=operationAccountId_;const auto expectedUid=fetchingUid_,expectedValidity=fetchingValidity_;
 if(operation==Operation::Send){finishOperation(true);if(result["protocol"]=="smtp"&&result["accepted"].isBool()&&result["accepted"].toBool())status("SMTP 服务器已接受邮件。这不代表已送达或已读；草稿仍保留，请勿重复发送。");else status("没有有效的 SMTP 接受确认，发送结果未知；请核对后再决定是否重发。",true);return;}
 if(result["protocol"]!="imap"||result["mailbox"]!="INBOX"||!result["uidValidity"].isString()){transportFailed("收件箱结果无效，请重新同步。");return;}
 if(operation==Operation::Fetch||operation==Operation::Headers){
  if(result["uid"].toString()!=expectedUid||result["uidValidity"].toString()!=expectedValidity||result["headersOnly"].toBool()!=(operation==Operation::Headers)||inboxes_.value(account).validity!=expectedValidity){transportFailed("读取结果的邮件 UID 不匹配或 UIDVALIDITY 已变化，正文和附件未显示，请重新同步。");return;}
  const auto parsed=MailMime::parse(pendingMime_);if(!parsed.ok){if(operation==Operation::Headers){finishOperation(true);status("部分邮件摘要无法解析，可单独查看该邮件。",true);QTimer::singleShot(50,this,&MailWindow::nextHeaders);}else transportFailed("邮件解析失败："+parsed.error);return;}
  auto &box=inboxes_[account];box.summaries[expectedUid]={parsed.message.from.displayName.isEmpty()?parsed.message.from.address:parsed.message.from.displayName,parsed.message.subject,parsed.message.date};
  if(operation==Operation::Fetch){
   if(account!=selectedId_){transportFailed("账户已变化，未显示邮件。");return;}receivedMessage_=parsed.message;receivedAccountId_=account;receivedValidity_=expectedValidity;receivedUid_=expectedUid;parsedFetch_=true;receivedFiles_=parsed.message.attachments;receivedAttachments_->clear();for(const auto &file:receivedFiles_)receivedAttachments_->addItem(file.filename+QString(" · %1 字节").arg(file.data.size()));reader_->setPlainText(MailMime::safeDisplayText(parsed.message));messageInfo_->setText(QString("%1\n%2\n%3").arg(parsed.message.subject,parsed.message.from.address,parsed.message.date));box.unread.remove(expectedUid);
  }
  finishOperation(true);if(account==selectedId_)renderInbox();refreshBadges();if(operation==Operation::Fetch)status("邮件已读取。可回复或显式保存附件；不会加载外部资源。");else QTimer::singleShot(50,this,&MailWindow::nextHeaders);return;
 }
 if(!result["uids"].isArray()){transportFailed("收件箱 UID 列表无效。");return;}QList<quint64> uids;QSet<quint64> seen;for(const auto &value:result["uids"].toArray()){bool valid=false;const auto uid=value.toString().toULongLong(&valid);if(!valid||!uid||uid>0xffffffffULL){transportFailed("收件箱 UID 列表包含无效标识。");return;}if(!seen.contains(uid)){seen.insert(uid);uids<<uid;}}
 const auto applied=syncState_->apply(account,result["uidValidity"].toString(),uids);if(!applied.ok){transportFailed(applied.error);return;}
 QString stateError;const bool saved=syncState_->save(&stateError);auto &box=inboxes_[account];const auto validity=syncState_->validity(account);if(box.validity!=validity){box=Inbox();if(account==selectedId_)clearReceived();}box.validity=validity;
 std::sort(uids.begin(),uids.end(),std::greater<quint64>());box.uids=uids.mid(0,500);QSet<QString> retained;for(auto uid:box.uids)retained.insert(QString::number(uid));for(auto i=box.summaries.begin();i!=box.summaries.end();)if(!retained.contains(i.key()))i=box.summaries.erase(i);else ++i;box.unread.intersect(retained);
 for(auto uid:applied.newUids)if(retained.contains(QString::number(uid)))box.unread.insert(QString::number(uid));
 headerQueue_.clear();headerAccount_=account;headerValidity_=validity;for(auto uid:box.uids.mid(0,30))if(!box.summaries.contains(QString::number(uid)))headerQueue_<<uid;
 finishOperation(true);if(account==selectedId_)renderInbox();refreshBadges();
 status(applied.reset?"收件箱标识已变化，旧内容已清除并重新建立基线。":QString("已同步 %1 封邮件，显示最新 %2 封。%3").arg(uids.size()).arg(box.uids.size()).arg(applied.baseline?"首次同步不提醒历史邮件。":""));
 if(!saved)status("同步完成，但去重记录未保存，下次启动可能重复提醒："+stateError,true);
 if(!applied.newUids.isEmpty())notifyNewMail(account,applied.newUids.size());QTimer::singleShot(50,this,&MailWindow::nextHeaders);
}
void MailWindow::notifyNewMail(QString id,int count){
 emit newMailAvailable(id,count);if(!notify_->isChecked())return;notificationAccount_=id;QApplication::alert(parentWidget()?parentWidget():this,5000);
 if(!tray_&&QSystemTrayIcon::isSystemTrayAvailable()){tray_=new QSystemTrayIcon(style()->standardIcon(QStyle::SP_MessageBoxInformation),this);tray_->setToolTip("LanLink · 邮箱");connect(tray_,&QSystemTrayIcon::messageClicked,this,[this]{
  if(shuttingDown_)return;showNormal();raise();activateWindow();if(!isBusy()||background_)for(int n=0;n<accountList_->count();++n)if(accountList_->item(n)->data(Qt::UserRole).toString()==notificationAccount_){accountList_->setCurrentRow(n);break;}tabs_->setCurrentIndex(0);
 });tray_->show();}
 if(tray_)tray_->showMessage("LanLink · 新邮件",QString("收到 %1 封新邮件，打开邮箱查看。").arg(count),QSystemTrayIcon::Information,5000);
}
void MailWindow::replySelected(){
 if(background_)cancelOperation();if(isBusy()||!parsedFetch_||receivedAccountId_!=selectedId_||receivedValidity_!=inboxes_.value(selectedId_).validity||!inbox_->currentItem()||inbox_->currentItem()->data(0,Qt::UserRole).toString()!=receivedUid_)return;
 const auto account=currentAccount();const auto reply=MailMime::makeReplyDraft(receivedMessage_,{account.label,account.address});if(!reply.ok){status("无法生成回复："+reply.error,true);return;}
 if(!recipients_->text().isEmpty()||!subject_->text().isEmpty()||!body_->toPlainText().isEmpty()||!attachmentPaths_.isEmpty()){
  QMessageBox box(QMessageBox::Question,"替换邮件草稿","当前账户已有草稿。生成回复将替换正文并移除附件选择，是否继续？",QMessageBox::Yes|QMessageBox::No,this);box.setObjectName("mailReplyConfirmation");box.setDefaultButton(QMessageBox::No);if(box.exec()!=QMessageBox::Yes)return;
 }
 if(!setExternalDraft(reply.message.to.first().address,reply.message.subject,reply.message.text,true))return;draftReplyTo_=reply.message.inReplyTo;draftReferences_=reply.message.references;
 status("回复草稿已生成，尚未发送。请核对 Reply-To 收件人、发件账户和引用内容。");
}
void MailWindow::addAttachments(){if(operation_==Operation::Send)return;const auto files=QFileDialog::getOpenFileNames(this,"选择附件（合计不超过 20 MiB）");for(const auto &path:files){if(attachmentPaths_.contains(path))continue;const QFileInfo file(path);if(!file.isFile()||file.isSymLink()||file.size()>20*1024*1024||attachmentPaths_.size()>=20){status("附件必须是普通文件；每个不超过 20 MiB，最多 20 个。",true);continue;}attachmentPaths_<<path;attachmentList_->addItem(file.fileName()+QString(" · %1 字节").arg(file.size()));}}
void MailWindow::sendDraft(){
 if(background_)cancelOperation();if(isBusy()||!transport_)return;const auto account=currentAccount();MailMime::Message message;message.from={account.label,account.address};QStringList recipients;
 for(auto address:recipients_->text().split(QRegularExpression("[,;\\n]"),Qt::SkipEmptyParts)){address=address.trimmed();if(!recipients.contains(address)){recipients<<address;message.to.append({{},address});}}
 message.subject=subject_->text();message.text=body_->toPlainText();message.inReplyTo=draftReplyTo_;message.references=draftReferences_;qsizetype total=message.text.toUtf8().size();QStringList attachmentSummary;
 for(const auto &path:attachmentPaths_){QFileInfo info(path);if(!info.isFile()||info.isSymLink()||info.size()<0||info.size()>20*1024*1024||total+info.size()>20*1024*1024){status("附件已变化、不可读取或总大小超过 20 MiB。",true);return;}QFile f(path);if(!f.open(QIODevice::ReadOnly)){status("无法读取附件："+info.fileName(),true);return;}auto data=f.read(20*1024*1024+1);if(!f.atEnd()||data.size()!=info.size()){status("附件读取不完整或大小发生变化。",true);return;}total+=data.size();QMimeDatabase db;message.attachments.append({info.fileName(),db.mimeTypeForFile(info,QMimeDatabase::MatchExtension).name(),data});attachmentSummary<<info.fileName()+QString(" (%1 字节)").arg(data.size());}
 const auto built=MailMime::build(message);if(!built.ok){status("不能发送："+built.error,true);return;}
 QMessageBox confirm(QMessageBox::Question,"确认发送邮件",QString("发件人：%1\n收件人：%2\n主题：%3\n附件：%4\n\n将经 %5:%6 发送此纯文本邮件。是否继续？").arg(account.address,recipients.join("; "),message.subject,attachmentSummary.isEmpty()?"无":attachmentSummary.join("\n"),account.smtp.host,QString::number(account.smtp.port)),QMessageBox::Yes|QMessageBox::No,this);
 confirm.setObjectName("mailSendConfirmation");confirm.setTextFormat(Qt::PlainText);confirm.setDefaultButton(QMessageBox::No);confirm.setDetailedText(message.text);
 if(confirm.exec()!=QMessageBox::Yes){status("未发送，草稿已保留。");return;}if(!transport_){status("邮件服务已停止，未发送。",true);return;}
 operation_=Operation::Send;operationAccountId_=account.id;background_=false;updateAvailability();
 withEndpoint(account,true,[this,account,recipients,wire=built.wire](MailTransportEndpoint endpoint){if(!transport_){transportFailed("邮件服务已停止，未发送。");return;}status("正在向 SMTP 服务器提交邮件，请勿重复发送…");if(!transport_->smtpSend(endpoint,account.address,recipients,wire)&&isBusy())transportFailed("无法开始发送");endpoint.password.fill(QChar(0));});
}
bool MailWindow::hasDraft() const {
 if(!recipients_->text().isEmpty()||!subject_->text().isEmpty()||!body_->toPlainText().isEmpty()||!attachmentPaths_.isEmpty())return true;
 for(auto i=drafts_.cbegin();i!=drafts_.cend();++i)if(i.key()!=selectedId_&&(!i->recipients.isEmpty()||!i->subject.isEmpty()||!i->body.isEmpty()||!i->paths.isEmpty()))return true;return false;
}
bool MailWindow::isBusy() const{return operation_!=Operation::None;}
bool MailWindow::setExternalDraft(QString recipient,QString subject,QString body,bool replaceConfirmed){
 if(background_)cancelOperation();const bool currentDraft=!recipients_->text().isEmpty()||!subject_->text().isEmpty()||!body_->toPlainText().isEmpty()||!attachmentPaths_.isEmpty();if(isBusy()||(currentDraft&&!replaceConfirmed))return false;
 recipients_->setText(std::move(recipient));subject_->setText(std::move(subject));body_->setPlainText(std::move(body));draftReplyTo_.clear();draftReferences_.clear();attachmentPaths_.clear();attachmentList_->clear();tabs_->setCurrentIndex(1);status("已预填邮件草稿，尚未发送。请核对收件人、发件账户及正文。");return true;
}
void MailWindow::clearReceived(){parsedFetch_=false;receivedMessage_={};receivedAccountId_.clear();receivedValidity_.clear();receivedUid_.clear();reader_->clear();receivedFiles_.clear();receivedAttachments_->clear();messageInfo_->setText("选择邮件，查看正文或回复。");}
void MailWindow::saveReceivedAttachment(){
 const int row=receivedAttachments_->currentRow();if((isBusy()&&!background_)||row<0||row>=receivedFiles_.size())return;const auto attachment=receivedFiles_[row];const auto account=receivedAccountId_,uid=receivedUid_,validity=receivedValidity_;
 QString suggested=attachment.filename;suggested.replace('\\','/');suggested=QFileInfo(suggested).fileName();bool safe=!suggested.isEmpty()&&suggested!="."&&suggested!="..";for(const auto c:suggested)if(c.unicode()<32||c.unicode()==127)safe=false;if(!safe)suggested="attachment.bin";
 const auto path=QFileDialog::getSaveFileName(this,"保存附件（不会打开文件）",QDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).filePath(suggested));if(path.isEmpty())return;
 if(account!=selectedId_||account!=receivedAccountId_||uid!=receivedUid_||validity!=inboxes_.value(account).validity){status("当前邮件已变化，附件未保存。",true);return;}
 if(QFileInfo(path).isSymLink()){status("不能通过符号链接覆盖附件目标。",true);return;}QSaveFile file(path);if(!file.open(QIODevice::WriteOnly)||file.write(attachment.data)!=attachment.data.size()||!file.commit()){status("附件保存失败，未报告成功。",true);return;}status("附件已保存："+QFileInfo(path).fileName()+"。未打开或执行。");
}
