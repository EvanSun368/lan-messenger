#include "contactmailbook.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
namespace {
bool fail(QString *error,const QString &text){if(error)*error=text;return false;}
bool validPeer(const QString &id){return QRegularExpression("^[a-f0-9]{32}$").match(id).hasMatch();}
}
bool ContactMailBook::validEmail(const QString &email){
 if(email.isEmpty())return true; // Empty means erase, not an address.
 if(email.size()>254)return false;
 for(const auto c:email)if(c.unicode()<32 || c.unicode()==127)return false;
 return QRegularExpression("^[^\\s@<>,;:]+@[^\\s@<>,;:]+$").match(email).hasMatch();
}
bool ContactMailBook::load(QString *error){
 if(error)error->clear();QFile file(path_);
 if(!file.exists()){entries_.clear();return true;}
 if(!file.open(QIODevice::ReadOnly))return fail(error,"无法读取本机联系人邮箱");
 if(file.size()>1024*1024)return fail(error,"本机联系人邮箱文件过大");
 QJsonParseError parse;auto doc=QJsonDocument::fromJson(file.readAll(),&parse);
 const auto root=doc.object();
 if(parse.error!=QJsonParseError::NoError || !doc.isObject() || root.size()!=2 ||
    root["version"]!=1 || !root["emails"].isObject())return fail(error,"本机联系人邮箱格式无效");
 QHash<QString,QString> next;const auto emails=root["emails"].toObject();
 if(emails.size()>10000)return fail(error,"本机联系人邮箱数量过多");
 for(auto it=emails.begin();it!=emails.end();++it){
  if(!validPeer(it.key()) || !it.value().isString() || it.value().toString().isEmpty() ||
     !validEmail(it.value().toString()))return fail(error,"本机联系人邮箱包含无效条目");
  next.insert(it.key(),it.value().toString());
 }
 entries_=next;return true;
}
bool ContactMailBook::setEmail(const QString &peerId,QString email,QString *error){
 if(error)error->clear();
 if(!validPeer(peerId))return fail(error,"联系人ID无效");
 // Reject controls before normalization; CR/LF must never be silently accepted.
 if(!validEmail(email))return fail(error,"邮箱地址无效，请输入单个邮箱，不能包含控制字符");
 auto next=entries_;if(email.isEmpty())next.remove(peerId);else next[peerId]=email;
 if(next.size()>10000)return fail(error,"本机联系人邮箱数量过多");
 QJsonObject values;for(auto it=next.begin();it!=next.end();++it)values[it.key()]=it.value();
 const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"emails",values}}).toJson();
 QSaveFile file(path_);
 if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit())
  return fail(error,"无法保存本机联系人邮箱，修改未生效");
 entries_=next;return true;
}
