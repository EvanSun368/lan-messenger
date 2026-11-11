#include "mailsyncstate.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QRegularExpression>
#include <algorithm>
namespace {
constexpr qint64 MaxFileBytes=1024*1024;
bool number(const QString &text,bool zero=false){
 static const QRegularExpression digits("^[0-9]{1,10}$");
 bool ok=false;const auto value=text.toULongLong(&ok);
 return digits.match(text).hasMatch()&&ok&&(zero||value>0)&&value<=0xffffffffULL;
}
bool account(const QString &id){return !id.isEmpty()&&id.size()<=256&&!id.contains(QChar(0));}
bool error(QString *out,const QString &text){if(out)*out=text;return false;}
}
MailSyncState::MailSyncState(QString path):path_(std::move(path)){}
QString MailSyncState::validity(const QString &id)const{return cursors_.value(id).validity;}
void MailSyncState::remove(const QString &id){cursors_.remove(id);}
MailSyncState::Result MailSyncState::apply(const QString &id,const QString &validity,const QList<quint64> &uids){
 Result result;
 if(!account(id)||!number(validity)||uids.size()>MaxUids){result.error="无效的邮件同步标识或数量";return result;}
 for(auto uid:uids)if(!uid||uid>0xffffffffULL){result.error="无效的邮件同步 UID";return result;}
 if(!cursors_.contains(id)&&cursors_.size()>=MaxAccounts){result.error="邮件同步账户数量超出限制";return result;}
 const auto canonical=QString::number(validity.toULongLong());
 auto it=cursors_.find(id);result.baseline=it==cursors_.end()||it->validity!=canonical;
 result.reset=it!=cursors_.end()&&it->validity!=canonical;
 quint64 highwater=result.baseline?0:it->highwater;
 const auto previous=highwater;
 for(auto uid:uids){if(!result.baseline&&uid>previous)result.newUids.append(uid);highwater=qMax(highwater,uid);}
 std::sort(result.newUids.begin(),result.newUids.end());
 result.newUids.erase(std::unique(result.newUids.begin(),result.newUids.end()),result.newUids.end());
 cursors_.insert(id,{canonical,highwater});result.ok=true;return result;
}
bool MailSyncState::load(QString *out){
 if(out)out->clear();if(path_.isEmpty())return true;
 QFile file(path_);if(!file.exists()){cursors_.clear();return true;}
 if(!file.open(QIODevice::ReadOnly))return error(out,"无法读取邮件同步状态");
 if(file.size()>MaxFileBytes)return error(out,"邮件同步状态文件过大");
 QJsonParseError parse;const auto doc=QJsonDocument::fromJson(file.read(MaxFileBytes+1),&parse);
 if(parse.error!=QJsonParseError::NoError||!doc.isObject())return error(out,"邮件同步状态格式无效");
 const auto root=doc.object();if(root["version"].toInt()!=1||!root["accounts"].isArray())return error(out,"邮件同步状态版本或内容无效");
 const auto entries=root["accounts"].toArray();if(entries.size()>MaxAccounts)return error(out,"邮件同步账户数量超出限制");
 QHash<QString,Cursor> next;
 for(const auto &entry:entries){const auto item=entry.toObject();const auto id=item["id"].toString(),validity=item["uidValidity"].toString(),high=item["highwater"].toString();
  if(!account(id)||!number(validity)||!number(high,true)||next.contains(id))return error(out,"邮件同步游标无效");
  next.insert(id,{QString::number(validity.toULongLong()),high.toULongLong()});
 }
 cursors_=std::move(next);return true;
}
bool MailSyncState::save(QString *out)const{
 if(out)out->clear();if(path_.isEmpty())return true;
 QJsonArray entries;auto ids=cursors_.keys();std::sort(ids.begin(),ids.end());
 for(const auto &id:ids){const auto cursor=cursors_.value(id);entries.append(QJsonObject{{"id",id},{"uidValidity",cursor.validity},{"highwater",QString::number(cursor.highwater)}});}
 const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"accounts",entries}}).toJson(QJsonDocument::Compact);
 if(bytes.size()>MaxFileBytes)return error(out,"邮件同步状态文件过大");
 QSaveFile file(path_);if(!file.open(QIODevice::WriteOnly))return error(out,"无法保存邮件同步状态");
 file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
 if(file.write(bytes)!=bytes.size()||!file.commit())return error(out,"无法完成邮件同步状态保存");
 return true;
}
