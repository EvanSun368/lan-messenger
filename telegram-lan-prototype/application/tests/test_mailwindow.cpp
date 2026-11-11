#include "mailwindow.h"
#include "mailmime.h"
#include <QtTest>
#include <QtWidgets>

class MockCredentials final:public MailCredentialStore {
public:
 bool enabled=true;QHash<QString,QByteArray> values;
 bool save(const QString&id,const QByteArray &value,QString*)override{if(!enabled)return false;values[id]=value;return true;}
 bool load(const QString&id,QByteArray *value,QString *error)override{if(!enabled||!values.contains(id)){if(error)*error="没有可用的系统凭据";return false;}*value=values[id];return true;}
 bool remove(const QString&id,QString*)override{if(!enabled)return false;values.remove(id);return true;}
 bool available()const override{return enabled;}
 QString unavailableReason()const override{return "系统凭据服务不可用（测试）";}
};
class MockMailTransport final:public MailTransport {
public:
 int lists=0,fetches=0,headers=0,sends=0,cancels=0;QString expectedValidity;bool active=false;quint64 fetchedUid=0;QByteArray lastWire;QStringList recipients;MailTransportEndpoint lastEndpoint;
 bool imapList(MailTransportEndpoint e,QString)override{++lists;lastEndpoint=e;active=true;return true;}
 bool imapFetch(MailTransportEndpoint e,quint64 uid,QString,QString validity)override{expectedValidity=validity;++fetches;lastEndpoint=e;fetchedUid=uid;active=true;return true;}
 bool imapHeaders(MailTransportEndpoint e,quint64 uid,QString,QString validity)override{++headers;lastEndpoint=e;fetchedUid=uid;expectedValidity=validity;active=true;return true;}
 bool smtpSend(MailTransportEndpoint e,QString,QStringList to,QByteArray wire)override{++sends;lastEndpoint=e;recipients=to;lastWire=wire;active=true;return true;}
 bool busy()const override{return active;}
 void cancel()override{if(active){++cancels;active=false;emit failed("操作取消");}}
 void listResult(QJsonArray uids={"12","4","12"},QString validity="7"){active=false;emit finished({{"protocol","imap"},{"mailbox","INBOX"},{"uidValidity",validity},{"uids",uids}});}
 void mimeResult(QByteArray bytes){active=false;emit messageFetched(bytes);emit finished({{"protocol","imap"},{"mailbox","INBOX"},{"uidValidity",expectedValidity},{"headersOnly",false},{"uid",QString::number(fetchedUid)}});}
 void sendResult(bool accepted){active=false;emit finished({{"protocol","smtp"},{"accepted",accepted}});}
 void failure(QString reason){active=false;emit failed(reason);}
};
class MailWindowTest:public QObject {
 Q_OBJECT
 static MailAccount seed(QString root,std::shared_ptr<MockCredentials> secrets,QString address="person@example.invalid") {
  MailAccount a=mailAccountFromTemplate("manual",address);a.label="演示账户（无真实连接）";a.imap={"imap.example.invalid",993,MailTls::ImplicitTls};a.smtp={"smtp.example.invalid",587,MailTls::StartTls};
  MailAccounts accounts(QDir(root).filePath("accounts.json"),secrets);QString error;
  if(!accounts.load(&error)||!accounts.upsert(a,&error))qFatal("Test account setup failed");
  if(secrets->enabled)accounts.saveCredential(a.id,"test-only-secret",&error);
  return a;
 }
 static void confirmation(bool yes,QString expected={}) {
  QTimer::singleShot(0,[yes,expected]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
   if(!box)qFatal("Expected a confirmation dialog");
   if(!expected.isEmpty()&&!box->text().contains(expected))qFatal("Confirmation missing recipient/attachment");
   if(box->text().contains("test-only-secret"))qFatal("Secret shown in confirmation");
   box->done(yes?QMessageBox::Yes:QMessageBox::No);
  });
 }
private slots:

 void enabledAccountPollsAndStopsOnShutdown() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();const auto account=seed(root.path(),credentials);MockMailTransport transport;
  QFile settings(root.filePath("sync-preferences.json"));QVERIFY(settings.open(QIODevice::WriteOnly));settings.write(QJsonDocument(QJsonObject{{"version",1},{"accounts",QJsonArray{account.id}},{"intervalMs",60000},{"notifications",false}}).toJson());settings.close();
  MailWindow window(root.path(),credentials,&transport);window.show();QSignalSpy notices(&window,&MailWindow::newMailAvailable);
  QTRY_COMPARE(transport.lists,1);QTest::qWait(1050);QCOMPARE(transport.lists,1); // one operation at a time
  transport.listResult({},"7");QCOMPARE(notices.count(),0); // empty first baseline
  QVERIFY(window.close());QVERIFY(!window.isVisible()); // hiding keeps configured background checks alive
  window.findChild<QComboBox*>("mailSyncInterval")->setCurrentIndex(1);QTRY_COMPARE(transport.lists,2);
  transport.listResult({"1"},"7");QCOMPARE(notices.count(),1);QCOMPARE(notices.first()[0].toString(),account.id);
  QTRY_COMPARE(transport.headers,1);QCOMPARE(transport.expectedValidity,"7");
  QVERIFY(window.shutdown());const auto lists=transport.lists;QTest::qWait(1100);QCOMPARE(transport.lists,lists);QVERIFY(!window.isBusy());QCOMPARE(transport.sends,0);
 }
 void newMailDedupesAndUidValidityResetsSilently() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();seed(root.path(),credentials);MockMailTransport transport;MailWindow window(root.path(),credentials,&transport);window.show();window.findChild<QCheckBox*>("mailNotifications")->setChecked(false);QSignalSpy notices(&window,&MailWindow::newMailAvailable);
  auto refresh=[&](QJsonArray ids,QString validity="7"){window.findChild<QPushButton*>("mailRefresh")->click();transport.listResult(ids,validity);};
  refresh({"4","12"});QCOMPARE(notices.count(),0);refresh({"13","4","12","13"});QCOMPARE(notices.count(),1);QCOMPARE(notices.first()[1].toInt(),1);
  refresh({"13","12"});QCOMPARE(notices.count(),1);refresh({"13","12"},"8");QCOMPARE(notices.count(),1);QCOMPARE(window.findChild<QTreeWidget*>("mailInbox")->topLevelItemCount(),2);
  refresh({"14","13"},"8");QCOMPARE(notices.count(),2);QVERIFY(window.shutdown());
  MockMailTransport reopenedTransport;MailWindow reopened(root.path(),credentials,&reopenedTransport);QSignalSpy reopenedNotices(&reopened,&MailWindow::newMailAvailable);reopened.findChild<QPushButton*>("mailRefresh")->click();reopenedTransport.listResult({"14","13"},"8");QCOMPARE(reopenedNotices.count(),0);
 }
 void backgroundAccountCannotPopulateOtherAccountReader() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();seed(root.path(),credentials);seed(root.path(),credentials,"second@example.invalid");MockMailTransport transport;MailWindow window(root.path(),credentials,&transport);window.show();
  window.findChild<QCheckBox*>("mailAutoSync")->setChecked(true);QTRY_COMPARE(transport.lists,1);auto *accounts=window.findChild<QListWidget*>("mailAccounts");QVERIFY(accounts->isEnabled());accounts->setCurrentRow(1);
  transport.listResult({"99"});QCOMPARE(window.findChild<QTreeWidget*>("mailInbox")->topLevelItemCount(),0);QVERIFY(window.findChild<QPlainTextEdit*>("mailReader")->toPlainText().isEmpty());
  accounts->setCurrentRow(0);QCOMPARE(window.findChild<QTreeWidget*>("mailInbox")->topLevelItemCount(),1);QCOMPARE(transport.sends,0);QVERIFY(window.shutdown());
 }
 void replyDraftUsesReplyToAndKeepsAccountsSeparate() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();seed(root.path(),credentials);seed(root.path(),credentials,"second@example.invalid");MockMailTransport transport;MailWindow window(root.path(),credentials,&transport);window.show();
  window.findChild<QPushButton*>("mailRefresh")->click();transport.listResult();auto *inbox=window.findChild<QTreeWidget*>("mailInbox");inbox->setCurrentItem(inbox->topLevelItem(0));window.findChild<QPushButton*>("mailRead")->click();
  transport.mimeResult("From: sender@example.invalid\r\nReply-To: reply@example.invalid\r\nTo: person@example.invalid\r\nSubject: Status\r\nMessage-ID: <original@example.invalid>\r\nContent-Type: text/plain; charset=UTF-8\r\n\r\nhello");
  auto *reply=window.findChild<QPushButton*>("mailReply");QVERIFY(reply->isEnabled());inbox->setCurrentItem(inbox->topLevelItem(1));QVERIFY(!reply->isEnabled());inbox->setCurrentItem(inbox->topLevelItem(0));QVERIFY(reply->isEnabled());reply->click();QCOMPARE(transport.sends,0);QCOMPARE(window.findChild<QLineEdit*>("mailRecipients")->text(),"reply@example.invalid");QCOMPARE(window.findChild<QLineEdit*>("mailSubject")->text(),"Re: Status");QVERIFY(window.findChild<QPlainTextEdit*>("mailBody")->toPlainText().contains("> hello"));
  auto *accounts=window.findChild<QListWidget*>("mailAccounts");accounts->setCurrentRow(1);QVERIFY(window.findChild<QLineEdit*>("mailRecipients")->text().isEmpty());QVERIFY(window.findChild<QPlainTextEdit*>("mailReader")->toPlainText().isEmpty());QVERIFY(!reply->isEnabled());QVERIFY(window.hasDraft());
  accounts->setCurrentRow(0);QCOMPARE(window.findChild<QLineEdit*>("mailRecipients")->text(),"reply@example.invalid");confirmation(true,"reply@example.invalid");window.findChild<QPushButton*>("mailSend")->click();QCOMPARE(transport.sends,1);QVERIFY(transport.lastWire.contains("In-Reply-To: <original@example.invalid>"));transport.sendResult(true);
 }

 void disablingSyncDuringHeaderGapStopsFurtherRequests() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();seed(root.path(),credentials);MockMailTransport transport;MailWindow window(root.path(),credentials,&transport);window.show();auto *sync=window.findChild<QCheckBox*>("mailAutoSync");sync->setChecked(true);QTRY_COMPARE(transport.lists,1);transport.listResult();sync->setChecked(false);QTest::qWait(150);QCOMPARE(transport.headers,0);QCOMPARE(transport.lists,1);
 }
 void malformedHeaderDoesNotBlockLaterSummary() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();seed(root.path(),credentials);MockMailTransport transport;MailWindow window(root.path(),credentials,&transport);window.show();window.findChild<QPushButton*>("mailRefresh")->click();transport.listResult();QTRY_COMPARE(transport.headers,1);emit transport.messageFetched("invalid header");transport.active=false;emit transport.finished({{"protocol","imap"},{"mailbox","INBOX"},{"uidValidity","7"},{"uid","12"},{"headersOnly",true}});QTRY_COMPARE(transport.headers,2);QCOMPARE(transport.fetchedUid,quint64(4));emit transport.messageFetched("From: person@example.invalid\r\nSubject: Summary\r\n\r\n");transport.active=false;emit transport.finished({{"protocol","imap"},{"mailbox","INBOX"},{"uidValidity","7"},{"uid","4"},{"headersOnly",true}});QCOMPARE(window.findChild<QTreeWidget*>("mailInbox")->topLevelItem(1)->text(1),"Summary");QVERIFY(window.findChild<QPlainTextEdit*>("mailReader")->toPlainText().isEmpty());
 }
 void fullModeUsesOriginalControlsAndLists() {
#ifdef LANLINK_FULL_TELEGRAM_UI
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();seed(root.path(),credentials);MockMailTransport transport;MailWindow window(root.path(),credentials,&transport);window.show();QCoreApplication::processEvents();
  for(const auto &name:{"telegramMailAccounts","telegramMailInbox","original_mailRefresh","original_mailBody","original_mailReader","original_mailRecipients"}){auto *view=window.findChild<QWidget*>(name);QVERIFY2(view,name);QVERIFY2(view->isVisible()||QString(name).contains("Body")||QString(name).contains("Recipients"),name);}
  QVERIFY(!window.findChild<QListWidget*>("mailAccounts")->isVisible());QVERIFY(!window.findChild<QTreeWidget*>("mailInbox")->isVisible());QVERIFY(!window.findChild<QPlainTextEdit*>("mailReader")->isVisible());
  auto *refresh=window.findChild<QWidget*>("original_mailRefresh");QTest::mouseClick(refresh,Qt::LeftButton);QCOMPARE(transport.lists,1);auto *cancel=window.findChild<QWidget*>("original_mailCancel");QVERIFY(cancel->isEnabled());QTest::mouseClick(cancel,Qt::LeftButton);QCOMPARE(transport.cancels,1);QVERIFY(refresh->isEnabled());
  window.findChild<QTabWidget*>("mailTabs")->setCurrentIndex(1);auto *recipientView=window.findChild<QWidget*>("original_mailRecipients");auto *editor=recipientView->findChild<QTextEdit*>();QVERIFY(editor);editor->setFocus();QTest::keyClicks(editor,"reply@example.invalid");QCOMPARE(window.findChild<QLineEdit*>("mailRecipients")->text(),"reply@example.invalid");
#else
  QSKIP("Original lib_ui is only linked in FULL mode.");
#endif
 }
 void constructionAndAccountSelectionNeverConnect() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();const auto a=seed(root.path(),credentials);seed(root.path(),credentials,"second@example.invalid");MockMailTransport transport;
  MailWindow window(root.path(),credentials,&transport);window.show();
  QCOMPARE(window.objectName(),"mailWindow");auto *list=window.findChild<QListWidget*>("mailAccounts");QCOMPARE(list->count(),2);list->setCurrentRow(1);QCoreApplication::processEvents();
  QCOMPARE(transport.lists,0);QCOMPARE(transport.fetches,0);QCOMPARE(transport.sends,0);
  QFile config(root.filePath("accounts.json"));QVERIFY(config.open(QIODevice::ReadOnly));QVERIFY(!config.readAll().contains("test-only-secret"));
  window.findChild<QPushButton*>("mailRefresh")->click();QCOMPARE(transport.lists,1);QVERIFY(!list->isEnabled());
  window.findChild<QPushButton*>("mailCancel")->click();QCOMPARE(transport.cancels,1);QVERIFY(list->isEnabled());QCOMPARE(transport.lists,1);
 }
 void unavailableSecretStoreFailsClosed() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();credentials->enabled=false;seed(root.path(),credentials);MockMailTransport transport;
  MailWindow window(root.path(),credentials,&transport);window.show();QVERIFY(!window.findChild<QPushButton*>("mailRefresh")->isEnabled());QVERIFY(!window.findChild<QPushButton*>("mailSend")->isEnabled());
  window.findChild<QPushButton*>("mailRefresh")->click();QCOMPARE(transport.lists,0);QVERIFY(window.findChild<QLabel*>("mailCredentialNotice")->text().contains("不可用"));
 }
 void explicitReadUsesSafePlainTextAndUid() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();seed(root.path(),credentials);MockMailTransport transport;MailWindow window(root.path(),credentials,&transport);window.show();
  window.findChild<QPushButton*>("mailRefresh")->click();transport.listResult();auto *inbox=window.findChild<QTreeWidget*>("mailInbox");QCOMPARE(inbox->topLevelItemCount(),2);QCOMPARE(inbox->topLevelItem(0)->text(0),"邮件 12");QCOMPARE(transport.fetches,0);
  inbox->setCurrentItem(inbox->topLevelItem(0));QCOMPARE(transport.fetches,0);window.findChild<QPushButton*>("mailRead")->click();QCOMPARE(transport.fetchedUid,quint64(12));QVERIFY(!inbox->isEnabled());
  transport.mimeResult("From: sender@example.invalid\r\nTo: person@example.invalid\r\nSubject: Safe message\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n<p>Hello mail</p><img src=\"http://127.0.0.1:9/tracker\"><img src=\"file:///etc/passwd\"><a href=\"file:///etc/passwd\">link label</a>");
  auto *reader=window.findChild<QPlainTextEdit*>("mailReader");QVERIFY(reader);QVERIFY(reader->isReadOnly());QVERIFY(reader->toPlainText().contains("Hello mail"));QVERIFY(!reader->toPlainText().contains("root:"));QVERIFY(!reader->toPlainText().contains("<img"));QCOMPARE(inbox->topLevelItem(0)->text(1),"Safe message");QVERIFY(window.findChild<QLabel*>("mailMessageInfo")->textFormat()==Qt::PlainText);
  window.setWindowTitle("邮箱 · 模拟账户界面验证（无网络）");window.grab().save("/tmp/mail-window-inbox-mock.png");
  inbox->setCurrentItem(inbox->topLevelItem(1));window.findChild<QPushButton*>("mailRead")->click();QVERIFY(reader->toPlainText().isEmpty());transport.failure("测试读取失败");QVERIFY(reader->toPlainText().isEmpty());QVERIFY(window.findChild<QLabel*>("mailStatus")->text().contains("失败"));
 }
 void closeNeedsConfirmationAndNoPreservesPendingOperation() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();seed(root.path(),credentials);MockMailTransport transport;MailWindow window(root.path(),credentials,&transport);window.show();
  QVERIFY(window.setExternalDraft("recipient@example.invalid","Subject","Keep this draft"));
  window.findChild<QPushButton*>("mailRefresh")->click();QVERIFY(window.isBusy());
  QTimer::singleShot(0,[]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(!box)qFatal("Expected close confirmation");QCOMPARE(box->objectName(),"mailCloseConfirmation");QCOMPARE(box->standardButton(box->defaultButton()),QMessageBox::No);QVERIFY(box->text().contains("取消等待不代表未发送"));box->done(QMessageBox::No);});
  QVERIFY(!window.close());QVERIFY(window.isVisible());QVERIFY(window.isBusy());QVERIFY(window.hasDraft());QCOMPARE(transport.cancels,0);QCOMPARE(window.findChild<QPlainTextEdit*>("mailBody")->toPlainText(),"Keep this draft");
  QTimer::singleShot(0,[]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(!box)qFatal("Expected Escape close confirmation");box->done(QMessageBox::No);});
  QTest::keyClick(&window,Qt::Key_Escape);QVERIFY(window.isVisible());QVERIFY(window.isBusy());QCOMPARE(transport.cancels,0);
  QTimer::singleShot(0,[]{auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(!box)qFatal("Expected close confirmation");box->done(QMessageBox::Yes);});
  QVERIFY(window.close());QVERIFY(!window.isVisible());QVERIFY(!window.isBusy());QCOMPARE(transport.cancels,1);
 }
 void fetchUidMismatchClearsBodyAndAttachments() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();seed(root.path(),credentials);MockMailTransport transport;MailWindow window(root.path(),credentials,&transport);window.show();
  window.findChild<QPushButton*>("mailRefresh")->click();transport.listResult();auto *inbox=window.findChild<QTreeWidget*>("mailInbox");inbox->setCurrentItem(inbox->topLevelItem(0));window.findChild<QPushButton*>("mailRead")->click();
  MailMime::Message message;message.from={"Sender","sender@example.invalid"};message.to.append(MailMime::Address{{},"person@example.invalid"});message.subject="Wrong UID";message.text="must disappear";message.attachments.append({"wrong.txt","text/plain","bytes"});const auto built=MailMime::build(message);QVERIFY(built.ok);
  emit transport.messageFetched(built.wire);QVERIFY(window.findChild<QPlainTextEdit*>("mailReader")->toPlainText().isEmpty());
  transport.active=false;emit transport.finished({{"protocol","imap"},{"mailbox","INBOX"},{"uidValidity","7"},{"headersOnly",false},{"uid","999"}});
  QVERIFY(window.findChild<QPlainTextEdit*>("mailReader")->toPlainText().isEmpty());QCOMPARE(window.findChild<QListWidget*>("mailReceivedAttachments")->count(),0);QVERIFY(!window.findChild<QPushButton*>("mailSaveAttachment")->isEnabled());QCOMPARE(inbox->topLevelItem(0)->text(1),"未读取");QVERIFY(window.findChild<QLabel*>("mailStatus")->text().contains("UID 不匹配"));QVERIFY(!window.isBusy());
  auto *info=window.findChild<QLabel*>("mailMessageInfo");QVERIFY(info->minimumHeight()>=info->fontMetrics().lineSpacing()*3);
 }
 void sendNeedsExplicitConfirmationAndPreservesDraftOnUnknown() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();seed(root.path(),credentials);MockMailTransport transport;MailWindow window(root.path(),credentials,&transport);window.show();window.findChild<QTabWidget*>("mailTabs")->setCurrentIndex(1);
  auto *to=window.findChild<QLineEdit*>("mailRecipients");auto *subject=window.findChild<QLineEdit*>("mailSubject");auto *body=window.findChild<QPlainTextEdit*>("mailBody");auto *send=window.findChild<QPushButton*>("mailSend");auto *status=window.findChild<QLabel*>("mailStatus");
  to->setText("recipient@example.invalid");subject->setText("界面验证（不连接真实邮箱）");body->setPlainText("Draft retained");
  confirmation(false,"recipient@example.invalid");send->click();QCOMPARE(transport.sends,0);QCOMPARE(body->toPlainText(),"Draft retained");
  confirmation(true,"recipient@example.invalid");send->click();QCOMPARE(transport.sends,1);QVERIFY(!send->isEnabled());QVERIFY(!body->isEnabled());
  transport.failure("邮件发送结果未知：请勿直接重发");QVERIFY(status->text().contains("结果未知"));QCOMPARE(body->toPlainText(),"Draft retained");QCOMPARE(transport.sends,1);
  confirmation(true);send->click();transport.sendResult(false);QVERIFY(status->text().contains("没有有效"));QCOMPARE(transport.sends,2);
  confirmation(true);send->click();transport.sendResult(true);QVERIFY(status->text().contains("SMTP 服务器已接受"));QVERIFY(status->text().contains("不代表已送达或已读"));QCOMPARE(transport.sends,3);QCOMPARE(body->toPlainText(),"Draft retained");
  QCOMPARE(transport.lastEndpoint.security,MailTransportEndpoint::Security::StartTls);auto parsed=MailMime::parse(transport.lastWire);QVERIFY(parsed.ok);QCOMPARE(parsed.message.text,QString("Draft retained"));
  window.setWindowTitle("邮箱 · 模拟账户界面验证（无网络）");window.grab().save("/tmp/mail-window-compose-mock.png");
 }
 void attachmentIsIncludedAndListedInConfirmation() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();seed(root.path(),credentials);MockMailTransport transport;MailWindow window(root.path(),credentials,&transport);window.show();window.findChild<QTabWidget*>("mailTabs")->setCurrentIndex(1);
  QFile file(root.filePath("attachment.txt"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("attachment body");file.close();
  QTimer::singleShot(0,[&]{auto *dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());if(!dialog)qFatal("Expected attachment chooser");dialog->selectFile(file.fileName());QMetaObject::invokeMethod(dialog,"accept");});
  window.findChild<QPushButton*>("mailAttach")->click();QCOMPARE(window.findChild<QListWidget*>("mailAttachments")->count(),1);
  window.findChild<QLineEdit*>("mailRecipients")->setText("recipient@example.invalid");window.findChild<QLineEdit*>("mailSubject")->setText("Attachment test");
  confirmation(true,"attachment.txt");window.findChild<QPushButton*>("mailSend")->click();QCOMPARE(transport.sends,1);const auto result=MailMime::parse(transport.lastWire);QVERIFY(result.ok);QCOMPARE(result.message.attachments.size(),1);QCOMPARE(result.message.attachments.first().filename,"attachment.txt");QCOMPARE(result.message.attachments.first().data,QByteArray("attachment body"));
  transport.failure("测试失败，草稿保留");QCOMPARE(window.findChild<QListWidget*>("mailAttachments")->count(),1);
 }
 void receivedAttachmentSavesOnlyOnExplicitChoiceAndClearsOnSwitch() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();seed(root.path(),credentials);seed(root.path(),credentials,"second@example.invalid");MockMailTransport transport;MailWindow window(root.path(),credentials,&transport);window.show();
  window.findChild<QPushButton*>("mailRefresh")->click();transport.listResult();auto *inbox=window.findChild<QTreeWidget*>("mailInbox");inbox->setCurrentItem(inbox->topLevelItem(0));window.findChild<QPushButton*>("mailRead")->click();
  MailMime::Message message;message.from={"Sender","sender@example.invalid"};message.to.append(MailMime::Address{{},"person@example.invalid"});message.subject="Attachment";message.text="mail body";message.attachments.append({"received.txt","text/plain","received bytes"});const auto built=MailMime::build(message);QVERIFY(built.ok);transport.mimeResult(built.wire);
  auto *files=window.findChild<QListWidget*>("mailReceivedAttachments");QCOMPARE(files->count(),1);files->setCurrentRow(0);auto *save=window.findChild<QPushButton*>("mailSaveAttachment");QVERIFY(save->isEnabled());const auto target=root.filePath("saved.txt");QVERIFY(!QFileInfo::exists(target));
  QTimer::singleShot(0,[&]{auto *dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());if(!dialog)qFatal("Expected explicit save chooser");dialog->selectFile(target);QMetaObject::invokeMethod(dialog,"accept");});save->click();QFile output(target);QVERIFY(output.open(QIODevice::ReadOnly));QCOMPARE(output.readAll(),QByteArray("received bytes"));QVERIFY(window.findChild<QLabel*>("mailStatus")->text().contains("未打开或执行"));
  window.findChild<QListWidget*>("mailAccounts")->setCurrentRow(1);QCOMPARE(files->count(),0);QVERIFY(!save->isEnabled());QVERIFY(window.findChild<QPlainTextEdit*>("mailReader")->toPlainText().isEmpty());QCOMPARE(transport.fetches,1);
 }
 void externalDraftNeverSendsAndRequiresExplicitReplacement() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();seed(root.path(),credentials);MockMailTransport transport;MailWindow window(root.path(),credentials,&transport);window.show();
  QVERIFY(!window.hasDraft());QVERIFY(!window.isBusy());QVERIFY(window.setExternalDraft("friend@example.invalid","聊天导出","selected history"));QVERIFY(window.hasDraft());QCOMPARE(window.findChild<QTabWidget*>("mailTabs")->currentIndex(),1);QCOMPARE(transport.sends+transport.lists+transport.fetches,0);
  QVERIFY(!window.setExternalDraft("other@example.invalid","new","new body"));QCOMPARE(window.findChild<QPlainTextEdit*>("mailBody")->toPlainText(),"selected history");
  QFile attachment(root.filePath("old.txt"));QVERIFY(attachment.open(QIODevice::WriteOnly));attachment.write("old attachment");attachment.close();
  QTimer::singleShot(0,[&]{auto *dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());if(!dialog)qFatal("Expected chooser");dialog->selectFile(attachment.fileName());QMetaObject::invokeMethod(dialog,"accept");});window.findChild<QPushButton*>("mailAttach")->click();QCOMPARE(window.findChild<QListWidget*>("mailAttachments")->count(),1);
  QVERIFY(window.setExternalDraft("other@example.invalid","new","new body",true));QCOMPARE(window.findChild<QListWidget*>("mailAttachments")->count(),0);QCOMPARE(window.findChild<QPlainTextEdit*>("mailBody")->toPlainText(),"new body");QCOMPARE(transport.sends,0);
  window.findChild<QPushButton*>("mailRefresh")->click();QVERIFY(window.operationInProgress());QVERIFY(!window.setExternalDraft("","","must not overwrite",true));QCOMPARE(window.findChild<QPlainTextEdit*>("mailBody")->toPlainText(),"new body");window.findChild<QPushButton*>("mailCancel")->click();QVERIFY(!window.isBusy());QCOMPARE(transport.sends,0);
 }
 void oauthNeedsExplicitAuthorizationAndNeverFallsBackToPassword() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();auto account=mailAccountFromTemplate("gmail","test@gmail.com");account.oauthClientId="test-client.apps.googleusercontent.com";
  MailAccounts accounts(root.filePath("accounts.json"),credentials);QVERIFY(accounts.upsert(account));MockMailTransport transport;
  MailWindow window(root.path(),credentials,&transport);window.show();QVERIFY(window.findChild<QPushButton*>("mailAuthorize")->isEnabled());
  QCOMPARE(transport.lists+transport.fetches+transport.sends,0);window.findChild<QPushButton*>("mailRefresh")->click();QVERIFY(!window.isBusy());QCOMPARE(transport.lists,0);QVERIFY(window.findChild<QLabel*>("mailStatus")->text().contains("浏览器授权"));
  QVERIFY(accounts.saveCredential(account.id,"fake-legacy-password-is-not-a-token"));window.findChild<QPushButton*>("mailRefresh")->click();QVERIFY(!window.isBusy());QCOMPARE(transport.lists,0);QVERIFY(window.findChild<QLabel*>("mailStatus")->text().contains("浏览器授权"));
 }
 void oauthDialogRequiresClientRegistrationAndPinsEndpoints() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();MockMailTransport transport;MailWindow window(root.path(),credentials,&transport);window.show();
  QTimer::singleShot(0,[&]{auto *dialog=QApplication::activeModalWidget();QVERIFY(dialog);auto *provider=dialog->findChild<QComboBox*>("mailProvider");provider->setCurrentIndex(provider->findData("gmail"));
   QVERIFY(!dialog->findChild<QLineEdit*>("mailAccountSecret")->isEnabled());QVERIFY(dialog->findChild<QLineEdit*>("mailImapHost")->isReadOnly());QVERIFY(dialog->findChild<QLineEdit*>("mailSmtpHost")->isReadOnly());
   dialog->findChild<QLineEdit*>("mailAccountLabel")->setText("OAuth fixture");dialog->findChild<QLineEdit*>("mailAccountAddress")->setText("fake@gmail.com");dialog->findChild<QLineEdit*>("mailAccountUser")->setText("fake@gmail.com");
   dialog->findChild<QPushButton*>("mailAccountSave")->click();QVERIFY(dialog->isVisible());QVERIFY(dialog->findChild<QLabel*>("mailAccountError")->text().contains("客户端 ID"));
   dialog->findChild<QLineEdit*>("mailOAuthClientId")->setText("test-client.apps.googleusercontent.com");dialog->findChild<QLineEdit*>("mailOAuthClientSecret")->setText("fixture-only-client-secret");dialog->grab().save("/tmp/mail-oauth-account-mock.png");dialog->findChild<QPushButton*>("mailAccountSave")->click();
  });window.findChild<QPushButton*>("mailAddAccount")->click();QCOMPARE(window.findChild<QListWidget*>("mailAccounts")->count(),1);QCOMPARE(transport.lists+transport.fetches+transport.sends,0);
  QFile f(root.filePath("accounts.json"));QVERIFY(f.open(QIODevice::ReadOnly));QVERIFY(!f.readAll().contains("fixture-only-client-secret"));QCOMPARE(credentials->values.size(),1);QVERIFY(credentials->values.constBegin().value().contains("fixture-only-client-secret"));
 }
 void templatesEnableOAuthWithRegistrationAndAccountSaveDoesNotConnect() {
  QTemporaryDir root;auto credentials=std::make_shared<MockCredentials>();MockMailTransport transport;MailWindow window(root.path(),credentials,&transport);window.show();bool checked=false;
  QTimer::singleShot(0,[&]{auto *dialog=QApplication::activeModalWidget();if(!dialog)qFatal("Expected account dialog");auto *providers=dialog->findChild<QComboBox*>("mailProvider");if(!providers)qFatal("Expected template selector");
   for(const auto &id:{QString("gmail"),QString("outlook")}){const int n=providers->findData(id);QVERIFY(n>=0);QVERIFY(providers->model()->flags(providers->model()->index(n,0))&Qt::ItemIsEnabled);}
   dialog->findChild<QLineEdit*>("mailAccountLabel")->setText("模拟账户");dialog->findChild<QLineEdit*>("mailAccountAddress")->setText("new@example.invalid");dialog->findChild<QLineEdit*>("mailAccountUser")->setText("new@example.invalid");dialog->findChild<QLineEdit*>("mailImapHost")->setText("imap.example.invalid");dialog->findChild<QLineEdit*>("mailSmtpHost")->setText("smtp.example.invalid");
   dialog->grab().save("/tmp/mail-account-configuration-mock.png");checked=true;dialog->findChild<QPushButton*>("mailAccountSave")->click();
  });window.findChild<QPushButton*>("mailAddAccount")->click();QVERIFY(checked);QCOMPARE(window.findChild<QListWidget*>("mailAccounts")->count(),1);QCOMPARE(transport.lists+transport.fetches+transport.sends,0);
 }
};
QTEST_MAIN(MailWindowTest)
#include "test_mailwindow.moc"
