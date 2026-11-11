#include "mailoauth.h"
#include <QTcpSocket>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslSocket>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QSslConfiguration>
#include <QSet>
#include <cmath>
namespace {
bool clean(const QString &s){for(auto c:s)if(c.unicode()<32||c.unicode()==127)return false;return true;}
QByteArray randomUrlBytes(){QByteArray bytes(32,Qt::Uninitialized);for(int i=0;i<32;i+=4){auto n=QRandomGenerator::system()->generate();for(int j=0;j<4;++j)bytes[i+j]=char(n>>(8*j));}return bytes.toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals);}
bool testUrl(const QUrl &url){QHostAddress host;return url.isValid()&&url.scheme()=="http"&&host.setAddress(url.host())&&host.isLoopback()&&url.port()>0&&url.userInfo().isEmpty()&&!url.hasQuery()&&!url.hasFragment();}
QString binding(const MailAccount &a){QJsonArray fields{a.id,a.provider,a.userName,a.address,a.oauthClientId,a.oauthTenant,a.imap.host,a.imap.port,a.smtp.host,a.smtp.port};return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(fields).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());}
QByteArray form(const QList<QPair<QString,QString>> &fields){QByteArray data;for(const auto &p:fields){if(!data.isEmpty())data+='&';data+=QUrl::toPercentEncoding(p.first)+'='+QUrl::toPercentEncoding(p.second);}return data;}
void respond(QTcpSocket *socket,bool success){
 const QByteArray body=success?"Authorization received. Return to LanLink. You may close this tab.":"Invalid authorization response. Return to LanLink.";
 socket->write(QByteArray("HTTP/1.1 ")+(success?"200 OK":"400 Bad Request")+"\r\nContent-Type: text/plain; charset=utf-8\r\nCache-Control: no-store\r\nPragma: no-cache\r\nContent-Security-Policy: default-src 'none'; frame-ancestors 'none'\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);
 socket->disconnectFromHost();
}
}
MailOAuth::MailOAuth(QObject *parent):QObject(parent){
 timeout_.setSingleShot(true);connect(&timeout_,&QTimer::timeout,this,[this]{fail("OAuth 操作超时，请重新授权；未连接邮件服务器。");});
 callbackServer_.setMaxPendingConnections(8);connect(&callbackServer_,&QTcpServer::newConnection,this,&MailOAuth::newConnection);
}
MailOAuth::MailOAuth(LoopbackTestEndpoints endpoints,QObject *parent):MailOAuth(parent){test_=std::move(endpoints);testMode_=true;}
MailOAuth::~MailOAuth(){reset();}
void MailOAuth::setTimeoutMs(int ms){timeoutMs_=qBound(100,ms,300000);}
QByteArray MailOAuth::pkceChallenge(const QByteArray &verifier){return QCryptographicHash::hash(verifier,QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding|QByteArray::OmitTrailingEquals);}
QString MailOAuth::scope(const QString &provider){return provider=="gmail"?QString("https://mail.google.com/"):(provider=="outlook"||provider=="outlook-personal")?QString("offline_access https://outlook.office.com/IMAP.AccessAsUser.All https://outlook.office.com/SMTP.Send"):QString();}
QByteArray MailOAuth::credential(const MailAccount &a,const QString &refreshToken,const QString &clientSecret){
 return QJsonDocument(QJsonObject{{"version",1},{"binding",binding(a)},{"refresh_token",refreshToken},{"client_secret",clientSecret}}).toJson(QJsonDocument::Compact);
}
bool MailOAuth::readCredential(const MailAccount &a,const QByteArray &bytes,QString *refreshToken,QString *clientSecret){
 if(!refreshToken||!clientSecret)return false;refreshToken->clear();clientSecret->clear();if(bytes.size()>32768)return false;
 QJsonParseError error;const auto document=QJsonDocument::fromJson(bytes,&error);const auto o=document.object();
 if(error.error!=QJsonParseError::NoError||!document.isObject()||o.size()!=4||o["version"]!=1||o["binding"]!=binding(a)||!o["refresh_token"].isString()||!o["client_secret"].isString())return false;
 const auto r=o["refresh_token"].toString(),s=o["client_secret"].toString();if(r.size()>16384||s.size()>4096||!clean(r)||!clean(s))return false;
 *refreshToken=r;*clientSecret=s;return true;
}
bool MailOAuth::prepare(const MailAccount &a){
 if(active_)return false;
 if(a.auth!=MailAuth::OAuth2||!validateMailAccount(a).isEmpty()||scope(a.provider).isEmpty()){emit failed("OAuth 账户或客户端注册配置无效。");return false;}
 if(testMode_){if(!testUrl(test_.authorize)||!testUrl(test_.token)){emit failed("OAuth 测试端点必须是明确的回环 HTTP 地址。");return false;}authorizationUrl_=test_.authorize;tokenUrl_=test_.token;}
 else if(a.provider=="gmail"){authorizationUrl_=QUrl("https://accounts.google.com/o/oauth2/v2/auth");tokenUrl_=QUrl("https://oauth2.googleapis.com/token");}
 else {const auto base="https://login.microsoftonline.com/"+a.oauthTenant+"/oauth2/v2.0/";authorizationUrl_=QUrl(base+"authorize");tokenUrl_=QUrl(base+"token");}
 account_=a;active_=true;timeout_.start(timeoutMs_);return true;
}
bool MailOAuth::authorize(const MailAccount &a,QByteArray stored){
 if(!prepare(a))return false;
 if(!stored.isEmpty()&&!readCredential(a,stored,&refresh_,&clientSecret_)){stored.fill('\0');fail("系统 OAuth 凭据与当前账户配置不符，请重新保存客户端配置。");return false;}stored.fill('\0');
 refresh_.clear(); // A new grant must include its own refresh token, not an old identity's.
 if(!callbackServer_.listen(QHostAddress::LocalHost,0)){fail("无法在本机建立 OAuth 回调监听，请检查本机端口策略。");return false;}
 redirect_=QUrl(QString("http://%1:%2/oauth/callback").arg(a.provider=="gmail"?"127.0.0.1":"localhost").arg(callbackServer_.serverPort()));
 verifier_=randomUrlBytes();state_=randomUrlBytes();
 QUrlQuery query;query.addQueryItem("client_id",a.oauthClientId);query.addQueryItem("redirect_uri",redirect_.toString());query.addQueryItem("response_type","code");query.addQueryItem("scope",scope(a.provider));query.addQueryItem("state",QString::fromLatin1(state_));query.addQueryItem("code_challenge",QString::fromLatin1(pkceChallenge(verifier_)));query.addQueryItem("code_challenge_method","S256");query.addQueryItem("login_hint",a.userName);query.addQueryItem("prompt","consent");
 if(a.provider=="gmail")query.addQueryItem("access_type","offline");else query.addQueryItem("response_mode","query");
 auto url=authorizationUrl_;url.setQuery(query);emit authorizationRequired(url);return true;
}
bool MailOAuth::refresh(const MailAccount &a,QByteArray stored){
 if(!prepare(a))return false;
 if(!readCredential(a,stored,&refresh_,&clientSecret_)||refresh_.isEmpty()){stored.fill('\0');fail("缺少匹配的 OAuth 刷新令牌，请点击浏览器授权。");return false;}stored.fill('\0');
 QList<QPair<QString,QString>> fields{{"grant_type","refresh_token"},{"refresh_token",refresh_}};
 if(a.provider!="gmail")fields.append({"scope",scope(a.provider)});exchange(fields);return true;
}
void MailOAuth::newConnection(){
 while(callbackServer_.hasPendingConnections()){
  auto *socket=callbackServer_.nextPendingConnection();socket->setParent(this);socket->setReadBufferSize(16385);
  if(findChildren<QTcpSocket*>(QString(),Qt::FindDirectChildrenOnly).size()>16||!active_){socket->abort();socket->deleteLater();continue;}
  auto buffer=std::make_shared<QByteArray>();connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
  QTimer::singleShot(5000,socket,[socket]{socket->abort();socket->deleteLater();});
  connect(socket,&QTcpSocket::readyRead,this,[this,socket,buffer]{
   *buffer+=socket->readAll();if(buffer->size()>16384){respond(socket,false);return;}
   if(buffer->contains("\r\n\r\n")){socket->disconnect(this);callback(socket,*buffer);buffer->fill('\0');}
  });
 }
}
void MailOAuth::callback(QTcpSocket *socket,QByteArray request){
 if(!active_||state_.isEmpty()){respond(socket,false);return;}
 const auto lines=request.split('\n');const auto first=lines.value(0).trimmed().split(' ');
 if(first.size()!=3||first[0]!="GET"||first[2]!="HTTP/1.1"||!first[1].startsWith('/')){respond(socket,false);return;}
 const auto target=QUrl::fromEncoded(first[1],QUrl::StrictMode);
 if(!target.isValid()||!target.isRelative()||!target.host().isEmpty()||target.path()!=redirect_.path()||target.hasFragment()){respond(socket,false);return;}
 QByteArray host;int hostCount=0;for(const auto &raw:lines){auto line=raw.trimmed();if(line.left(5).toLower()=="host:"){host=line.mid(5).trimmed();++hostCount;}}
 if(hostCount!=1||host!=redirect_.authority().toLatin1()){respond(socket,false);return;}
 const QUrlQuery query(target);QSet<QString> keys;for(const auto &item:query.queryItems()){if(keys.contains(item.first)){respond(socket,false);return;}keys.insert(item.first);}
 if(query.queryItemValue("state",QUrl::FullyDecoded).toLatin1()!=state_){respond(socket,false);return;}
 if(query.hasQueryItem("error")){respond(socket,false);fail("浏览器授权被拒绝或取消，请核对供应商权限配置后重试。");return;}
 const auto code=query.queryItemValue("code",QUrl::FullyDecoded);
 if(code.isEmpty()||code.size()>4096||!clean(code)){respond(socket,false);fail("OAuth 回调未包含有效授权码。");return;}
 respond(socket,true);callbackServer_.close();state_.fill('\0');state_.clear();
 exchange({{"grant_type","authorization_code"},{"code",code},{"redirect_uri",redirect_.toString()},{"code_verifier",QString::fromLatin1(verifier_)}});verifier_.fill('\0');verifier_.clear();
}
void MailOAuth::exchange(const QList<QPair<QString,QString>> &grant){
 auto fields=grant;fields.append({"client_id",account_.oauthClientId});if(!clientSecret_.isEmpty())fields.append({"client_secret",clientSecret_});
 QNetworkRequest request(tokenUrl_);request.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");request.setRawHeader("Accept","application/json");request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
 auto ssl=request.sslConfiguration();ssl.setProtocol(QSsl::TlsV1_2OrLater);ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);request.setSslConfiguration(ssl);
 reply_=network_.post(request,form(fields));reply_->setReadBufferSize(65537);timeout_.start(qMin(timeoutMs_,30000));
 connect(reply_,&QNetworkReply::readyRead,this,[this]{if(!reply_)return;response_+=reply_->readAll();if(response_.size()>65536)fail("OAuth 令牌响应过大，已终止。");});
 connect(reply_,&QNetworkReply::sslErrors,this,[this](const QList<QSslError>&){fail("OAuth 服务器 TLS 证书验证失败。");});
 connect(reply_,&QNetworkReply::finished,this,&MailOAuth::finishReply);
}
void MailOAuth::finishReply(){
 if(!active_||!reply_)return;response_+=reply_->readAll();
 if(reply_->error()!=QNetworkReply::NoError||reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200||response_.size()>65536){fail("OAuth 令牌请求失败。授权可能过期或被撤销；请检查客户端配置并重新授权，不会重发邮件。");return;}
 QJsonParseError error;const auto document=QJsonDocument::fromJson(response_,&error);const auto o=document.object();
 const auto access=o["access_token"].toString(),refresh=o["refresh_token"].toString();const auto expires=o["expires_in"].toDouble(-1);
 if(error.error!=QJsonParseError::NoError||!document.isObject()||o["token_type"].toString().compare("Bearer",Qt::CaseInsensitive)!=0||access.isEmpty()||access.size()>16384||!clean(access)||!std::isfinite(expires)||expires<60||expires>86400||std::floor(expires)!=expires||(o.contains("refresh_token")&&(!o["refresh_token"].isString()||refresh.isEmpty()||refresh.size()>16384||!clean(refresh)))){fail("OAuth 令牌响应无效，未保存或用于邮件认证。");return;}
 if(o.contains("scope")){
  if(!o["scope"].isString()){fail("OAuth 权限响应无效。");return;}
  const auto granted=o["scope"].toString().split(' ',Qt::SkipEmptyParts);
  for(const auto &needed:scope(account_.provider).split(' '))if(needed!="offline_access"&&!granted.contains(needed)){fail("OAuth 授权缺少所需的 IMAP/SMTP 邮件权限。");return;}
 }
 if(!refresh.isEmpty())refresh_=refresh;
 if(refresh_.isEmpty()){fail("供应商未返回刷新令牌，请启用离线访问并重新同意授权。");return;}
 auto saved=credential(account_,refresh_,clientSecret_);reset();emit authorized(access,saved);
}
void MailOAuth::reset(){
 active_=false;timeout_.stop();callbackServer_.close();if(reply_){auto *reply=reply_.data();reply_=nullptr;reply->disconnect(this);reply->abort();reply->deleteLater();}
 for(auto *socket:findChildren<QTcpSocket*>(QString(),Qt::FindDirectChildrenOnly)){socket->disconnect(this);if(socket->state()!=QAbstractSocket::ClosingState)socket->abort();socket->deleteLater();}
 for(auto *secret:{&refresh_,&clientSecret_}){secret->fill(QChar(0));secret->clear();}for(auto *secret:{&state_,&verifier_,&response_}){secret->fill('\0');secret->clear();}
}
void MailOAuth::fail(QString error){if(!active_)return;reset();emit failed(error);}
void MailOAuth::cancel(){if(active_)fail("OAuth 操作已取消。");}
