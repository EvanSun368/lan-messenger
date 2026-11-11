#pragma once
#include <QJsonArray>
#include <QStringList>
namespace ChatMailExport {
struct Result {bool ok=false;QString error,subject,body;int selectedCount=0;};
// Pure local draft export. Records must bind selected ids to conversationId via
// peer_id (Node history schema), conversation_id or peer; conflicting bindings
// fail closed. Order is the supplied history order, NOT checkbox order.
// No network, file reads, binary attachment or recipient inference.
Result build(const QString &conversationId,const QJsonArray &records,
 const QStringList &selectedIds,const QString &conversationTitle={},
 qsizetype maxUtf8Bytes=1024*1024);
}
