#pragma once
#include "mailaccounts.h"
#include <QObject>
#include <QTcpServer>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QUrl>
class QNetworkReply;

// Public-client authorization code + S256 PKCE. No built-in client credentials,
// embedded webview, device flow, implicit grant, or token in account metadata.
class MailOAuth final : public QObject {
 Q_OBJECT
public:
 explicit MailOAuth(QObject *parent=nullptr);
 // Only for local fake-server tests. Cannot override a production HTTPS endpoint.
 struct LoopbackTestEndpoints { QUrl authorize,token; };
 explicit MailOAuth(LoopbackTestEndpoints endpoints,QObject *parent=nullptr);
 ~MailOAuth() override;
 bool authorize(const MailAccount&,QByteArray storedCredential={});
 bool refresh(const MailAccount&,QByteArray storedCredential);
 void cancel();
 bool busy() const {return active_;}
 void setTimeoutMs(int ms);
 static QByteArray credential(const MailAccount&,const QString &refreshToken,const QString &clientSecret={});
 static bool readCredential(const MailAccount&,const QByteArray&,QString *refreshToken,QString *clientSecret);
 static QString scope(const QString &provider);
 static QByteArray pkceChallenge(const QByteArray &verifier);
signals:
 void authorizationRequired(QUrl url); // UI opens system browser only on explicit login.
 void authorized(QString accessToken,QByteArray storedCredential);
 void failed(QString safeError); // Never contains remote text, codes, or tokens.
private:
 bool prepare(const MailAccount&);
 void newConnection();
 void callback(QTcpSocket*,QByteArray request);
 void exchange(const QList<QPair<QString,QString>>&);
 void finishReply();
 void fail(QString);
 void reset();
 QUrl authorizationUrl_,tokenUrl_,redirect_;
 LoopbackTestEndpoints test_;bool testMode_=false,active_=false;
 MailAccount account_;QString refresh_,clientSecret_;QByteArray verifier_,state_,response_;
 QTcpServer callbackServer_;QNetworkAccessManager network_;QPointer<QNetworkReply> reply_;QTimer timeout_;
 int timeoutMs_=180000;
};
