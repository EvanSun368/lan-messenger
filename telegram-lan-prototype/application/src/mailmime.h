#pragma once
#include <QString>
#include <QByteArray>
#include <QList>
#include <QStringList>
namespace MailMime {
struct Address { QString displayName,address; };
struct Attachment {QString filename,contentType;QByteArray data;};
struct Message {Address from;QList<Address> to;QString subject,text,html;QList<Attachment> attachments;
 Address replyTo;QString messageId,inReplyTo,date;QStringList references;
};
struct Limits {qsizetype maxWireBytes=25*1024*1024;qsizetype maxDecodedBytes=20*1024*1024;int maxParts=100;int maxDepth=8;};
struct Result {bool ok=false;QString error;Message message;QByteArray wire;};
// Minimal MIME scope: plain outbound, base64 attachments; inbound multipart,
// plain/html, base64/quoted-printable/7bit/8bit/binary, RFC2047 words and single
// RFC2231 filename*. No RFC2231 continuation segments, address groups/comments,
// encrypted/signed mail verification or SMTP transport. Attachment filenames are
// metadata, not authorization to create/overwrite local paths.
Result build(const Message &message,const Limits &limits={});
Result parse(const QByteArray &wire,const Limits &limits={});
// Strict ASCII dot-atom Message-IDs, including <>, max 254 characters each.
// References: max 100 IDs / 8192 characters. Draft creation never sends mail.
Result makeReplyDraft(const Message &original,const Address &sender);
// Render ONLY this output with setPlainText, never setHtml. HTML-only mail is
// converted by a QTextDocument subclass denying all resource loads; URLs are not fetched.
QString safeDisplayText(const Message &message);
}
