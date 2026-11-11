#include "mailoauth.h"
#include <QtTest>
#include <QNetworkProxy>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QFile>
#include <functional>

// All traffic stays on numeric IPv4 loopback. Authorization URLs are inspected,
// never opened. Credentials below are invented test fixtures.
namespace {
MailAccount account(const QString &provider="gmail") {
 auto a=mailAccountFromTemplate(provider,"fixture@example.invalid");
 a.oauthClientId="test-public-client"; return a;
}
QByteArray tokenBody(QString refresh="refresh-fixture") {
 QJsonObject o{{"access_token","access-fixture"},{"token_type","Bearer"},{"expires_in",3600}};
 if(!refresh.isNull())o["refresh_token"]=refresh;
 return QJsonDocument(o).toJson(QJsonDocument::Compact);
}
class TokenServer : public QTcpServer {
public:
 QList<QByteArray> requests;
 QByteArray body=tokenBody(), extraHeaders;
 int status=200;
 bool hold=false;
 TokenServer() {
  listen(QHostAddress::LocalHost,0);
  connect(this,&QTcpServer::newConnection,this,[this]{
   while(hasPendingConnections()) {
    auto *s=nextPendingConnection();auto bytes=std::make_shared<QByteArray>();
    connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);
    connect(s,&QTcpSocket::readyRead,s,[this,s,bytes]{
     *bytes+=s->readAll();int header=bytes->indexOf("\r\n\r\n");if(header<0)return;
     int size=0;for(auto line:bytes->left(header).split('\n'))if(line.toLower().startsWith("content-length:"))size=line.mid(15).trimmed().toInt();
     if(bytes->size()<header+4+size||s->property("handled").toBool())return;
     s->setProperty("handled",true);requests.append(*bytes);
     if(hold)return;
     s->write("HTTP/1.1 "+QByteArray::number(status)+" Test\r\nContent-Type: application/json\r\nConnection: close\r\n"+extraHeaders+"Content-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);
     s->disconnectFromHost();
    });
   }
  });
 }
 QUrl url(QString path="/token")const{return QUrl(QString("http://127.0.0.1:%1%2").arg(serverPort()).arg(path));}
 MailOAuth::LoopbackTestEndpoints endpoints()const{return {url("/authorize"),url()};}
 QUrlQuery form(int i=0)const {auto r=requests.at(i);return QUrlQuery(QString::fromLatin1(r.mid(r.indexOf("\r\n\r\n")+4)));}
};
QUrl redirect(const QSignalSpy &spy){return QUrl(QUrlQuery(spy.last().at(0).toUrl()).queryItemValue("redirect_uri",QUrl::FullyDecoded));}
QString state(const QSignalSpy &spy){return QUrlQuery(spy.last().at(0).toUrl()).queryItemValue("state");}
QByteArray request(const QUrl &r,QString stateValue,QString code="code-fixture") {
 QUrlQuery q;q.addQueryItem("state",stateValue);q.addQueryItem("code",code);
 return "GET "+r.path().toLatin1()+"?"+q.toString(QUrl::FullyEncoded).toLatin1()+" HTTP/1.1\r\nHost: "+r.authority().toLatin1()+"\r\n\r\n";
}
bool sendCallback(QTcpSocket &s,const QUrl &r,const QByteArray &bytes) {
 s.connectToHost(QHostAddress::LocalHost,quint16(r.port()));
 if(!s.waitForConnected(1000))return false;
 return s.write(bytes)==bytes.size();
}
class MemoryStore:public MailCredentialStore {
public:
 QByteArray bytes;
 bool save(const QString&,const QByteArray &b,QString*)override{bytes=b;return true;}
 bool load(const QString&,QByteArray *b,QString*)override{*b=bytes;return true;}
 bool remove(const QString&,QString*)override{bytes.clear();return true;}
 bool available()const override{return true;}
 QString unavailableReason()const override{return {};}
};
}
class MailOAuthTests:public QObject {
 Q_OBJECT
private slots:
 void initTestCase(){QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);}
 void pkceVector(){QCOMPARE(MailOAuth::pkceChallenge("dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"),QByteArray("E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"));}
 void authorizationUrl_data(){QTest::addColumn<QString>("provider");QTest::newRow("Google")<<QString("gmail");QTest::newRow("Microsoft")<<QString("outlook");QTest::newRow("Microsoft personal")<<QString("outlook-personal");}
 void authorizationUrl(){
  QFETCH(QString,provider);TokenServer server;QVERIFY(server.isListening());MailOAuth oauth(server.endpoints());auto a=account(provider);
  QSignalSpy url(&oauth,&MailOAuth::authorizationRequired);QVERIFY(oauth.authorize(a));QCOMPARE(url.size(),1);
  QUrl u=url.at(0).at(0).toUrl();QUrlQuery q(u);QCOMPARE(u.host(),QString("127.0.0.1"));QCOMPARE(u.path(),QString("/authorize"));
  QCOMPARE(q.queryItemValue("scope",QUrl::FullyDecoded),MailOAuth::scope(provider));QCOMPARE(q.queryItemValue("client_id"),a.oauthClientId);
  QCOMPARE(q.queryItemValue("response_type"),QString("code"));QCOMPARE(q.queryItemValue("code_challenge_method"),QString("S256"));
  QCOMPARE(q.queryItemValue("login_hint"),a.userName);QCOMPARE(q.queryItemValue("prompt"),QString("consent"));
  QCOMPARE(state(url).size(),43);QCOMPARE(q.queryItemValue("code_challenge").size(),43);
  QUrl r=redirect(url);QCOMPARE(r.scheme(),QString("http"));QCOMPARE(r.host(),provider=="gmail"?QString("127.0.0.1"):QString("localhost"));QVERIFY(r.port()>0);QCOMPARE(r.path(),QString("/oauth/callback"));
  QCOMPARE(q.queryItemValue(provider=="gmail"?"access_type":"response_mode"),provider=="gmail"?QString("offline"):QString("query"));
  QVERIFY(!oauth.authorize(a));QCOMPARE(url.size(),1);QCOMPARE(server.requests.size(),0);oauth.cancel();
 }
 void productionAuthorizationUrlsArePinnedWithoutOpeningBrowser(){
  for(const auto &provider:{QString("gmail"),QString("outlook"),QString("outlook-personal")}){
   MailOAuth oauth;auto a=account(provider);QSignalSpy urls(&oauth,&MailOAuth::authorizationRequired);
   QVERIFY(oauth.authorize(a));QCOMPARE(urls.size(),1);const auto url=urls.first().first().toUrl();QCOMPARE(url.scheme(),QString("https"));
   QCOMPARE(url.host(),provider=="gmail"?QString("accounts.google.com"):QString("login.microsoftonline.com"));
   QCOMPARE(url.path(),provider=="gmail"?QString("/o/oauth2/v2/auth"):"/"+a.oauthTenant+"/oauth2/v2.0/authorize");
   QVERIFY(!QUrlQuery(url).hasQueryItem("client_secret"));oauth.cancel();QVERIFY(!oauth.busy());
  }
 }
 void invalidCallbacks_data(){
  QTest::addColumn<QString>("variant");for(auto v:{"state","method","host","path","duplicate-state","duplicate-code","duplicate-host","fragment","absolute-target","http-version"})QTest::newRow(v)<<QString(v);
 }
 void invalidCallbacks(){
  QFETCH(QString,variant);TokenServer server;MailOAuth oauth(server.endpoints());QSignalSpy url(&oauth,&MailOAuth::authorizationRequired),ok(&oauth,&MailOAuth::authorized),error(&oauth,&MailOAuth::failed);
  QVERIFY(oauth.authorize(account()));QUrl r=redirect(url);auto bytes=request(r,state(url));
  if(variant=="state")bytes.replace(state(url).toLatin1(),"wrong-state");
  if(variant=="method")bytes.replace("GET ","POST ");
  if(variant=="host")bytes.replace(r.authority().toLatin1(),"attacker.invalid");
  if(variant=="path")bytes.replace("/oauth/callback","/elsewhere");
  if(variant=="duplicate-state")bytes.replace(" HTTP/1.1","&state="+state(url).toLatin1()+" HTTP/1.1");
  if(variant=="duplicate-code")bytes.replace(" HTTP/1.1","&code=second HTTP/1.1");
  if(variant=="duplicate-host")bytes.replace("\r\n\r\n","\r\nHost: "+r.authority().toLatin1()+"\r\n\r\n");
  if(variant=="fragment")bytes.replace(" HTTP/1.1","#fragment HTTP/1.1");
  if(variant=="absolute-target")bytes.replace("GET /","GET http://127.0.0.1/");
  if(variant=="http-version")bytes.replace("HTTP/1.1","HTTP/1.0");
  QTcpSocket socket;QVERIFY(sendCallback(socket,r,bytes));QTRY_VERIFY(socket.bytesAvailable()>0);QVERIFY(socket.readAll().startsWith("HTTP/1.1 400"));
  QCOMPARE(server.requests.size(),0);QCOMPARE(ok.size(),0);QCOMPARE(error.size(),0);QVERIFY(oauth.busy());
  QTcpSocket valid;QVERIFY(sendCallback(valid,r,request(r,state(url))));QTRY_COMPARE(ok.size(),1);QCOMPARE(server.requests.size(),1);QCOMPARE(error.size(),0);
 }
 void sequentialInvalidCallbacksDoNotExhaustListener(){
  TokenServer server;MailOAuth oauth(server.endpoints());
  QSignalSpy url(&oauth,&MailOAuth::authorizationRequired),ok(&oauth,&MailOAuth::authorized),error(&oauth,&MailOAuth::failed);
  QVERIFY(oauth.authorize(account()));const auto r=redirect(url);
  // More than the simultaneous-socket limit, but never more than one active
  // callback at a time. Invalid browser requests must not consume a permanent
  // attempt budget and prevent the user's subsequent legitimate callback.
  for(int i=0;i<24;++i){
   QTcpSocket invalid;QVERIFY(sendCallback(invalid,r,request(r,"wrong-state")));
   QTRY_VERIFY_WITH_TIMEOUT(invalid.bytesAvailable()>0||invalid.state()==QAbstractSocket::UnconnectedState,1000);
   QVERIFY2(invalid.readAll().startsWith("HTTP/1.1 400"),qPrintable(QString("Sequential invalid callback %1 was not handled").arg(i+1)));
   QTRY_COMPARE(invalid.state(),QAbstractSocket::UnconnectedState);
   QTest::qWait(1); // Allow deferred socket cleanup before the next connection.
  }
  QCOMPARE(server.requests.size(),0);QCOMPARE(ok.size(),0);QCOMPARE(error.size(),0);QVERIFY(oauth.busy());
  QTcpSocket valid;QVERIFY(sendCallback(valid,r,request(r,state(url))));
  QTRY_COMPARE(ok.size(),1);QCOMPARE(server.requests.size(),1);QCOMPARE(error.size(),0);QVERIFY(!oauth.busy());
 }
 void codeExchangeAndPersistence(){
  TokenServer server;MailOAuth oauth(server.endpoints());auto a=account();QSignalSpy url(&oauth,&MailOAuth::authorizationRequired),ok(&oauth,&MailOAuth::authorized),error(&oauth,&MailOAuth::failed);
  QVERIFY(oauth.authorize(a,MailOAuth::credential(a,"old-refresh","secret-fixture")));QTcpSocket socket;QVERIFY(sendCallback(socket,redirect(url),request(redirect(url),state(url))));
  QTRY_COMPARE(ok.size(),1);QCOMPARE(error.size(),0);QVERIFY(!oauth.busy());QCOMPARE(server.requests.size(),1);
  auto form=server.form();QCOMPARE(form.queryItemValue("grant_type"),QString("authorization_code"));QCOMPARE(form.queryItemValue("code"),QString("code-fixture"));
  QCOMPARE(form.queryItemValue("redirect_uri",QUrl::FullyDecoded),redirect(url).toString());QCOMPARE(form.queryItemValue("client_id"),a.oauthClientId);
  const auto verifier=form.queryItemValue("code_verifier").toLatin1();QCOMPARE(verifier.size(),43);QCOMPARE(QString::fromLatin1(MailOAuth::pkceChallenge(verifier)),QUrlQuery(url.last().at(0).toUrl()).queryItemValue("code_challenge"));
  QCOMPARE(ok.at(0).at(0).toString(),QString("access-fixture"));auto saved=ok.at(0).at(1).toByteArray();QString refresh,secret;QVERIFY(MailOAuth::readCredential(a,saved,&refresh,&secret));QCOMPARE(refresh,QString("refresh-fixture"));QCOMPARE(secret,QString("secret-fixture"));QVERIFY(!saved.contains("access-fixture"));QVERIFY(!saved.contains("access_token"));QVERIFY(!saved.contains("old-refresh"));
  QTemporaryDir temp;auto store=std::make_shared<MemoryStore>();MailAccounts accounts(temp.filePath("accounts.json"),store);QVERIFY(accounts.upsert(a));QVERIFY(accounts.saveCredential(a.id,saved));
  QFile file(temp.filePath("accounts.json"));QVERIFY(file.open(QIODevice::ReadOnly));auto metadata=file.readAll();for(auto value:{"access-fixture","refresh-fixture","secret-fixture","refresh_token","client_secret"})QVERIFY(!metadata.contains(value));QVERIFY(!store->bytes.contains("access-fixture"));
 }
 void refreshRotation_data(){QTest::addColumn<QString>("provider");QTest::addColumn<bool>("rotate");for(auto p:{"gmail","outlook","outlook-personal"})for(bool r:{false,true})QTest::newRow(qPrintable(QString(p)+(r?"-rotate":"-retain")))<<QString(p)<<r;}
 void refreshRotation(){
  QFETCH(QString,provider);QFETCH(bool,rotate);TokenServer server;server.body=tokenBody(rotate?QString("rotated-refresh"):QString());MailOAuth oauth(server.endpoints());auto a=account(provider);
  const QString secret="s&=+ %/?é";QSignalSpy ok(&oauth,&MailOAuth::authorized),url(&oauth,&MailOAuth::authorizationRequired);
  QVERIFY(oauth.refresh(a,MailOAuth::credential(a,"original-refresh",secret)));QTRY_COMPARE(ok.size(),1);QCOMPARE(url.size(),0);auto f=server.form();QCOMPARE(f.queryItemValue("grant_type"),QString("refresh_token"));QCOMPARE(f.queryItemValue("refresh_token"),QString("original-refresh"));QCOMPARE(f.queryItemValue("client_secret",QUrl::FullyDecoded),secret);QVERIFY(server.requests.first().contains("client_secret=s%26%3D%2B%20%25%2F%3F%C3%A9"));
  if(provider!="gmail")QCOMPARE(f.queryItemValue("scope",QUrl::FullyDecoded),MailOAuth::scope(provider));else QVERIFY(!f.hasQueryItem("scope"));
  QString refresh,readSecret;auto saved=ok.first().at(1).toByteArray();QVERIFY(MailOAuth::readCredential(a,saved,&refresh,&readSecret));QCOMPARE(refresh,rotate?QString("rotated-refresh"):QString("original-refresh"));QCOMPARE(readSecret,secret);QVERIFY(!saved.contains("access-fixture"));
 }
 void credentialBinding(){
  auto a=account();const auto bytes=MailOAuth::credential(a,"refresh","secret");QString r,s;QVERIFY(MailOAuth::readCredential(a,bytes,&r,&s));
  QList<std::function<void(MailAccount&)>> mutations={[](auto &a){a.id=QString(32,'a');},[](auto &a){a.provider="outlook";},[](auto &a){a.userName="other@example.invalid";},[](auto &a){a.address="other@example.invalid";},[](auto &a){a.oauthClientId="different";},[](auto &a){a.oauthTenant="different";},[](auto &a){a.imap.host="other.invalid";},[](auto &a){++a.imap.port;},[](auto &a){a.smtp.host="other.invalid";},[](auto &a){++a.smtp.port;}};
  for(auto mutate:mutations){auto changed=a;mutate(changed);r=s="must-clear";QVERIFY(!MailOAuth::readCredential(changed,bytes,&r,&s));QVERIFY(r.isEmpty());QVERIFY(s.isEmpty());}
  auto label=a;label.label="Renamed";QVERIFY(MailOAuth::readCredential(label,bytes,&r,&s));
  for(const auto &invalid:QList<QByteArray>{"{}","[]","not json",QByteArray(32769,'x'),MailOAuth::credential(a,"bad\nrefresh"),MailOAuth::credential(a,"r",QString(4097,'s'))})QVERIFY(!MailOAuth::readCredential(a,invalid,&r,&s));
  auto object=QJsonDocument::fromJson(bytes).object();object["access_token"]="should-not-store";QVERIFY(!MailOAuth::readCredential(a,QJsonDocument(object).toJson(),&r,&s));
  TokenServer server;MailOAuth oauth(server.endpoints());QSignalSpy error(&oauth,&MailOAuth::failed);auto changed=a;changed.oauthClientId="different";QVERIFY(!oauth.refresh(changed,bytes));QCOMPARE(error.size(),1);QCOMPARE(server.requests.size(),0);QVERIFY(!oauth.busy());
 }
 void cancellationAndStaleCallback(){
  TokenServer server;MailOAuth oauth(server.endpoints());QSignalSpy url(&oauth,&MailOAuth::authorizationRequired),ok(&oauth,&MailOAuth::authorized),error(&oauth,&MailOAuth::failed);auto a=account();
  QVERIFY(oauth.authorize(a));auto oldState=state(url);auto oldRedirect=redirect(url);oauth.cancel();QCOMPARE(error.size(),1);QVERIFY(!oauth.busy());oauth.cancel();QCOMPARE(error.size(),1);
  QVERIFY(oauth.authorize(a));QVERIFY(state(url)!=oldState);QTcpSocket stale;QVERIFY(sendCallback(stale,redirect(url),request(redirect(url),oldState)));QTRY_VERIFY(stale.bytesAvailable()>0);QVERIFY(stale.readAll().startsWith("HTTP/1.1 400"));QCOMPARE(server.requests.size(),0);QCOMPARE(ok.size(),0);oauth.cancel();
  QTcpSocket closed;closed.connectToHost(QHostAddress::LocalHost,quint16(oldRedirect.port()));QVERIFY(!closed.waitForConnected(200));
  server.hold=true;QVERIFY(oauth.refresh(a,MailOAuth::credential(a,"refresh")));QTRY_COMPARE(server.requests.size(),1);oauth.cancel();QTest::qWait(50);QCOMPARE(ok.size(),0);QVERIFY(!oauth.busy());
 }
 void timeout(){
  TokenServer server;MailOAuth oauth(server.endpoints());oauth.setTimeoutMs(100);
  QSignalSpy error(&oauth,&MailOAuth::failed),ok(&oauth,&MailOAuth::authorized);
  QVERIFY(oauth.authorize(account()));QTRY_COMPARE(error.size(),1);QVERIFY(!oauth.busy());QCOMPARE(server.requests.size(),0);
  server.hold=true;auto a=account();QVERIFY(oauth.refresh(a,MailOAuth::credential(a,"r")));
  QTRY_COMPARE(error.size(),2);QCOMPARE(server.requests.size(),1);QCOMPARE(ok.size(),0);QVERIFY(!oauth.busy());
 }
 void callbackReplay(){
  TokenServer server;server.hold=true;MailOAuth oauth(server.endpoints());
  QSignalSpy url(&oauth,&MailOAuth::authorizationRequired),ok(&oauth,&MailOAuth::authorized);
  QVERIFY(oauth.authorize(account()));const auto r=redirect(url);const auto bytes=request(r,state(url));
  QTcpSocket first,replay;first.connectToHost(QHostAddress::LocalHost,quint16(r.port()));replay.connectToHost(QHostAddress::LocalHost,quint16(r.port()));
  QVERIFY(first.waitForConnected(1000));QVERIFY(replay.waitForConnected(1000));
  QTest::qWait(20); // Both sockets are accepted before the single-use callback closes its listener.
  QCOMPARE(first.write(bytes),qint64(bytes.size()));QTRY_COMPARE(server.requests.size(),1);
  QCOMPARE(replay.write(bytes),qint64(bytes.size()));QTRY_VERIFY(replay.bytesAvailable()>0);
  QVERIFY(replay.readAll().startsWith("HTTP/1.1 400"));QCOMPARE(server.requests.size(),1);QCOMPARE(ok.size(),0);oauth.cancel();
 }
 void validGrantedScopes(){
  for(auto provider:{QString("gmail"),QString("outlook"),QString("outlook-personal")}){
   TokenServer server;auto object=QJsonDocument::fromJson(tokenBody()).object();
   object["scope"]=MailOAuth::scope(provider).remove("offline_access ");object["token_type"]="bEaReR";
   server.body=QJsonDocument(object).toJson();MailOAuth oauth(server.endpoints());auto a=account(provider);
   QSignalSpy ok(&oauth,&MailOAuth::authorized);QVERIFY(oauth.refresh(a,MailOAuth::credential(a,"r")));QTRY_COMPARE(ok.size(),1);
  }
 }
 void refusal(){
  TokenServer server;MailOAuth oauth(server.endpoints());QSignalSpy url(&oauth,&MailOAuth::authorizationRequired),error(&oauth,&MailOAuth::failed);QVERIFY(oauth.authorize(account()));auto r=redirect(url);auto bytes=request(r,state(url));bytes.replace("code=code-fixture","error=access_denied&error_description=PRIVATE_REMOTE_TEXT");QTcpSocket socket;QVERIFY(sendCallback(socket,r,bytes));QTRY_COMPARE(error.size(),1);QVERIFY(!error.first().first().toString().contains("PRIVATE_REMOTE_TEXT"));QCOMPARE(server.requests.size(),0);QVERIFY(!oauth.busy());
 }
 void invalidTokenResponse_data(){
  QTest::addColumn<QByteArray>("body");QTest::addColumn<int>("status");
  QTest::newRow("invalid-json")<<QByteArray("PRIVATE_REMOTE_TEXT")<<200;QTest::newRow("array")<<QByteArray("[]")<<200;QTest::newRow("http-error")<<QByteArray("PRIVATE_REMOTE_TEXT")<<400;QTest::newRow("oversize")<<QByteArray(66000,'x')<<200;
  auto base=QJsonDocument::fromJson(tokenBody()).object();
  auto add=[&](const char *name,QString key,QJsonValue value){auto o=base;o[key]=value;QTest::newRow(name)<<QJsonDocument(o).toJson()<<200;};
  add("wrong-type","token_type","MAC");add("empty-access","access_token","");add("control-access","access_token","PRIVATE_REMOTE_TEXT\n");add("huge-access","access_token",QString(16385,'x'));
  add("expires-string","expires_in","3600");add("expires-small","expires_in",59);add("expires-large","expires_in",86401);add("expires-fraction","expires_in",60.5);
  add("empty-refresh","refresh_token","");add("numeric-refresh","refresh_token",42);add("control-refresh","refresh_token","PRIVATE_REMOTE_TEXT\r");add("missing-mail-scope","scope","openid");add("scope-type","scope",42);
 }
 void invalidTokenResponse(){
  QFETCH(QByteArray,body);QFETCH(int,status);TokenServer server;server.body=body;server.status=status;MailOAuth oauth(server.endpoints());auto a=account();QSignalSpy error(&oauth,&MailOAuth::failed),ok(&oauth,&MailOAuth::authorized);QVERIFY(oauth.refresh(a,MailOAuth::credential(a,"refresh-fixture","secret-fixture")));QTRY_COMPARE(error.size(),1);QCOMPARE(ok.size(),0);QVERIFY(!oauth.busy());auto message=error.first().first().toString();for(auto secret:{"PRIVATE_REMOTE_TEXT","access-fixture","refresh-fixture","secret-fixture"})QVERIFY(!message.contains(secret));
 }
 void newGrantRequiresRefresh(){
  TokenServer server;server.body=tokenBody(QString());MailOAuth oauth(server.endpoints());auto a=account();QSignalSpy url(&oauth,&MailOAuth::authorizationRequired),error(&oauth,&MailOAuth::failed),ok(&oauth,&MailOAuth::authorized);QVERIFY(oauth.authorize(a,MailOAuth::credential(a,"old-refresh")));QTcpSocket socket;QVERIFY(sendCallback(socket,redirect(url),request(redirect(url),state(url))));QTRY_COMPARE(error.size(),1);QCOMPARE(ok.size(),0);
 }
 void redirectsNotFollowed(){
  TokenServer server,destination;server.status=302;server.extraHeaders="Location: "+destination.url().toEncoded()+"\r\n";MailOAuth oauth(server.endpoints());auto a=account();QSignalSpy error(&oauth,&MailOAuth::failed);QVERIFY(oauth.refresh(a,MailOAuth::credential(a,"r")));QTRY_COMPARE(error.size(),1);QCOMPARE(server.requests.size(),1);QCOMPARE(destination.requests.size(),0);
 }
 void endpointOverrideRestrictions(){
  TokenServer server;for(auto url:{"http://192.0.2.1:8080/token","https://127.0.0.1:8080/token","http://localhost:8080/token","http://127.0.0.1/token","http://user@127.0.0.1:8080/token","http://127.0.0.1:8080/token?query=x","http://127.0.0.1:8080/token#fragment"})for(bool token:{false,true}){
   auto endpoints=server.endpoints();(token?endpoints.token:endpoints.authorize)=QUrl(url);MailOAuth oauth(endpoints);QSignalSpy error(&oauth,&MailOAuth::failed),auth(&oauth,&MailOAuth::authorizationRequired);QVERIFY(!oauth.authorize(account()));QCOMPARE(error.size(),1);QCOMPARE(auth.size(),0);QVERIFY(!oauth.busy());
  }QCOMPARE(server.requests.size(),0);
 }
};
QTEST_GUILESS_MAIN(MailOAuthTests)
#include "test_mailoauth.moc"
