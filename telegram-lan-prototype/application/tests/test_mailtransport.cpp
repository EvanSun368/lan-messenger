#include "mailtransport.h"
#include <QtTest>
#include <QTcpServer>
#include <QFile>
#include <QFileInfo>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslConfiguration>
#include <QJsonArray>
class MailFakeServer:public QTcpServer {
public:
 enum Mode{Smtp,Imap,DropBeforeDataAck,NoStartTls,RejectStartTls,RejectAuth,WrongUid,SelfSigned,TlsSmtp,TlsImap,StartTlsSmtp,StartTlsImap};
 QByteArray validityResponse="* OK [UIDVALIDITY 123] epoch\r\n";QByteArray searchResponse="12 99";
 Mode mode;bool rejectOAuth=false,oauthCompleted=false;QByteArray oauthPayload;bool encryptedSession=false,authBeforeTls=false;QByteArray received,data;QByteArray message=QString("Subject: 中文\r\nContent-Type: application/octet-stream\r\n\r\n中文附件").toUtf8()+QByteArray::fromHex("000102ff")+"\r\n";
 explicit MailFakeServer(Mode mode):mode(mode){listen(QHostAddress::LocalHost,0);}
protected:
 void incomingConnection(qintptr descriptor)override {
  auto *s=new QSslSocket(this);s->setSocketDescriptor(descriptor);
  const bool imap=mode==Imap||mode==WrongUid||mode==TlsImap||mode==StartTlsImap;
  const bool implicit=mode==SelfSigned||mode==TlsSmtp||mode==TlsImap;
  if(implicit||mode==StartTlsSmtp||mode==StartTlsImap){
   const auto path=QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()+"/fixtures/";
   QFile cert(path+"mail-fixture.crt"),key(path+"mail-fixture.key");
   if(!cert.open(QIODevice::ReadOnly)||!key.open(QIODevice::ReadOnly)){s->abort();return;}
   s->setLocalCertificate(QSslCertificate(cert.readAll()));s->setPrivateKey(QSslKey(key.readAll(),QSsl::Rsa));
   s->setPeerVerifyMode(QSslSocket::VerifyNone);
   connect(s,&QSslSocket::encrypted,s,[this,s,implicit,imap]{encryptedSession=true;if(implicit)s->write(imap?"* OK test\r\n":"220 test\r\n");});
  }
  if(implicit)s->startServerEncryption();else s->write(imap?"* OK test\r\n":"220 test\r\n");
  auto buffer=std::make_shared<QByteArray>();auto body=std::make_shared<bool>(false);auto oauthTag=std::make_shared<QByteArray>();auto oauthStage=std::make_shared<int>(0);
  connect(s,&QSslSocket::readyRead,s,[this,s,buffer,body,imap,oauthTag,oauthStage]{
   auto bytes=s->readAll();received+=bytes;*buffer+=bytes;
   while(true){
    if(*body){auto end=buffer->indexOf("\r\n.\r\n");if(end<0)return;data=buffer->left(end+2);buffer->remove(0,end+5);*body=false;
     if(mode==DropBeforeDataAck){s->disconnectFromHost();return;}s->write("250 queued\r\n");continue;}
    auto end=buffer->indexOf("\r\n");if(end<0)return;auto line=buffer->left(end);buffer->remove(0,end+2);
    if(*oauthStage){
     if(*oauthStage==1){oauthPayload=QByteArray::fromBase64(line);if(rejectOAuth){*oauthStage=2;s->write("+ token-error-do-not-expose\r\n");}else{*oauthStage=0;oauthCompleted=true;s->write(*oauthTag+" OK authenticated\r\n");}}
     else{*oauthStage=0;oauthCompleted=line.isEmpty();s->write(imap?*oauthTag+" NO token-error-do-not-expose\r\n":"535 token-error-do-not-expose\r\n");}
     continue;
    }
    if(imap){
     auto tag=line.left(line.indexOf(' '));auto command=line.mid(tag.size()+1);
     if(command=="STARTTLS"&&mode==StartTlsImap){s->write(tag+" OK begin TLS\r\n");s->flush();s->startServerEncryption();}
     else if(command=="AUTHENTICATE XOAUTH2"){if((mode==TlsImap||mode==StartTlsImap)&&!s->isEncrypted())authBeforeTls=true;*oauthTag=tag;*oauthStage=1;s->write("+\r\n");}
     else if(command.startsWith("LOGIN")){if((mode==TlsImap||mode==StartTlsImap)&&!s->isEncrypted())authBeforeTls=true;s->write(tag+" OK logged in\r\n");}
     else if(command.startsWith("EXAMINE"))s->write("* 2 EXISTS\r\n"+validityResponse+tag+" OK selected\r\n");
     else if(command.startsWith("UID SEARCH"))s->write("* SEARCH "+searchResponse+"\r\n"+tag+" OK search\r\n");
     else if(command.startsWith("UID FETCH")){
      const auto prefix="* 1 FETCH (UID "+QByteArray(mode==WrongUid?"88":"99")+(command.contains("HEADER.FIELDS")?" BODY[HEADER.FIELDS (FROM SUBJECT)] {":" BODY[] {")+QByteArray::number(message.size())+"}\r\n";
      s->write(prefix+message.first(7));QPointer<QSslSocket> weak(s);
      QTimer::singleShot(5,s,[this,weak,tag]{if(weak)weak->write(message.mid(7)+")\r\n"+tag+" OK fetch\r\n");});
     }else s->write(tag+" BAD unsupported\r\n");
    }else{
     if(line.startsWith("EHLO"))s->write((mode==RejectStartTls||mode==StartTlsSmtp)?"250-test\r\n250 STARTTLS\r\n":"250-test\r\n250 AUTH PLAIN\r\n");
     else if(line=="STARTTLS"){if(mode==StartTlsSmtp){s->write("220 begin TLS\r\n");s->flush();s->startServerEncryption();}else s->write("454 unavailable\r\n");}
     else if(line.startsWith("AUTH XOAUTH2 ")){if((mode==TlsSmtp||mode==StartTlsSmtp)&&!s->isEncrypted())authBeforeTls=true;oauthPayload=QByteArray::fromBase64(line.mid(12));if(rejectOAuth){*oauthStage=2;s->write("334 token-error-do-not-expose\r\n");}else{oauthCompleted=true;s->write("235 authenticated\r\n");}}
     else if(line.startsWith("AUTH")){if((mode==TlsSmtp||mode==StartTlsSmtp)&&!s->isEncrypted())authBeforeTls=true;s->write(mode==RejectAuth?"535 rejected\r\n":"235 authenticated\r\n");}
     else if(line.startsWith("MAIL FROM")||line.startsWith("RCPT TO"))s->write("250 ok\r\n");
     else if(line=="DATA"){s->write("354 send\r\n");*body=true;}
    }
   }
  });
 }
};
class MailTransportTest:public QObject {
 Q_OBJECT
 MailTransportEndpoint config(MailFakeServer &server,MailTransportEndpoint::Security security=MailTransportEndpoint::Security::PlaintextLoopbackTestOnly){
  MailTransportEndpoint result;result.host="127.0.0.1";result.port=server.serverPort();result.username="mail-test-user";result.password="test-only-password";result.security=security;return result;
 }
private slots:
 void validityIsRequiredBeforeSearchOrFetch(){
  for(const auto &response:QList<QByteArray>{"", "* OK [UIDVALIDITY 0] invalid\r\n", "* OK [UIDVALIDITY 4294967296] invalid\r\n", "* OK [UIDVALIDITY abc] invalid\r\n", "* OK [UIDVALIDITY 1] one\r\n* OK [UIDVALIDITY 2] two\r\n"}){
   MailFakeServer server(MailFakeServer::Imap);server.validityResponse=response;
   MailTransport client;QSignalSpy failed(&client,&MailTransport::failed),done(&client,&MailTransport::finished);
   QVERIFY(client.imapList(config(server)));QTRY_COMPARE(failed.size(),1);QCOMPARE(done.size(),0);QVERIFY(!server.received.contains("UID SEARCH"));
  }
 }
 void rolloverNeverFetchesStaleUid(){
  MailFakeServer server(MailFakeServer::Imap);MailTransport client;QSignalSpy failed(&client,&MailTransport::failed),fetched(&client,&MailTransport::messageFetched);
  QVERIFY(client.imapFetch(config(server),99,"INBOX","122"));QTRY_COMPARE(failed.size(),1);QCOMPARE(fetched.size(),0);QVERIFY(!server.received.contains("UID FETCH"));
 }
 void boundedHeaderFetchAndMetadata(){
  MailFakeServer server(MailFakeServer::Imap);MailTransport client;QSignalSpy done(&client,&MailTransport::finished),failed(&client,&MailTransport::failed);
  QVERIFY(client.imapHeaders(config(server),99,"INBOX","123"));QTRY_COMPARE(done.size(),1);
  const auto result=done.first().first().toJsonObject();QCOMPARE(result["uidValidity"].toString(),QString("123"));QCOMPARE(result["mailbox"].toString(),QString("INBOX"));QVERIFY(result["headersOnly"].toBool());
  QVERIFY(server.received.contains("BODY.PEEK[HEADER.FIELDS (FROM TO SUBJECT DATE MESSAGE-ID REPLY-TO REFERENCES IN-REPLY-TO)]"));
  server.message=QByteArray(65537,'x');QVERIFY(client.imapHeaders(config(server),99,"INBOX","123"));QTRY_COMPARE(failed.size(),1);QCOMPARE(done.size(),1);
 }
 void invalidUidLimits(){
  MailFakeServer server(MailFakeServer::Imap);MailTransport client;QSignalSpy failed(&client,&MailTransport::failed);
  QVERIFY(!client.imapFetch(config(server),4294967296ULL));QVERIFY(!client.imapFetch(config(server),99,"INBOX","0"));QCOMPARE(failed.size(),2);
  server.searchResponse="12 4294967296";QVERIFY(client.imapList(config(server)));QTRY_COMPARE(failed.size(),3);
 }

 void oauthSuccessAndRejection(){
  for(bool imap:{false,true})for(bool reject:{false,true}){
   MailFakeServer server(imap?MailFakeServer::Imap:MailFakeServer::Smtp);server.rejectOAuth=reject;
   MailTransport client;QSignalSpy finished(&client,&MailTransport::finished),failed(&client,&MailTransport::failed);
   auto endpoint=config(server);endpoint.auth=MailTransportEndpoint::Auth::OAuth2;endpoint.password="test-access-token";
   if(imap)QVERIFY(client.imapList(endpoint));else QVERIFY(client.smtpSend(endpoint,"from@example.test",{"to@example.test"},"Subject: oauth\r\n\r\nbody"));
   QTRY_VERIFY(!finished.isEmpty()||!failed.isEmpty());
   QCOMPARE(failed.size(),reject?1:0);QCOMPARE(finished.size(),reject?0:1);
   QCOMPARE(server.oauthPayload,QByteArray("user=mail-test-user")+char(1)+"auth=Bearer test-access-token"+char(1)+char(1));
   QVERIFY(server.oauthCompleted);QVERIFY(!server.received.contains("LOGIN "));QVERIFY(!server.received.contains("AUTH PLAIN"));
   if(reject){QVERIFY(!server.received.contains("MAIL FROM"));QVERIFY(!server.received.contains("EXAMINE"));QVERIFY(!failed.first().first().toString().contains("test-access-token"));QVERIFY(!failed.first().first().toString().contains("token-error-do-not-expose"));}
  }
 }
 void oauthNeverBypassesTls(){
  for(auto mode:{MailFakeServer::NoStartTls,MailFakeServer::RejectStartTls,MailFakeServer::SelfSigned}){
   MailFakeServer server(mode);MailTransport client;QSignalSpy failed(&client,&MailTransport::failed);
   auto endpoint=config(server,mode==MailFakeServer::SelfSigned?MailTransportEndpoint::Security::ImplicitTls:MailTransportEndpoint::Security::StartTls);
   endpoint.auth=MailTransportEndpoint::Auth::OAuth2;endpoint.password="test-access-token";
   QVERIFY(client.smtpSend(endpoint,"from@example.test",{"to@example.test"},"Subject: oauth\r\n\r\nbody"));QTRY_COMPARE(failed.size(),1);
   QVERIFY(!server.received.contains("AUTH"));QVERIFY(server.oauthPayload.isEmpty());
  }
 }
 void smtpRequiresFinalAckAndPreservesMime(){
  MailFakeServer server(MailFakeServer::Smtp);MailTransport client;QSignalSpy finished(&client,&MailTransport::finished),failed(&client,&MailTransport::failed);
  QByteArray mime="Subject: =?UTF-8?B?5Lit5paH?=\r\nContent-Type: multipart/mixed; boundary=x\r\n\r\n--x\r\nContent-Transfer-Encoding: base64\r\n\r\nAAH/\r\n--x--\r\n.dot\r\n";
  QVERIFY(client.smtpSend(config(server),"from@example.test",{"to@example.test"},mime));QTRY_COMPARE(finished.size(),1);QCOMPARE(failed.size(),0);
  QVERIFY(finished.first().first().toJsonObject()["accepted"].toBool());QVERIFY(server.data.contains("..dot\r\n"));QVERIFY(server.data.contains("AAH/"));
  QCOMPARE(server.data,mime.replace("\r\n.dot","\r\n..dot"));
 }
 void disconnectedBeforeFinalDataAckIsNotSuccess(){
  MailFakeServer server(MailFakeServer::DropBeforeDataAck);MailTransport client;QSignalSpy finished(&client,&MailTransport::finished),failed(&client,&MailTransport::failed);
  QVERIFY(client.smtpSend(config(server),"from@example.test",{"to@example.test"},"Subject: test\r\n\r\nbody\r\n"));QTRY_COMPARE(failed.size(),1);QCOMPARE(finished.size(),0);QVERIFY(failed.first().first().toString().contains("结果未知"));
 }
 void imapListsAndFetchesSplitBinaryLiteral(){
  MailFakeServer server(MailFakeServer::Imap);MailTransport client;QSignalSpy finished(&client,&MailTransport::finished),failed(&client,&MailTransport::failed),fetched(&client,&MailTransport::messageFetched);
  QVERIFY(client.imapList(config(server)));QTRY_COMPARE(finished.size(),1);
  QCOMPARE(finished.first().first().toJsonObject()["uidValidity"].toString(),QString("123"));
  QCOMPARE(finished.first().first().toJsonObject()["uids"].toArray(),QJsonArray({"12","99"}));finished.clear();
  QVERIFY(client.imapFetch(config(server),99));QTRY_COMPARE(fetched.size(),1);QTRY_COMPARE(finished.size(),1);QCOMPARE(failed.size(),0);
  QCOMPARE(fetched.first().first().toByteArray(),server.message);
 }
 void incorrectUidCannotReturnOtherMessage(){
  MailFakeServer server(MailFakeServer::WrongUid);MailTransport client;QSignalSpy failed(&client,&MailTransport::failed),fetched(&client,&MailTransport::messageFetched);
  QVERIFY(client.imapFetch(config(server),99));QTRY_COMPARE(failed.size(),1);QCOMPARE(fetched.size(),0);
 }
 void startTlsNeverDowngradesOrAuthenticates(){
  for(auto mode:{MailFakeServer::NoStartTls,MailFakeServer::RejectStartTls}){
   MailFakeServer server(mode);MailTransport client;QSignalSpy failed(&client,&MailTransport::failed);
   QVERIFY(client.smtpSend(config(server,MailTransportEndpoint::Security::StartTls),"from@example.test",{"to@example.test"},"Subject: test\r\n\r\nbody"));QTRY_COMPARE(failed.size(),1);
   QVERIFY(!server.received.contains("AUTH"));QVERIFY(!server.received.contains("test-only-password"));
  }
 }
 void selfSignedTlsIsRejectedBeforeCredentials(){
  QVERIFY(QSslSocket::supportsSsl());MailFakeServer server(MailFakeServer::SelfSigned);MailTransport client;QSignalSpy failed(&client,&MailTransport::failed),finished(&client,&MailTransport::finished);
  QVERIFY(client.smtpSend(config(server,MailTransportEndpoint::Security::ImplicitTls),"from@example.test",{"to@example.test"},"Subject: test\r\n\r\nbody"));QTRY_COMPARE(failed.size(),1);
  QCOMPARE(finished.size(),0);QVERIFY(server.received.isEmpty());
 }
 void explicitAuthenticationRejectionIsFailure(){
  MailFakeServer server(MailFakeServer::RejectAuth);MailTransport client;QSignalSpy failed(&client,&MailTransport::failed),finished(&client,&MailTransport::finished);
  QVERIFY(client.smtpSend(config(server),"from@example.test",{"to@example.test"},"Subject: test\r\n\r\nbody"));QTRY_COMPARE(failed.size(),1);QCOMPARE(finished.size(),0);
  QVERIFY(!server.received.contains("MAIL FROM"));QVERIFY(!failed.first().first().toString().contains("test-only-password"));
 }
 void cancellationAndTimeoutAreBounded(){
  QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost,0));MailTransport client;client.setTimeoutMs(100);QSignalSpy failed(&client,&MailTransport::failed);
  MailTransportEndpoint endpoint;endpoint.host="127.0.0.1";endpoint.port=server.serverPort();endpoint.username="test";endpoint.password="only";endpoint.security=MailTransportEndpoint::Security::PlaintextLoopbackTestOnly;
  QVERIFY(client.imapList(endpoint));QTRY_COMPARE(failed.size(),1);QVERIFY(!client.busy());
  QVERIFY(client.imapList(endpoint));client.cancel();QCOMPARE(failed.size(),2);QVERIFY(!client.busy());
 }
 void trustedFixtureTlsAndStartTlsActuallyTransmitEncrypted(){
  const auto original=QSslConfiguration::defaultConfiguration();
  struct Restore {QSslConfiguration configuration;~Restore(){QSslConfiguration::setDefaultConfiguration(configuration);}} restore{original};
  QFile file(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath()+"/fixtures/mail-fixture.crt");QVERIFY(file.open(QIODevice::ReadOnly));
  auto trusted=original;auto certificates=trusted.caCertificates();certificates.append(QSslCertificate(file.readAll()));trusted.setCaCertificates(certificates);QSslConfiguration::setDefaultConfiguration(trusted);
  for(auto auth:{MailTransportEndpoint::Auth::Password,MailTransportEndpoint::Auth::OAuth2})for(const auto mode:{MailFakeServer::TlsSmtp,MailFakeServer::TlsImap,MailFakeServer::StartTlsSmtp,MailFakeServer::StartTlsImap}){
   MailFakeServer server(mode);MailTransport client;QSignalSpy finished(&client,&MailTransport::finished),failed(&client,&MailTransport::failed),fetched(&client,&MailTransport::messageFetched);
   const bool imap=mode==MailFakeServer::TlsImap||mode==MailFakeServer::StartTlsImap;
   const bool start=mode==MailFakeServer::StartTlsSmtp||mode==MailFakeServer::StartTlsImap;
   auto endpoint=config(server,start?MailTransportEndpoint::Security::StartTls:MailTransportEndpoint::Security::ImplicitTls);
   endpoint.auth=auth;
   if(imap)QVERIFY(client.imapFetch(endpoint,99));else QVERIFY(client.smtpSend(endpoint,"from@example.test",{"to@example.test"},"Subject: tls\r\n\r\nbody\r\n"));
   QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty()||!failed.isEmpty(),5000);
   QCOMPARE(failed.size(),0);QCOMPARE(finished.size(),1);QVERIFY(server.encryptedSession);QVERIFY(!server.authBeforeTls);
   if(auth==MailTransportEndpoint::Auth::OAuth2)QVERIFY(server.oauthCompleted);
   if(imap){QCOMPARE(fetched.size(),1);QCOMPARE(fetched.first().first().toByteArray(),server.message);}
   else QVERIFY(server.data.contains("Subject: tls"));
  }
 }
 void fetchedCallbackReentryKeepsOriginalUid(){
  MailFakeServer server(MailFakeServer::Imap);MailTransport client;QSignalSpy finished(&client,&MailTransport::finished),failed(&client,&MailTransport::failed);
  bool restarted=false;
  connect(&client,&MailTransport::messageFetched,&client,[&](QByteArray){if(!restarted){restarted=true;QVERIFY(client.imapFetch(config(server),77));}});
  QVERIFY(client.imapFetch(config(server),99));QTRY_COMPARE(finished.size(),1);
  QCOMPARE(finished.first().first().toJsonObject()["uid"].toString(),QString("99"));
  QTRY_COMPARE(failed.size(),1); // fake only returns UID99, never falsely UID77.
 }
 void invalidAndUnsafeInputsNeverConnect(){
  MailFakeServer server(MailFakeServer::Smtp);MailTransport client;QSignalSpy failed(&client,&MailTransport::failed);auto c=config(server);
  c.host="example.test";QVERIFY(!client.imapList(c));c=config(server);QVERIFY(!client.imapList(c,"INBOX\r\nA1 LOGOUT"));c.username="name\r\ncommand";QVERIFY(!client.imapList(c));
  QCOMPARE(failed.size(),3);QVERIFY(server.received.isEmpty());
 }
};
QTEST_GUILESS_MAIN(MailTransportTest)
#include "test_mailtransport.moc"
