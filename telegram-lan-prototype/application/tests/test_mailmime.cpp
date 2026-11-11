#include "mailmime.h"
#include <QtTest>
using namespace MailMime;
class MailMimeTest:public QObject {
 Q_OBJECT
 Message example(){Message m;m.from={"发件人 😀","alice@example.com"};m.to={{"收件人","bob@example.com"}};m.subject="中文主题";m.text="你好\n第二行 😀";m.attachments={{"报告.bin","application/octet-stream",QByteArray("a\0b\r\nc",7)}};return m;}
private slots:
 void replyMetadataRoundtrip(){
  auto m=example();m.replyTo={"Support", "support@example.com"};m.messageId="<untrusted@example.com>";
  m.inReplyTo="<parent@example.com>";m.references={"<root@example.com>",m.inReplyTo};
  auto b=build(m);QVERIFY2(b.ok,qPrintable(b.error));QVERIFY(b.message.messageId!=m.messageId);
  auto p=parse(b.wire);QVERIFY2(p.ok,qPrintable(p.error));QCOMPARE(p.message.replyTo.address,m.replyTo.address);
  QCOMPARE(p.message.messageId,b.message.messageId);QCOMPARE(p.message.inReplyTo,m.inReplyTo);QCOMPARE(p.message.references,m.references);QVERIFY(!p.message.date.isEmpty());
  QVERIFY(build(m).message.messageId!=b.message.messageId);
  m.messageId="injected\r\nBcc: bad@example.com";m.date="injected\r\nX-Bad: yes";b=build(m);QVERIFY(b.ok);QVERIFY(!b.wire.contains("Bcc:"));QVERIFY(!b.wire.contains("X-Bad:"));QVERIFY(parse(b.wire).ok);
 }
 void replyDraftIsSafeAndUnsent(){
  auto original=example();original.replyTo={"Support", "support@example.com"};original.messageId="<parent@example.com>";original.references={"<root@example.com>"};
  Address sender={"Me","me@example.com"};auto r=makeReplyDraft(original,sender);QVERIFY2(r.ok,qPrintable(r.error));QVERIFY(r.wire.isEmpty());
  QCOMPARE(r.message.from.address,sender.address);QCOMPARE(r.message.to.size(),1);QCOMPARE(r.message.to.first().address,original.replyTo.address);
  QCOMPARE(r.message.subject,QString("Re: 中文主题"));QCOMPARE(r.message.inReplyTo,original.messageId);
  QCOMPARE(r.message.references,QStringList({"<root@example.com>","<parent@example.com>"}));
  QVERIFY(r.message.attachments.isEmpty());QVERIFY(r.message.html.isEmpty());QVERIFY(r.message.messageId.isEmpty());
  QCOMPARE(r.message.text,QString("\n\n> 你好\n> 第二行 😀"));QVERIFY(build(r.message).ok);
  original.replyTo={};original.subject="rE: existing";original.text.clear();original.html="<b>Hello</b><img src='file:///etc/passwd'><script>bad()</script>";
  r=makeReplyDraft(original,sender);QVERIFY(r.ok);QCOMPARE(r.message.to.first().address,original.from.address);QCOMPARE(r.message.subject,original.subject);
  QVERIFY(r.message.text.contains("> Hello"));QVERIFY(!r.message.text.contains("<img"));QVERIFY(!r.message.text.contains("bad()"));
 }
 void rejectsMaliciousReplyMetadata(){
  for(const auto &value:QStringList{"<ok@example.com>\r\nBcc: evil@example.com", "ok@example.com", "<one@example.com> <two@example.com>", "<bad..id@example.com>","<id@éxample.com>","<"+QString(255,'a')+"@example.com>"}){
   auto m=example();m.inReplyTo=value;QVERIFY(!build(m).ok);m.inReplyTo.clear();m.references={value};QVERIFY(!build(m).ok);
   for(const auto &header:QList<QByteArray>{"Message-ID", "In-Reply-To", "References"}){
    // CRLF is a new field in wire format, not a value; direct builder injection is tested above.
    if(value.contains('\r')||(header=="References"&&value=="<one@example.com> <two@example.com>"))continue;
    QVERIFY(!parse(header+": "+value.toUtf8()+"\r\n\r\nbody").ok);
   }
  }
  for(const auto &header:QList<QByteArray>{"Reply-To: a@example.com", "Message-ID: <a@example.com>", "In-Reply-To: <a@example.com>", "References: <a@example.com>"})
   QVERIFY(!parse(header+"\r\n"+header+"\r\n\r\nbody").ok);
  for(const auto &value:QStringList{"a@example.com, b@example.com", "a@example.com, B <b@example.com>", "=?UTF-8?Q?bad=0ABcc=3Ax?= <a@example.com>","a@example.com\rX:1"})
   QVERIFY(!parse("Reply-To: "+value.toUtf8()+"\r\n\r\nbody").ok);
  auto m=example();m.replyTo={"bad\nname","a@example.com"};QVERIFY(!build(m).ok);QVERIFY(!makeReplyDraft(m,{"Me","me@example.com"}).ok);
  m.replyTo={"name",""};QVERIFY(!makeReplyDraft(m,{"Me","me@example.com"}).ok);
  m.replyTo={};QVERIFY(!makeReplyDraft(m,{"Me","me@example.com\nBcc:x"}).ok);
 }
 void boundedReferences(){
  auto m=example();for(int i=0;i<101;++i)m.references.append(QString("<%1@example.com>").arg(i));
  QVERIFY(!build(m).ok);QVERIFY(!parse("References: "+m.references.join(' ').toLatin1()+"\r\n\r\nbody").ok);
  m.references.removeLast();m.messageId="<new@example.com>";auto draft=makeReplyDraft(m,{"Me","me@example.com"});QVERIFY(draft.ok);
  QCOMPARE(draft.message.references.size(),100);QCOMPARE(draft.message.references.last(),m.messageId);QVERIFY(build(draft.message).ok);
  for(const auto &line:build(draft.message).wire.split('\n'))QVERIFY(line.size()<998);
  m.references.clear();for(int i=0;i<40;++i)m.references.append("<"+QString(220,'a')+"@example.com>");
  QVERIFY(!build(m).ok);QVERIFY(!parse("References: "+m.references.join(' ').toLatin1()+"\r\n\r\nbody").ok);
 }
 void emptyBodyAndAttachment(){auto m=example();m.text.clear();m.attachments[0].data.clear();auto b=build(m);QVERIFY(b.ok);auto p=parse(b.wire);QVERIFY2(p.ok,qPrintable(p.error));QVERIFY(p.message.text.isEmpty());QCOMPARE(p.message.attachments.size(),1);QVERIFY(p.message.attachments.first().data.isEmpty());}
 void roundtripUtf8AndBinary(){auto m=example();auto built=build(m);QVERIFY2(built.ok,qPrintable(built.error));auto result=parse(built.wire);QVERIFY2(result.ok,qPrintable(result.error));QCOMPARE(result.message.subject,m.subject);QCOMPARE(result.message.from.displayName,m.from.displayName);QCOMPARE(result.message.to.first().displayName,m.to.first().displayName);QCOMPARE(result.message.text,m.text);QCOMPARE(result.message.attachments.size(),1);QCOMPARE(result.message.attachments.first().filename,m.attachments.first().filename);QCOMPARE(result.message.attachments.first().data,m.attachments.first().data);}
 void longUnicodeHeader(){auto m=example();m.subject=QString("中文😀").repeated(100);auto b=build(m);QVERIFY(b.ok);auto p=parse(b.wire);QVERIFY2(p.ok,qPrintable(p.error));QCOMPARE(p.message.subject,m.subject);for(auto line:b.wire.split('\n'))QVERIFY(line.size()<998);}
 void rejectsHeaderInjection(){for(int mode=0;mode<5;++mode){auto m=example();switch(mode){case 0:m.subject="hi\r\nBcc:evil@example.com";break;case 1:m.from.address="alice@example.com\nX:1";break;case 2:m.to[0].displayName="x\r\nBcc:x";break;case 3:m.attachments[0].filename="foo\nbar";break;case 4:m.attachments[0].contentType="text/plain\r\nX:1";break;}QVERIFY(!build(m).ok);}}
 void htmlAndQuotedPrintable(){auto raw=QByteArray("From: a@example.com\r\nSubject: =?UTF-8?Q?=E4=B8=AD=E6=96=87?=\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\n<p>Hello <b>world</b></p><img src=3D\"https://invalid.example/track\"><img src=3D\"file:///etc/passwd\"><script>bad()</script>");auto p=parse(raw);QVERIFY(p.ok);QCOMPARE(p.message.subject,QString("中文"));auto text=safeDisplayText(p.message);QVERIFY(text.contains("Hello world"));QVERIFY(!text.contains("<img"));QVERIFY(!text.contains("root:"));QVERIFY(!text.contains("bad()"));}
 void traceHeadersAndStandardFilename(){
  auto b=build(example());QVERIFY(b.ok);QVERIFY(b.wire.startsWith("Date: "));QVERIFY(b.wire.contains("\r\nMessage-ID: <"));QVERIFY(b.wire.contains("filename*=UTF-8''"));
  const auto traced=QByteArray("Received: from relay1 by relay2\r\nReceived: from sender by relay1\r\nX-Trace: one\r\nX-Trace: two\r\n")+b.wire;
  auto p=parse(traced);QVERIFY2(p.ok,qPrintable(p.error));QCOMPARE(p.message.attachments.first().filename,QString("报告.bin"));
  auto bad=b.wire;bad.replace("filename*=UTF-8''","filename*=UTF-8''%ZZ");QVERIFY(!parse(bad).ok);
 }
 void limitsAndMalformed(){auto m=example();Limits l;l.maxDecodedBytes=1;QVERIFY(!build(m,l).ok);auto b=build(m);l={};l.maxWireBytes=10;QVERIFY(!parse(b.wire,l).ok);QVERIFY(!parse("Subject: x\r\nSubject: y\r\n\r\na").ok);QVERIFY(!parse("Content-Type: multipart/mixed; boundary=\"x\"\r\n\r\n--x\r\nContent-Type: text/plain\r\n\r\na").ok);QVERIFY(!parse("Content-Transfer-Encoding: base64\r\n\r\n!invalid!").ok);QVERIFY(!parse("Content-Type: text/plain; charset=unsupported-charset\r\n\r\na").ok);}
 void nestedAlternativeAndBinaryAttachment(){QByteArray raw="MIME-Version: 1.0\r\nContent-Type: multipart/mixed; boundary=outer\r\n\r\n--outer\r\nContent-Type: multipart/alternative; boundary=inner\r\n\r\n--inner\r\nContent-Type: text/plain; charset=UTF-8\r\n\r\nplain\r\n--inner\r\nContent-Type: text/html; charset=UTF-8\r\n\r\n<b>html</b>\r\n--inner--\r\n--outer\r\nContent-Type: application/octet-stream\r\nContent-Disposition: attachment; filename=\"../../file.bin\"\r\nContent-Transfer-Encoding: binary\r\n\r\n";QByteArray binary("a\r\nb\0c",6);raw+=binary;raw+="\r\n--outer--\r\n";auto p=parse(raw);QVERIFY2(p.ok,qPrintable(p.error));QCOMPARE(p.message.text,QString("plain"));QCOMPARE(safeDisplayText(p.message),QString("plain"));QCOMPARE(p.message.attachments.first().filename,QString("file.bin"));QCOMPARE(p.message.attachments.first().data,binary);Limits l;l.maxDepth=1;QVERIFY(!parse(raw,l).ok);}
};
QTEST_MAIN(MailMimeTest)
#include "test_mailmime.moc"
