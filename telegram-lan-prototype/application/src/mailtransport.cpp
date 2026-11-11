#include "mailtransport.h"
#include <QHostAddress>
#include <QJsonArray>
#include <QPointer>
#include <QRegularExpression>
namespace {
bool validId(const QString &value){
 static const QRegularExpression digits("^[0-9]{1,10}$");
 bool ok=false;const auto id=value.toULongLong(&ok);
 return digits.match(value).hasMatch()&&ok&&id>0&&id<=0xffffffffULL;
}
bool clean(const QString &s){for(auto c:s)if(c.unicode()<32||c.unicode()==127)return false;return true;}
QByteArray xoauth2(const MailTransportEndpoint &endpoint){
 return ("user="+endpoint.username.toUtf8()+char(1)+"auth=Bearer "+endpoint.password.toUtf8()+char(1)+char(1)).toBase64();
}
QByteArray quoted(QString s){return '"'+s.replace("\\","\\\\").replace("\"","\\\"").toUtf8()+'"';}
bool address(const QString &s){return !s.isEmpty()&&s.size()<=254&&clean(s)&&s.contains('@')&&!s.contains(' ')&&!s.contains('<')&&!s.contains('>')&&s.toLatin1()==s.toUtf8();}
QString mailboxWire(const QString &name) {
 // IMAP4rev1 modified UTF-7 (RFC3501): UTF-16BE runs, ',' instead of '/'.
 QString result;QByteArray unicode;
 auto flush=[&]{if(unicode.isEmpty())return;auto encoded=unicode.toBase64();encoded.replace('/',',');encoded.replace("=","");result+='&'+QString::fromLatin1(encoded)+'-';unicode.clear();};
 for(const auto c:name){const auto u=c.unicode();if(u>=0x20&&u<=0x7e){flush();result+=c=='&'?QString("&-"):QString(c);}else{unicode+=char(u>>8);unicode+=char(u&255);}}
 flush();return result;
}
QByteArray smtpData(QByteArray mime) {
 mime.replace("\r\n","\n");mime.replace('\r','\n');
 auto lines=mime.split('\n');if(!lines.isEmpty()&&lines.last().isEmpty())lines.removeLast();
 QByteArray result;for(const auto &line:lines){if(line.startsWith('.'))result+='.';result+=line;result+="\r\n";}
 result+=".\r\n";return result;
}
}
MailTransport::MailTransport(QObject *parent):QObject(parent){timer_.setSingleShot(true);connect(&timer_,&QTimer::timeout,this,[this]{fail("邮件服务器操作超时");});}
void MailTransport::setTimeoutMs(int ms){timeoutMs_=qBound(100,ms,120000);}
bool MailTransport::smtpSend(MailTransportEndpoint endpoint,QString from,QStringList recipients,QByteArray mime){
 if(active_)return false;
 if(!address(from)||recipients.isEmpty()||recipients.size()>100||mime.size()>MaxMessageBytes||mime.isEmpty()) {emit failed("无效的邮件地址、收件人或邮件大小");return false;}
 for(const auto &to:recipients)if(!address(to)){emit failed("无效的收件人地址");return false;}
 from_=std::move(from);recipients_=std::move(recipients);mime_=std::move(mime);return start(std::move(endpoint),Kind::Smtp);
}
bool MailTransport::imapList(MailTransportEndpoint endpoint,QString mailbox){
 if(active_)return false;if(mailbox.isEmpty()||!clean(mailbox)){emit failed("无效的邮箱目录");return false;}
 expectedUidValidity_.clear();mailbox_=std::move(mailbox);return start(std::move(endpoint),Kind::List);
}
bool MailTransport::imapFetch(MailTransportEndpoint endpoint,quint64 uid,QString mailbox,QString expectedUidValidity){
 if(active_)return false;if(!uid||uid>0xffffffffULL||(!expectedUidValidity.isEmpty()&&!validId(expectedUidValidity))||mailbox.isEmpty()||!clean(mailbox)){emit failed("无效的邮件 UID 或邮箱目录");return false;}
 expectedUidValidity_=expectedUidValidity.isEmpty()?QString():QString::number(expectedUidValidity.toULongLong());uid_=uid;mailbox_=std::move(mailbox);return start(std::move(endpoint),Kind::Fetch);
}
bool MailTransport::imapHeaders(MailTransportEndpoint endpoint,quint64 uid,QString mailbox,QString expectedUidValidity){
 if(active_)return false;if(!uid||uid>0xffffffffULL||(!expectedUidValidity.isEmpty()&&!validId(expectedUidValidity))||mailbox.isEmpty()||!clean(mailbox)){emit failed("无效的邮件 UID 或邮箱目录");return false;}
 expectedUidValidity_=expectedUidValidity.isEmpty()?QString():QString::number(expectedUidValidity.toULongLong());uid_=uid;mailbox_=std::move(mailbox);return start(std::move(endpoint),Kind::Headers);
}
bool MailTransport::start(MailTransportEndpoint endpoint,Kind kind){
 if(endpoint.host.isEmpty()||!endpoint.port||!clean(endpoint.host)||endpoint.username.isEmpty()||!clean(endpoint.username)||!clean(endpoint.password)||(endpoint.auth==MailTransportEndpoint::Auth::OAuth2&&endpoint.password.isEmpty())) {emit failed("无效的邮件服务器配置");return false;}
 if(endpoint.security==MailTransportEndpoint::Security::PlaintextLoopbackTestOnly) {
  QHostAddress host;if(!host.setAddress(endpoint.host)||!host.isLoopback()){emit failed("明文测试策略仅允许回环地址");return false;}
 }
 endpoint_=std::move(endpoint);kind_=kind;step_=Step::Greeting;active_=true;
 uidValidity_.clear();buffer_.clear();smtpReply_.clear();literal_.clear();uids_.clear();gotLiteral_=false;literalRemaining_=-1;
 tagNumber_=0;recipient_=0;smtpCode_=0;tlsUpgrading_=false;authPayloadSent_=false;
 socket_=new QSslSocket(this);socket_->setPeerVerifyMode(QSslSocket::VerifyPeer);
 socket_->setProtocol(QSsl::TlsV1_2OrLater);
 connect(socket_,&QSslSocket::connected,this,&MailTransport::connected);
 connect(socket_,&QSslSocket::encrypted,this,&MailTransport::encrypted);
 connect(socket_,&QSslSocket::readyRead,this,&MailTransport::readyRead);
 connect(socket_,&QSslSocket::sslErrors,this,[this](const QList<QSslError>&){fail("邮件服务器 TLS 证书验证失败");});
 connect(socket_,&QSslSocket::errorOccurred,this,[this](QAbstractSocket::SocketError){if(active_)fail("邮件服务器连接失败或中断");});
 connect(socket_,&QSslSocket::disconnected,this,[this]{if(active_)fail("邮件服务器在操作完成前断开连接");});
 timer_.start(timeoutMs_);
 if(endpoint_.security==MailTransportEndpoint::Security::ImplicitTls)socket_->connectToHostEncrypted(endpoint_.host,endpoint_.port);
 else socket_->connectToHost(endpoint_.host,endpoint_.port);
 return true;
}
void MailTransport::connected(){} // greeting drives both protocols; no auth here.
bool MailTransport::secure()const{return socket_&&(socket_->isEncrypted()||endpoint_.security==MailTransportEndpoint::Security::PlaintextLoopbackTestOnly);}
void MailTransport::encrypted(){
 if(!active_)return;
 if(tlsUpgrading_){tlsUpgrading_=false;buffer_.clear();if(kind_==Kind::Smtp){step_=Step::Ehlo;write("EHLO lanlink.local\r\n");}else authenticate();}
}
void MailTransport::write(QByteArray bytes){if(socket_&&active_)socket_->write(bytes);}
void MailTransport::authenticate(){
 if(!secure()){fail("拒绝在未验证 TLS 的连接上发送凭据");return;}
 step_=Step::Authenticating;
 if(endpoint_.auth==MailTransportEndpoint::Auth::OAuth2){
  if(kind_==Kind::Smtp){write("AUTH XOAUTH2 "+xoauth2(endpoint_)+"\r\n");authPayloadSent_=true;endpoint_.password.clear();}
  else imapCommand("AUTHENTICATE XOAUTH2");
  return;
 }
 if(kind_==Kind::Smtp){QByteArray auth(1,'\0');auth+=endpoint_.username.toUtf8();auth+='\0';auth+=endpoint_.password.toUtf8();write("AUTH PLAIN "+auth.toBase64()+"\r\n");}
 else imapCommand("LOGIN "+quoted(endpoint_.username)+' '+quoted(endpoint_.password));
}
void MailTransport::readyRead(){
 if(!socket_||!active_)return;buffer_+=socket_->readAll();
 if(buffer_.size()>MaxMessageBytes+65536){fail("邮件服务器响应超出限制");return;}
 QPointer<MailTransport> guard(this);
 while(active_) {
  if(kind_!=Kind::Smtp&&literalRemaining_>=0){
   auto count=qMin<qint64>(literalRemaining_,buffer_.size());literal_+=buffer_.first(count);buffer_.remove(0,count);literalRemaining_-=count;
   if(literalRemaining_>0)return;literalRemaining_=-1;gotLiteral_=true;
  }
  auto newline=buffer_.indexOf("\r\n");if(newline<0){if(buffer_.size()>65536)fail("无效的邮件服务器响应行");return;}
  auto line=buffer_.left(newline);buffer_.remove(0,newline+2);
  if(kind_==Kind::Smtp)smtpLine(line);else imapLine(line);
  if(!guard)return;
 }
}
void MailTransport::smtpLine(QByteArray line){
 if(line.size()<4||line[0]<'2'||line[0]>'5'||line[1]<'0'||line[1]>'9'||line[2]<'0'||line[2]>'9'||(line[3]!=' '&&line[3]!='-')){fail("无效的 SMTP 响应");return;}
 int code=line.first(3).toInt();if(smtpCode_&&smtpCode_!=code){fail("SMTP 多行响应不一致");return;}smtpCode_=code;smtpReply_+=line+'\n';
 if(smtpReply_.size()>65536){fail("SMTP 响应过长");return;}if(line[3]=='-')return;
 auto reply=smtpReply_;smtpReply_.clear();smtpCode_=0;
 auto expect=[&](int wanted){if(code!=wanted){fail("SMTP 操作被服务器拒绝");return false;}return true;};
 switch(step_){
 case Step::Greeting:if(expect(220)){if(endpoint_.security==MailTransportEndpoint::Security::ImplicitTls&&!secure()){fail("TLS 尚未建立");return;}step_=Step::Ehlo;write("EHLO lanlink.local\r\n");}break;
 case Step::Ehlo:
  if(!expect(250))break;
  if(endpoint_.security==MailTransportEndpoint::Security::StartTls&&!socket_->isEncrypted()){
   if(!reply.toUpper().contains("STARTTLS")){fail("SMTP 服务器未提供 STARTTLS");return;}step_=Step::StartTls;write("STARTTLS\r\n");
  }else authenticate();break;
 case Step::StartTls:if(expect(220)){if(!buffer_.isEmpty()){fail("STARTTLS 边界存在未加密数据");return;}tlsUpgrading_=true;socket_->startClientEncryption();}break;
 case Step::AuthRejected:fail("SMTP 身份验证被服务器拒绝");break;
 case Step::Authenticating:
  if(endpoint_.auth==MailTransportEndpoint::Auth::OAuth2&&code==334){step_=Step::AuthRejected;write("\r\n");break;}
  if(expect(235)){endpoint_.password.clear();step_=Step::MailFrom;write("MAIL FROM:<"+from_.toUtf8()+">\r\n");}break;
 case Step::MailFrom:if(expect(250)){step_=Step::Recipient;write("RCPT TO:<"+recipients_[recipient_].toUtf8()+">\r\n");}break;
 case Step::Recipient:
  if(code!=250&&code!=251){fail("SMTP 收件人被拒绝；邮件未提交");return;}
  if(++recipient_<recipients_.size())write("RCPT TO:<"+recipients_[recipient_].toUtf8()+">\r\n");
  else {step_=Step::Data;write("DATA\r\n");}break;
 case Step::Data:if(expect(354)){step_=Step::Body;write(smtpData(mime_));}break;
 case Step::Body:if(code>=400)step_=Step::Data;if(expect(250)){write("QUIT\r\n");succeed({{"protocol","smtp"},{"accepted",true}});}break;
 default:fail("无效的 SMTP 状态");break;
 }
}
void MailTransport::imapCommand(QByteArray command){tag_="A"+QByteArray::number(++tagNumber_);write(tag_+' '+command+"\r\n");}
void MailTransport::imapLine(QByteArray line){
 if(step_==Step::Greeting){
  if(!line.toUpper().startsWith("* OK")){fail("IMAP 服务器未提供有效欢迎响应");return;}
  if(endpoint_.security==MailTransportEndpoint::Security::StartTls){step_=Step::StartTls;imapCommand("STARTTLS");}else authenticate();return;
 }
 if(line.toUpper().startsWith("* BYE")){fail("IMAP 服务器关闭会话");return;}
 if(endpoint_.auth==MailTransportEndpoint::Auth::OAuth2&&(step_==Step::Authenticating||step_==Step::AuthRejected)&&line.startsWith('+')){
  if(step_==Step::AuthRejected){fail("IMAP 身份验证被服务器拒绝");return;}
  if(!authPayloadSent_&&line.mid(1).trimmed().isEmpty()){write(xoauth2(endpoint_)+"\r\n");authPayloadSent_=true;endpoint_.password.clear();}
  else {endpoint_.password.clear();step_=Step::AuthRejected;write("\r\n");}
  return;
 }
 if(step_==Step::Select&&(line.toUpper().startsWith("* OK ")||line.startsWith(tag_+" "))) {
  const auto match=QRegularExpression("\\[UIDVALIDITY +([^\\]]*)\\]",QRegularExpression::CaseInsensitiveOption).match(QString::fromLatin1(line));
  if(match.hasMatch()) {
   const auto value=match.captured(1);
   if(!validId(value)){fail("无效的 IMAP UIDVALIDITY");return;}
   const auto canonical=QString::number(value.toULongLong());
   if(!uidValidity_.isEmpty()&&uidValidity_!=canonical){fail("IMAP UIDVALIDITY 不一致");return;}
   uidValidity_=canonical;
  }
 }
 if(step_==Step::Fetch&&line.endsWith('}')) {
  const auto brace=line.lastIndexOf('{');bool ok=false;auto size=line.mid(brace+1,line.size()-brace-2).toLongLong(&ok);
  if(brace<0||!ok||size<0||size>(kind_==Kind::Headers?65536:MaxMessageBytes)||gotLiteral_||!line.toUpper().contains(kind_==Kind::Headers?"BODY[HEADER.FIELDS (":"BODY[]")){fail("无效或过大的 IMAP 邮件字面量");return;}
  const auto uidMatch=QRegularExpression("\\bUID +([0-9]+)\\b",QRegularExpression::CaseInsensitiveOption).match(QString::fromLatin1(line));
  if(!uidMatch.hasMatch()||uidMatch.captured(1).toULongLong()!=uid_){fail("IMAP 正文 UID 不匹配或缺失");return;}
  literalRemaining_=size;return;
 }
 if(step_==Step::Search&&line.startsWith("* SEARCH")) {
  for(const auto &value:line.mid(8).simplified().split(' ')){if(value.isEmpty())continue;bool ok=false;auto uid=value.toULongLong(&ok);if(!ok||!validId(QString::fromLatin1(value))||uids_.size()>=100000){fail("无效或过多的 IMAP UID");return;}uids_.append(uid);}return;
 }
 if(!line.startsWith(tag_+' '))return;
 if(step_==Step::AuthRejected||(step_==Step::Authenticating&&endpoint_.auth==MailTransportEndpoint::Auth::OAuth2&&!authPayloadSent_)){fail("IMAP 身份验证被服务器拒绝");return;}
 if(line.mid(tag_.size()+1).toUpper().split(' ').first()!="OK"){fail("IMAP 操作被服务器拒绝");return;}
 switch(step_) {
 case Step::StartTls:if(!buffer_.isEmpty()){fail("STARTTLS 边界存在未加密数据");return;}tlsUpgrading_=true;socket_->startClientEncryption();break;
 case Step::Authenticating:endpoint_.password.clear();step_=Step::Select;imapCommand("EXAMINE "+quoted(mailboxWire(mailbox_)));break;
 case Step::Select:
  if(uidValidity_.isEmpty()){fail("IMAP 响应缺少有效 UIDVALIDITY");return;}
  if(kind_!=Kind::List&&!expectedUidValidity_.isEmpty()&&expectedUidValidity_!=uidValidity_){fail("邮箱 UIDVALIDITY 已改变，请先刷新邮件列表");return;}
  if(kind_==Kind::List){step_=Step::Search;imapCommand("UID SEARCH ALL");}else{step_=Step::Fetch;imapCommand("UID FETCH "+QByteArray::number(uid_)+(kind_==Kind::Headers?" (UID BODY.PEEK[HEADER.FIELDS (FROM TO SUBJECT DATE MESSAGE-ID REPLY-TO REFERENCES IN-REPLY-TO)])":" (UID BODY.PEEK[])"));}break;
 case Step::Search:{QJsonArray ids;for(auto uid:uids_)ids.append(QString::number(uid));succeed({{"protocol","imap"},{"uids",ids},{"mailbox",mailbox_},{"uidValidity",uidValidity_}});break;}
 case Step::Fetch:{if(!gotLiteral_){fail("IMAP 响应缺少邮件正文");return;}auto data=literal_;const QJsonObject result{{"protocol","imap"},{"uid",QString::number(uid_)},{"bytes",data.size()},{"headersOnly",kind_==Kind::Headers},{"mailbox",mailbox_},{"uidValidity",uidValidity_}};QPointer<MailTransport> guard(this);reset();emit messageFetched(data);if(guard)emit finished(result);break;}
 default:fail("无效的 IMAP 状态");break;
 }
}
void MailTransport::reset(){active_=false;timer_.stop();endpoint_.password.clear();mime_.clear();buffer_.clear();literal_.clear();if(socket_){auto *socket=socket_;socket_=nullptr;socket->disconnect(this);socket->abort();socket->deleteLater();}}
void MailTransport::fail(QString error){
 if(!active_)return;
 if(kind_==Kind::Smtp&&step_==Step::Body)
  error=QStringLiteral("邮件发送结果未知：正文已提交但未收到服务器最终确认，请勿直接重发");
 reset();emit failed(error);
}
void MailTransport::succeed(QJsonObject result){reset();emit finished(result);}
void MailTransport::cancel(){if(active_)fail("邮件操作已取消");}
