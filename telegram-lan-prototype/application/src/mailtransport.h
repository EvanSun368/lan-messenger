#pragma once
#include <QObject>
#include <QJsonObject>
#include <QStringList>
#include <QSslSocket>
#include <QTimer>

// Network-only mail adapter. MIME parsing/storage/UI belong to the caller.
struct MailTransportEndpoint {
 enum class Auth { Password, OAuth2 };
 Auth auth=Auth::Password;
 enum class Security { ImplicitTls, StartTls, PlaintextLoopbackTestOnly };
 QString host,username,password;
 quint16 port=0;
 Security security=Security::ImplicitTls;
};
class MailTransport:public QObject {
 Q_OBJECT
public:
 explicit MailTransport(QObject *parent=nullptr);
 virtual bool smtpSend(MailTransportEndpoint endpoint,QString from,QStringList recipients,QByteArray mime);
 virtual bool imapList(MailTransportEndpoint endpoint,QString mailbox="INBOX");
 virtual bool imapFetch(MailTransportEndpoint endpoint,quint64 uid,QString mailbox="INBOX",QString expectedUidValidity={});
 virtual bool imapHeaders(MailTransportEndpoint endpoint,quint64 uid,QString mailbox="INBOX",QString expectedUidValidity={});
 virtual bool busy()const{return active_;}
 virtual void cancel();
 void setTimeoutMs(int ms); // bounded, for cancellation/testing; default 30s.
 static constexpr qsizetype MaxMessageBytes=32*1024*1024;
signals:
 void finished(QJsonObject result); // smtp accepted or IMAP UID list/fetch metadata
 void messageFetched(QByteArray mime); // exact RFC822 literal, not decoded text
 void failed(QString error); // never includes credentials or protocol lines
private:
 enum class Kind{Smtp,List,Fetch,Headers};
 enum class Step{Greeting,Ehlo,StartTls,Authenticating,AuthRejected,MailFrom,Recipient,Data,Body,Select,Search,Fetch};
 bool start(MailTransportEndpoint endpoint,Kind kind);
 void connected();void encrypted();void readyRead();void smtpLine(QByteArray line);void imapLine(QByteArray line);
 void authenticate();void imapCommand(QByteArray command);void write(QByteArray bytes);
 void fail(QString error);void succeed(QJsonObject result);void reset();
 bool secure()const;
 QSslSocket *socket_=nullptr;QTimer timer_;MailTransportEndpoint endpoint_;
 Kind kind_=Kind::Smtp;Step step_=Step::Greeting;bool active_=false,tlsUpgrading_=false,authPayloadSent_=false;
 QByteArray buffer_,smtpReply_,tag_,mime_,literal_;qint64 literalRemaining_=-1;
 QString from_,mailbox_,uidValidity_,expectedUidValidity_;QStringList recipients_;int recipient_=0,tagNumber_=0,smtpCode_=0;
 quint64 uid_=0;
 QList<quint64> uids_;bool gotLiteral_=false;int timeoutMs_=30000;
};
