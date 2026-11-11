#include "mailmime.h"
#include <QRegularExpression>
#include <QUuid>
#include <QTextDocument>
#include <QMap>
#include <QDateTime>
#include <QLocale>
#include <QUrl>
#include <QSet>
#include <QStringConverter>
#include <functional>
namespace MailMime {
namespace {
bool headerSafe(const QString &s){for(auto c:s)if(c.unicode()<32||c.unicode()==127)return false;return true;}
bool validAddress(const Address &a){return a.address.size()<=254&&a.displayName.size()<=256&&headerSafe(a.displayName)&&headerSafe(a.address)&&QRegularExpression("^[A-Za-z0-9.!#$%&'*+/=?^_`{|}~-]+@[A-Za-z0-9](?:[A-Za-z0-9.-]*[A-Za-z0-9])?$").match(a.address).hasMatch();}
bool validId(const QString &id){
 static const QRegularExpression re(QStringLiteral("^<[A-Za-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\\.[A-Za-z0-9!#$%&'*+/=?^_`{|}~-]+)*@[A-Za-z0-9-]+(?:\\.[A-Za-z0-9-]+)*>$"));
 return id.size()<=254&&headerSafe(id)&&re.match(id).hasMatch();
}
bool validReferences(const QStringList &ids){
 if(ids.size()>100)return false;qsizetype length=0;
 for(const auto &id:ids){if(!validId(id))return false;length+=id.size()+1;}
 return length<=8192;
}
bool readReferences(const QByteArray &raw,QStringList &ids){
 if(raw.size()>8192)return false;
 const auto value=QString::fromLatin1(raw).trimmed();
 if(value.isEmpty())return false;
 ids=value.split(QRegularExpression("[ \t]+"),Qt::SkipEmptyParts);
 return validReferences(ids);
}
QByteArray word(const QString &s){
 QByteArray result,chunk;auto flush=[&]{if(!result.isEmpty())result+="\r\n ";result+="=?UTF-8?B?"+chunk.toBase64()+"?=";chunk.clear();};
 for(qsizetype i=0;i<s.size();++i){QString unit(s[i]);if(s[i].isHighSurrogate()&&i+1<s.size()&&s[i+1].isLowSurrogate())unit+=s[++i];auto bytes=unit.toUtf8();if(chunk.size()+bytes.size()>42)flush();chunk+=bytes;}
 if(!chunk.isEmpty()||result.isEmpty())flush();return result;
}
QByteArray addr(const Address &a){return a.displayName.isEmpty()?a.address.toUtf8():word(a.displayName)+" <"+a.address.toUtf8()+">";}
QByteArray base64Lines(const QByteArray &data){const auto b=data.toBase64();QByteArray out;for(qsizetype i=0;i<b.size();i+=76)out+=b.mid(i,76)+"\r\n";return out;}
bool decode64(QByteArray b,QByteArray &out){b.replace("\r","");b.replace("\n","");b.replace(" ","");b.replace("\t","");auto result=QByteArray::fromBase64Encoding(b,QByteArray::AbortOnBase64DecodingErrors);if(!result)return false;out=result.decoded;return true;}
bool quotedPrintable(const QByteArray &s,QByteArray &out){
 for(qsizetype i=0;i<s.size();++i){if(s[i]!='='){out+=s[i];continue;}if(s.mid(i,3)=="=\r\n"){i+=2;continue;}if(s.mid(i,2)=="=\n"){++i;continue;}if(i+2>=s.size())return false;
 auto hex=[](char c){return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;};int a=hex(s[i+1]),b=hex(s[i+2]);if(a<0||b<0)return false;out+=char(a*16+b);i+=2;}return true;
}
QString charsetText(const QByteArray &bytes,const QString &charset,bool &ok){QStringDecoder decoder(charset.isEmpty()?"UTF-8":charset.toLatin1().constData());if(!decoder.isValid()){ok=false;return {};}auto s=decoder.decode(bytes);if(decoder.hasError())ok=false;return s;}
QString decodeWords(QString text,bool &ok){
 QRegularExpression re("=\\?([^?]+)\\?([bBqQ])\\?([^?]*)\\?=");QString result;qsizetype end=0;bool previous=false;
 auto it=re.globalMatch(text);while(it.hasNext()){auto m=it.next();auto gap=text.mid(end,m.capturedStart()-end);if(!(previous&&gap.trimmed().isEmpty()))result+=gap;QByteArray data;
 if(m.captured(2).toLower()=="b"){if(!decode64(m.captured(3).toLatin1(),data)){ok=false;return {};}}else{auto raw=m.captured(3).toLatin1();raw.replace('_',' ');if(!quotedPrintable(raw,data)){ok=false;return {};}}
 result+=charsetText(data,m.captured(1),ok);end=m.capturedEnd();previous=true;}result+=text.mid(end);return result;
}
QString param(const QString &value,const QString &name){auto m=QRegularExpression("(?:^|;)\\s*"+QRegularExpression::escape(name)+"\\s*=\\s*(?:\"([^\"]*)\"|([^;\\s]+))",QRegularExpression::CaseInsensitiveOption).match(value);return m.hasMatch()?(m.captured(1).isNull()?m.captured(2):m.captured(1)):QString();}
Address parseAddress(QString s,bool &ok){Address a;const int left=s.lastIndexOf('<');if(left>=0&&s.endsWith('>')){auto name=s.left(left).trimmed();if(name.contains('<')||name.contains('>')||(name.contains(',')&&!(name.startsWith('"')&&name.endsWith('"'))))ok=false;a.displayName=decodeWords(name,ok);if(a.displayName.startsWith('"')&&a.displayName.endsWith('"'))a.displayName=a.displayName.mid(1,a.displayName.size()-2);a.address=s.mid(left+1,s.size()-left-2).trimmed();}else a.address=s.trimmed();if(!validAddress(a))ok=false;return a;}
}
Result build(const Message &m,const Limits &limits){
 Result r;auto fail=[&](QString e){r.error=e;return r;};
 if(!validAddress(m.from)||m.to.isEmpty()||!headerSafe(m.subject))return fail("Invalid address or unsafe header");
 if((!m.replyTo.address.isEmpty()||!m.replyTo.displayName.isEmpty())&&!validAddress(m.replyTo))return fail("Invalid Reply-To address");
 if((!m.inReplyTo.isEmpty()&&!validId(m.inReplyTo))||!validReferences(m.references))return fail("Invalid reply threading metadata");
 if(m.to.size()>100||m.attachments.size()+2>limits.maxParts)return fail("Too many MIME parts or recipients");
 QByteArray recipients;for(const auto &a:m.to){if(!validAddress(a))return fail("Invalid recipient");if(!recipients.isEmpty())recipients+=",\r\n ";recipients+=addr(a);}
 qsizetype total=m.text.toUtf8().size()+m.html.toUtf8().size();
 for(const auto &a:m.attachments){if(!headerSafe(a.filename)||a.filename.isEmpty()||a.filename.toUtf8().size()>255||a.filename.contains('/')||a.filename.contains('\\')||!QRegularExpression("^[A-Za-z0-9!#$&^_.+-]+/[A-Za-z0-9!#$&^_.+-]+$").match(a.contentType).hasMatch())return fail("Invalid attachment metadata");total+=a.data.size();if(total>limits.maxDecodedBytes)return fail("Decoded message too large");}
 if(total>limits.maxDecodedBytes)return fail("Decoded message too large");
 const auto generatedId="<"+QUuid::createUuid().toByteArray(QUuid::Id128)+"@lanlink.local>";
 QByteArray threading;
 if(!m.replyTo.address.isEmpty())threading+="Reply-To: "+addr(m.replyTo)+"\r\n";
 if(!m.inReplyTo.isEmpty())threading+="In-Reply-To: "+m.inReplyTo.toLatin1()+"\r\n";
 if(!m.references.isEmpty()){threading+="References:";for(const auto &id:m.references)threading+="\r\n "+id.toLatin1();threading+="\r\n";}
 const auto boundary="=_LanLink_"+QUuid::createUuid().toByteArray(QUuid::Id128);
 const auto generatedDate=QLocale::c().toString(QDateTime::currentDateTimeUtc(),"ddd, dd MMM yyyy HH:mm:ss '+0000'");
 QByteArray wire="Date: "+generatedDate.toLatin1()+"\r\nMessage-ID: "+generatedId+"\r\n"+threading+"From: "+addr(m.from)+"\r\nTo: "+recipients+"\r\nSubject: "+word(m.subject)+"\r\nMIME-Version: 1.0\r\nContent-Type: multipart/mixed; boundary=\""+boundary+"\"\r\n\r\n";
 auto part=[&](QByteArray type,QByteArray data,QByteArray extra={}){wire+="--"+boundary+"\r\nContent-Type: "+type+"\r\nContent-Transfer-Encoding: base64\r\n"+extra+"\r\n"+base64Lines(data);};
 part("text/plain; charset=UTF-8",m.text.isEmpty()&&!m.html.isEmpty()?safeDisplayText(m).toUtf8():m.text.toUtf8());
 // Outbound intentionally uses a safe plain body, not active HTML.
 for(const auto &a:m.attachments)part(a.contentType.toLatin1(),a.data,"Content-Disposition: attachment; filename=\"attachment.bin\";\r\n filename*=UTF-8''"+QUrl::toPercentEncoding(a.filename)+"\r\n");
 wire+="--"+boundary+"--\r\n";if(wire.size()>limits.maxWireBytes)return fail("Wire message too large");r.ok=true;r.wire=wire;r.message=m;r.message.messageId=QString::fromLatin1(generatedId);r.message.date=generatedDate;return r;
}
Result parse(const QByteArray &wire,const Limits &limits){
 Result r;if(wire.size()>limits.maxWireBytes){r.error="Wire message too large";return r;}int parts=0;qsizetype decoded=0;bool ok=true;QString failure;
 std::function<bool(QByteArray,int)> visit=[&](QByteArray raw,int depth){
 if(depth>limits.maxDepth||++parts>limits.maxParts){failure="MIME nesting/part limit exceeded";return false;}
 const auto cr=raw.indexOf("\r\n\r\n"),lf=raw.indexOf("\n\n");const bool crlf=cr>=0&&(lf<0||cr<lf);const auto split=crlf?cr:lf;if(split<0||split>65536){failure="Invalid or oversized MIME headers";return false;}
 QMap<QByteArray,QByteArray> h;QByteArray key;
 auto headers=raw.left(split);headers.replace("\r\n","\n");
 for(auto line:headers.split('\n')){if(line.startsWith(' ')||line.startsWith('\t')){if(key.isEmpty())return false;h[key]+=" "+line.trimmed();continue;}
 const auto colon=line.indexOf(':');if(colon<1)return false;key=line.left(colon).toLower();for(char c:key)if(c<33||c>126)return false;if(h.contains(key)){
 static const QSet<QByteArray> single={"from","to","subject","content-type","content-transfer-encoding","content-disposition","mime-version","date","message-id","reply-to","in-reply-to","references","return-path"};
 if(single.contains(key))return false;
 // Received and other non-consumed trace/extension fields legitimately repeat.
 continue;
 }h[key]=line.mid(colon+1).trimmed();}
 for(auto v:h)if(v.contains('\0')||v.contains('\r'))return false;
 if(depth==0){r.message.subject=decodeWords(QString::fromUtf8(h.value("subject")),ok);if(h.contains("from"))r.message.from=parseAddress(QString::fromUtf8(h["from"]),ok);
 if(h.contains("reply-to"))r.message.replyTo=parseAddress(QString::fromUtf8(h["reply-to"]),ok);
 if(h.contains("message-id")){r.message.messageId=QString::fromLatin1(h["message-id"]);if(!validId(r.message.messageId))ok=false;}
 if(h.contains("in-reply-to")){r.message.inReplyTo=QString::fromLatin1(h["in-reply-to"]);if(!validId(r.message.inReplyTo))ok=false;}
 if(h.contains("references")&&!readReferences(h["references"],r.message.references))ok=false;
 r.message.date=QString::fromLatin1(h.value("date"));if(!headerSafe(r.message.date))ok=false;
 // Quoted/encoded display names may contain commas: split only outside quotes/brackets.
 QString addresses=QString::fromUtf8(h.value("to"));QString token;bool quoted=false;int angle=0;
 for(QChar c:addresses){if(c=='"')quoted=!quoted;if(c=='<')++angle;if(c=='>')--angle;if(c==','&&!quoted&&!angle){r.message.to.push_back(parseAddress(token,ok));token.clear();}else token+=c;}
 if(!token.trimmed().isEmpty())r.message.to.push_back(parseAddress(token,ok));if(quoted||angle)ok=false;}
 if(!ok||!headerSafe(r.message.subject))return false;
 const QString type=QString::fromLatin1(h.value("content-type","text/plain"));auto body=raw.mid(split+(crlf?4:2));
 if(type.startsWith("multipart/",Qt::CaseInsensitive)){
 const auto boundary=param(type,"boundary").toLatin1();if(boundary.isEmpty()||boundary.size()>200)return false;
 QByteArray child;bool started=false,closed=false;
 for(auto line:body.split('\n')){auto marker=line;if(marker.endsWith('\r'))marker.chop(1);
 if(marker=="--"+boundary||marker=="--"+boundary+"--"){
 if(started&&!child.isEmpty()){const auto headerEnd=child.indexOf("\r\n\r\n");const auto lfEnd=child.indexOf("\n\n");if(child.endsWith("\r\n")&&headerEnd>=0&&child.size()>headerEnd+4)child.chop(2);else if(child.endsWith('\n')&&lfEnd>=0&&child.size()>lfEnd+2)child.chop(1);if(!visit(child,depth+1))return false;child.clear();}
 if(marker=="--"+boundary+"--"){closed=true;break;}started=true;
 }else if(started)child+=line+'\n';}return started&&closed;
 }
 QByteArray bytes;const auto enc=h.value("content-transfer-encoding").toLower();
 if(enc=="base64"){if(!decode64(body,bytes))return false;}else if(enc=="quoted-printable"){if(!quotedPrintable(body,bytes))return false;}else if(enc.isEmpty()||enc=="7bit"||enc=="8bit"||enc=="binary")bytes=body;else return false;
 decoded+=bytes.size();if(decoded>limits.maxDecodedBytes){failure="Decoded message too large";return false;}
 const auto disposition=QString::fromLatin1(h.value("content-disposition"));QString filename=param(disposition,"filename");
 const auto extended=param(disposition,"filename*");
 if(!extended.isEmpty()) {
  const auto first=extended.indexOf('\'');const auto second=extended.indexOf('\'',first+1);
  if(first<=0||second<0)return false;
  const auto encoded=extended.mid(second+1).toLatin1();
  for(int i=0;i<encoded.size();++i)if(encoded[i]=='%'){
   if(i+2>=encoded.size()||!QRegularExpression("^[0-9A-Fa-f]{2}$").match(QString::fromLatin1(encoded.mid(i+1,2))).hasMatch())return false;i+=2;
  }
  filename=charsetText(QByteArray::fromPercentEncoding(encoded),extended.left(first),ok);
 }else{if(filename.isEmpty())filename=param(type,"name");filename=decodeWords(filename,ok);}
 const auto mime=type.section(';',0,0).trimmed().toLower();
 if(disposition.startsWith("attachment",Qt::CaseInsensitive)||!filename.isEmpty()||(mime!="text/plain"&&mime!="text/html")){
 // Incoming filenames are display metadata only; never paths to write blindly.
 filename.replace('\\','/');filename=filename.section('/',-1);if(filename.isEmpty()||filename=="."||filename=="..")filename="attachment.bin";if(!headerSafe(filename))return false;
 r.message.attachments.push_back({filename,mime,bytes});
 }else{const auto text=charsetText(bytes,param(type,"charset"),ok);if(mime=="text/plain")r.message.text+=text;else r.message.html+=text;}
 return ok;
 };
 if(!visit(wire,0)){r.error=failure.isEmpty()?"Malformed or unsupported MIME message":failure;r.message={};return r;}r.ok=true;return r;
}
Result makeReplyDraft(const Message &original,const Address &sender){
 Result r;
 const auto recipient=original.replyTo.address.isEmpty()&&original.replyTo.displayName.isEmpty()?original.from:original.replyTo;
 if(!validAddress(sender)||!validAddress(recipient)||!headerSafe(original.subject)) {r.error="Invalid reply address or subject";return r;}
 if((!original.messageId.isEmpty()&&!validId(original.messageId))||(!original.inReplyTo.isEmpty()&&!validId(original.inReplyTo))||!validReferences(original.references)){r.error="Invalid reply threading metadata";return r;}
 auto &draft=r.message;draft.from=sender;draft.to={recipient};
 draft.subject=original.subject.trimmed();
 if(!draft.subject.startsWith("Re:",Qt::CaseInsensitive))draft.subject="Re: "+draft.subject;
 auto body=safeDisplayText(original);body.replace("\r\n","\n");body.replace('\r','\n');
 draft.text="\n\n> "+body.split('\n').join("\n> ");
 if(!original.messageId.isEmpty()){
  draft.inReplyTo=original.messageId;draft.references=original.references;
  if(draft.references.isEmpty()&&!original.inReplyTo.isEmpty())draft.references.append(original.inReplyTo);
  if(!draft.references.contains(original.messageId))draft.references.append(original.messageId);
  while(!validReferences(draft.references))draft.references.removeFirst();
 }
 r.ok=true;return r;
}
QString safeDisplayText(const Message &m){
 if(!m.text.isEmpty())return m.text;
 class NoResources final:public QTextDocument {
  QVariant loadResource(int,const QUrl &)override{return {};}
 } doc;
 doc.setHtml(m.html);return doc.toPlainText();
}
}
