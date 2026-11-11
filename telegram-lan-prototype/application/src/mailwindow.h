#pragma once
#include <QDialog>
#include <QPointer>
#include <QTimer>
#include <QHash>
#include <QSet>
#include <memory>
#include <functional>
#include "mailaccounts.h"
#include "mailoauth.h"
#include "mailtransport.h"
#include "mailmime.h"
#include "mailsyncstate.h"
class QListWidget;class QTreeWidget;class QPlainTextEdit;class QLineEdit;class QLabel;class QPushButton;class QTabWidget;class QCloseEvent;class QCheckBox;class QComboBox;class QSystemTrayIcon;
// Account-isolated mail controller. Polling runs only for explicitly enabled
// accounts, independently of LAN identity; SMTP is always an explicit action.
class MailWindow final : public QDialog {
 Q_OBJECT
public:
 explicit MailWindow(QString configurationDirectory={},QWidget *parent=nullptr);
 MailWindow(QString configurationDirectory,std::shared_ptr<MailCredentialStore> credentials,MailTransport *transport,QWidget *parent=nullptr);
 ~MailWindow() override;
 void reject() override;
 bool shutdown(); // Confirm/stop before application exit, unlike hiding the pane.
 bool hasDraft() const;
 bool isBusy() const;
 bool operationInProgress() const {return isBusy()&&!background_;}
 bool setExternalDraft(QString recipient,QString subject,QString body,bool replaceConfirmed=false);
signals:
 void newMailAvailable(QString accountId,int count);
protected:
 void closeEvent(QCloseEvent *event) override;
private:
 enum class Operation{None,List,Headers,Fetch,Send,Authorize};
 struct Summary {QString from,subject,date;};
 struct Inbox {QString validity;QList<quint64> uids;QHash<QString,Summary> summaries;QSet<QString> unread;};
 struct Draft {QString recipients,subject,body;QStringList paths;QString inReplyTo;QStringList references;};
 void buildUi();void refreshAccounts(QString selected={});void accountSelected();void renderInbox();void refreshBadges();
 void editAccount(bool existing);void removeAccount();MailAccount currentAccount()const;MailAccount accountById(QString)const;
 void withEndpoint(const MailAccount&,bool smtp,std::function<void(MailTransportEndpoint)> ready);void authorizeAccount();
 void refreshInbox();void startList(const MailAccount&,bool background);void fetchSelected();void replySelected();void sendDraft();void addAttachments();void saveReceivedAttachment();void clearReceived();
 void cancelOperation();void updateAvailability();void status(QString,bool error=false);
 void transportFinished(QJsonObject);void transportFailed(QString);void messageFetched(QByteArray);
 void pollDue();void nextHeaders();void loadPreferences();bool savePreferences();void notifyNewMail(QString,int);void finishOperation(bool success);void saveDraft(QString);void restoreDraft(QString);bool confirmClose();
 std::unique_ptr<MailAccounts> accounts_;std::unique_ptr<MailSyncState> syncState_;QPointer<MailTransport> transport_;
 MailOAuth *oauth_=nullptr;std::function<void(QString)> oauthReady_;
 QString directory_,selectedId_,operationAccountId_,fetchingUid_,fetchingValidity_;Operation operation_=Operation::None;bool parsedFetch_=false,background_=false,shuttingDown_=false,preferencesLoading_=false;
 QByteArray pendingMime_;MailMime::Message receivedMessage_;QString receivedAccountId_,receivedValidity_,receivedUid_,draftReplyTo_;QStringList draftReferences_;
 QHash<QString,Inbox> inboxes_;QHash<QString,Draft> drafts_;QSet<QString> syncEnabled_;QHash<QString,qint64> nextSync_;QHash<QString,int> failures_;QList<quint64> headerQueue_;QString headerAccount_,headerValidity_;QTimer syncTimer_;int pollIntervalMs_=120000;
 QListWidget *accountList_=nullptr,*attachmentList_=nullptr,*receivedAttachments_=nullptr;QList<MailMime::Attachment> receivedFiles_;
 QTreeWidget *inbox_=nullptr;QPlainTextEdit *reader_=nullptr,*body_=nullptr;QLineEdit *recipients_=nullptr,*subject_=nullptr;
 QLabel *status_=nullptr,*credentialNotice_=nullptr,*messageInfo_=nullptr,*sender_=nullptr;
 QPushButton *addAccount_=nullptr,*editAccount_=nullptr,*removeAccount_=nullptr,*refresh_=nullptr,*read_=nullptr,*reply_=nullptr,*send_=nullptr,*cancel_=nullptr,*attach_=nullptr,*removeAttachment_=nullptr,*saveAttachment_=nullptr,*authorize_=nullptr;
 QCheckBox *autoSync_=nullptr,*notify_=nullptr;QComboBox *interval_=nullptr;QSystemTrayIcon *tray_=nullptr;QString notificationAccount_;
 QWidget *accountView_=nullptr,*inboxView_=nullptr;QTabWidget *tabs_=nullptr;QStringList attachmentPaths_;
};
