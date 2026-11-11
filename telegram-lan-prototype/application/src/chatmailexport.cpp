#include "chatmailexport.h"
#include <QJsonObject>
#include <QSet>
#include <QDateTime>
#include <QTimeZone>
#include <cmath>
namespace ChatMailExport {
namespace {
bool safeLabel(const QString &value){for(auto c:value)if(c.unicode()<32||c.unicode()==127)return false;return true;}
QString sender(const QJsonObject &r){
 auto id=r.value("sender_id").toString();auto name=r.value("sender_name").toString();
 if(!safeLabel(id)||!safeLabel(name))return {};
 if(!name.isEmpty())return id.isEmpty()?name:name+" ["+id+"]";
 if(!id.isEmpty())return id;
 return r.value("direction")=="out"?QStringLiteral("我（记录未提供发送者 ID）"):QStringLiteral("发送者未知");
}
QString timeLabel(const QJsonObject &r){
 const auto value=r.contains("timestamp")?r.value("timestamp"):r.value("time");
 if(!value.isDouble())return QStringLiteral("时间未知");
 const double seconds=value.toDouble();
 // Limit to supported civil dates, avoiding undefined floating->integer casts.
 if(!std::isfinite(seconds)||seconds < -62135596800.0||seconds > 253402300799.0)return QStringLiteral("时间无效");
 const auto date=QDateTime::fromMSecsSinceEpoch(qint64(seconds*1000),QTimeZone::UTC);
 return date.isValid()?date.toString(Qt::ISODateWithMs):QStringLiteral("时间无效");
}
}
Result build(const QString &conversationId,const QJsonArray &records,const QStringList &selectedIds,
 const QString &conversationTitle,qsizetype maxUtf8Bytes){
 Result result;auto fail=[&](const QString &why){Result error;error.error=why;return error;};
 if(conversationId.isEmpty()||!safeLabel(conversationId)||conversationId.size()>512)return fail("会话标识无效");
 if(!safeLabel(conversationTitle)||conversationTitle.size()>200)return fail("会话标题无效或过长");
 if(maxUtf8Bytes<=0||maxUtf8Bytes>20*1024*1024)return fail("导出大小限制无效");
 if(selectedIds.isEmpty())return fail("请先选择消息");
 if(selectedIds.size()>1000)return fail("一次最多导出 1000 条消息");
 QSet<QString> requested,seen;
 for(const auto &id:selectedIds){if(id.isEmpty()||id.size()>256||!safeLabel(id)||requested.contains(id))return fail("消息 ID 为空、无效或重复");requested.insert(id);}
 result.subject=QStringLiteral("聊天记录：")+(conversationTitle.isEmpty()?conversationId:conversationTitle);
 result.body=QStringLiteral("以下为手动选择的聊天记录（时间使用 UTC）。\n会话：")+(conversationTitle.isEmpty()?conversationId:conversationTitle)+"\n\n";
 qsizetype bytes=result.subject.toUtf8().size()+result.body.toUtf8().size();
 if(bytes>maxUtf8Bytes)return fail("选中消息超过邮件草稿大小限制");
 for(const auto &entry:records){
  if(!entry.isObject())continue;const auto r=entry.toObject();const auto id=r.value("id").toString();if(!requested.contains(id))continue;
  if(seen.contains(id))return fail("历史记录存在重复消息 ID，无法安全导出");seen.insert(id);
  bool bound=false;
  for(const auto &key:{"peer_id","conversation_id","peer"})if(r.contains(key)){
   if(!r.value(key).isString()||r.value(key).toString()!=conversationId)return fail("所选消息不属于当前会话");bound=true;
  }
  if(!bound)return fail("所选消息缺少会话归属，拒绝导出");
  if(r.contains("text")&&!r.value("text").isString())return fail("消息正文格式无效");
  const auto author=sender(r);if(author.isEmpty())return fail("发送者信息无效");
  QString block="["+timeLabel(r)+"] "+author+"\n"+r.value("text").toString();
  const bool image=!r.value("image_path").toString().isEmpty()||r.value("type")=="image";
  const bool file=!r.value("transfer_id").toString().isEmpty()||r.value("type")=="file_transfer"||id.startsWith("file:");
  if(image)block+=QStringLiteral("\n[图片：仅导出文字说明，未包含图片文件]");
  if(file)block+=QStringLiteral("\n[文件：仅导出文字说明，未附加文件]");
  block+="\n\n";
  bytes+=block.toUtf8().size();if(bytes>maxUtf8Bytes)return fail("选中消息超过邮件草稿大小限制");
  result.body+=block;++result.selectedCount;
 }
 if(seen!=requested)return fail("部分所选消息不存在或尚未加载，请重新选择");
 result.ok=true;return result;
}
}
