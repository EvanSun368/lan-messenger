#include "mailaccounts.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUuid>
#include <QSet>
#include <QHostAddress>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#endif
namespace {
bool fail(QString *error,const QString &text){if(error)*error=text;return false;}
bool validId(const QString &id){return QRegularExpression("^[a-f0-9]{32}$").match(id).hasMatch();}
QString key(const QString &id){return "LanLink/Mail/"+id;}
bool hasControl(const QString &s){for(const auto c:s)if(c.unicode()<32 || c.unicode()==127)return true;return false;}
bool validHost(const QString &s){
 if(s.isEmpty() || hasControl(s) || s.size()>253)return false;
 QHostAddress literal;if(literal.setAddress(s))return true;
 if(s.contains(':') || QRegularExpression("^[0-9.]+$").match(s).hasMatch())return false;
 return s.size()<=253 && QRegularExpression("^[A-Za-z0-9](?:[A-Za-z0-9.-]*[A-Za-z0-9])?$").match(s).hasMatch()
    && !s.contains("..");
}
QJsonObject endpointJson(const MailEndpoint &e){
 return {{"host",e.host},{"port",e.port},{"tls",e.tls==MailTls::ImplicitTls?"tls":"starttls"}};
}
QJsonObject accountJson(const MailAccount &a){
 return {{"id",a.id},{"label",a.label},{"address",a.address},{"provider",a.provider},{"username",a.userName},
    {"imap",endpointJson(a.imap)},{"smtp",endpointJson(a.smtp)},{"auth",a.auth==MailAuth::Password?"password":"oauth2"},
    {"oauth",QJsonObject{{"clientId",a.oauthClientId},{"tenant",a.oauthTenant}}}};
}
bool onlyKeys(const QJsonObject &o,const QStringList &keys){
 for(auto it=o.begin();it!=o.end();++it)if(!keys.contains(it.key()))return false;
 return true;
}
bool parseEndpoint(const QJsonValue &v,MailEndpoint *e){
 if(!v.isObject())return false;const auto o=v.toObject();
 if(!onlyKeys(o,{"host","port","tls"}) || !o["host"].isString() || !o["port"].isDouble())return false;
 const auto port=o["port"].toDouble();if(port<1 || port>65535 || port!=int(port))return false;
 if(o["tls"]!="tls" && o["tls"]!="starttls")return false;
 e->host=o["host"].toString();e->port=int(port);e->tls=o["tls"]=="tls"?MailTls::ImplicitTls:MailTls::StartTls;
 return true;
}
bool parseAccount(const QJsonValue &v,MailAccount *a){
 if(!v.isObject())return false;const auto o=v.toObject();
 if(!onlyKeys(o,{"id","label","address","provider","username","imap","smtp","auth","oauth"}))return false;
 for(auto field:{"id","label","address","provider","username","auth"})if(!o[field].isString())return false;
 if(o["auth"]!="password" && o["auth"]!="oauth2")return false;
 a->id=o["id"].toString();a->label=o["label"].toString();a->address=o["address"].toString();
 a->provider=o["provider"].toString();a->userName=o["username"].toString();
 a->auth=o["auth"]=="password"?MailAuth::Password:MailAuth::OAuth2;
 if(o.contains("oauth")){
  if(!o["oauth"].isObject())return false;const auto oauth=o["oauth"].toObject();
  if(!onlyKeys(oauth,{"clientId","tenant"})||!oauth["clientId"].isString()||!oauth["tenant"].isString())return false;
  a->oauthClientId=oauth["clientId"].toString();a->oauthTenant=oauth["tenant"].toString();
 }
 return parseEndpoint(o["imap"],&a->imap)&&parseEndpoint(o["smtp"],&a->smtp)&&validateMailAccount(*a).isEmpty();
}
}
QList<MailProviderTemplate> mailProviderTemplates(){
 return {
 {"manual","内网 / 手动配置","填写企业邮箱服务器；必须使用TLS或STARTTLS，证书必须正常验证。",{"",993,MailTls::ImplicitTls},{"",587,MailTls::StartTls},MailAuth::Password,true},
 {"qq","QQ邮箱","需要在QQ邮箱开启IMAP/SMTP并使用授权码，不是QQ登录密码。",{"imap.qq.com",993,MailTls::ImplicitTls},{"smtp.qq.com",465,MailTls::ImplicitTls},MailAuth::Password,true},
 {"163","163邮箱","需要开启IMAP/SMTP并使用客户端授权密码。",{"imap.163.com",993,MailTls::ImplicitTls},{"smtp.163.com",465,MailTls::ImplicitTls},MailAuth::Password,true},
 {"gmail","Gmail","OAuth2 + PKCE：需自行注册 Google 桌面客户端并填写客户端 ID；邮件范围可能需要 Google 审核。尚未真实账户验收。",{"imap.gmail.com",993,MailTls::ImplicitTls},{"smtp.gmail.com",587,MailTls::StartTls},MailAuth::OAuth2,true},
 {"gmail-app-password","Gmail（应用专用密码）","仅当账号政策允许、已开启两步验证（2FA）并生成应用专用密码时可用。必须使用应用专用密码，不是普通登录密码；尚未通过真实供应商验收。",{"imap.gmail.com",993,MailTls::ImplicitTls},{"smtp.gmail.com",587,MailTls::StartTls},MailAuth::Password,true},
 {"outlook-personal","Outlook.com / Hotmail（个人邮箱）","OAuth2 + PKCE：需支持个人 Microsoft 账户的公共客户端注册；开启 IMAP。SMTP 使用 smtp-mail.outlook.com，尚未真实账户验收。",{"outlook.office365.com",993,MailTls::ImplicitTls},{"smtp-mail.outlook.com",587,MailTls::StartTls},MailAuth::OAuth2,true},
 {"outlook","Microsoft 365（企业 / 学校）","OAuth2 + PKCE：需自行注册 Microsoft 公共桌面客户端；配置 localhost 回调及委托 IMAP/SMTP 权限，服务端需允许 IMAP 和 SMTP AUTH。尚未真实账户验收。",{"outlook.office365.com",993,MailTls::ImplicitTls},{"smtp.office365.com",587,MailTls::StartTls},MailAuth::OAuth2,true}
 };
}
MailAccount mailAccountFromTemplate(const QString &providerId,const QString &address){
 MailAccount a;a.id=QUuid::createUuid().toString(QUuid::Id128);a.provider=providerId;if(providerId=="outlook-personal")a.oauthTenant="consumers";
 a.address=address;a.userName=address;a.label=address;
 for(const auto &p:mailProviderTemplates())if(p.id==providerId){a.imap=p.imap;a.smtp=p.smtp;a.auth=p.auth;break;}
 return a;
}
QStringList validateMailAccount(const MailAccount &a){
 QStringList errors;
 if(!validId(a.id))errors<<"账户ID无效";
 if(a.label.trimmed().isEmpty() || a.label.size()>200)errors<<"账户名称无效";
 if(hasControl(a.address) || !QRegularExpression("^[^\\s@<>]+@[^\\s@<>]+$").match(a.address).hasMatch() || a.address.size()>254)errors<<"邮箱地址无效";
 if(a.userName.trimmed().isEmpty() || a.userName.size()>254 || hasControl(a.userName))errors<<"登录用户名无效";
 bool known=false;
 for(const auto &p:mailProviderTemplates())if(p.id==a.provider){
  known=true;if(!p.available)errors<<p.notice;
  if(a.auth!=p.auth)errors<<"认证方式与邮箱模板不一致";
  if(p.auth==MailAuth::OAuth2 && (a.imap.host!=p.imap.host||a.smtp.host!=p.smtp.host||a.imap.port!=p.imap.port||a.smtp.port!=p.smtp.port||a.imap.tls!=p.imap.tls||a.smtp.tls!=p.smtp.tls))
   errors<<"OAuth2 令牌仅可用于所选供应商的固定 TLS 邮件服务器";
 }
 if(!known)errors<<"未知邮箱模板，请选择内网 / 手动配置";
 if(a.auth==MailAuth::OAuth2){
  if(a.oauthClientId.isEmpty()||a.oauthClientId.size()>512||!QRegularExpression("^[A-Za-z0-9._-]+$").match(a.oauthClientId).hasMatch())errors<<"请填写自己的 OAuth 公共客户端 ID";
  if(!QRegularExpression("^[A-Za-z0-9][A-Za-z0-9.-]{0,252}$").match(a.oauthTenant).hasMatch())errors<<"Microsoft 租户无效";
 }else if(a.auth!=MailAuth::Password)errors<<"认证方式无效";
 for(const auto &e:{a.imap,a.smtp}){
  if(!validHost(e.host))errors<<"服务器主机名无效（不要输入URL或路径）";
  if(e.port<1 || e.port>65535)errors<<"服务器端口必须为1至65535";
  if(e.tls!=MailTls::ImplicitTls && e.tls!=MailTls::StartTls)errors<<"禁止未加密连接";
 }
 return errors;
}
bool PlatformMailCredentialStore::available() const {
#ifdef Q_OS_WIN
 return true;
#else
 return false;
#endif
}
QString PlatformMailCredentialStore::unavailableReason() const {
 return available()?QString():QString("当前平台尚未接入系统凭据服务；不会以明文保存密码或令牌。");
}
bool PlatformMailCredentialStore::save(const QString &id,const QByteArray &secret,QString *error){
 if(error)error->clear();if(!validId(id)||secret.isEmpty())return fail(error,"凭据账户ID或内容无效");
#ifdef Q_OS_WIN
 if(secret.size()>CRED_MAX_CREDENTIAL_BLOB_SIZE)return fail(error,"凭据超过系统大小限制");
 const auto target=key(id);CREDENTIALW c{};c.Type=CRED_TYPE_GENERIC;
 c.TargetName=const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(target.utf16()));
 c.CredentialBlobSize=DWORD(secret.size());c.CredentialBlob=reinterpret_cast<LPBYTE>(const_cast<char*>(secret.constData()));
 c.Persist=CRED_PERSIST_LOCAL_MACHINE;
 if(CredWriteW(&c,0))return true;
 return fail(error,QString("Windows凭据保存失败（错误%1）").arg(GetLastError()));
#else
 Q_UNUSED(secret);return fail(error,unavailableReason());
#endif
}
bool PlatformMailCredentialStore::load(const QString &id,QByteArray *secret,QString *error){
 if(secret)secret->clear();if(error)error->clear();if(!validId(id)||!secret)return fail(error,"凭据请求无效");
#ifdef Q_OS_WIN
 const auto target=key(id);PCREDENTIALW c=nullptr;
 if(!CredReadW(reinterpret_cast<LPCWSTR>(target.utf16()),CRED_TYPE_GENERIC,0,&c))
  return fail(error,QString("Windows凭据读取失败（错误%1）").arg(GetLastError()));
 *secret=QByteArray(reinterpret_cast<const char*>(c->CredentialBlob),int(c->CredentialBlobSize));
 CredFree(c);return true;
#else
 return fail(error,unavailableReason());
#endif
}
bool PlatformMailCredentialStore::remove(const QString &id,QString *error){
 if(error)error->clear();if(!validId(id))return fail(error,"凭据账户ID无效");
#ifdef Q_OS_WIN
 const auto target=key(id);
 if(CredDeleteW(reinterpret_cast<LPCWSTR>(target.utf16()),CRED_TYPE_GENERIC,0))return true;
 const auto e=GetLastError();if(e==ERROR_NOT_FOUND)return true;
 return fail(error,QString("Windows凭据删除失败（错误%1）").arg(e));
#else
 return fail(error,unavailableReason());
#endif
}
MailAccounts::MailAccounts(QString path,std::shared_ptr<MailCredentialStore> store)
 :path_(std::move(path)),credentials_(store?std::move(store):std::make_shared<PlatformMailCredentialStore>()){}
bool MailAccounts::contains(const QString &id) const {for(const auto &a:accounts_)if(a.id==id)return true;return false;}
bool MailAccounts::load(QString *error){
 if(error)error->clear();QFile f(path_);if(!f.exists()){accounts_.clear();return true;}
 if(!f.open(QIODevice::ReadOnly))return fail(error,"无法读取邮件账户配置");
 if(f.size()>1024*1024)return fail(error,"账户配置过大");
 QJsonParseError parse;const auto doc=QJsonDocument::fromJson(f.readAll(),&parse);const auto root=doc.object();
 if(parse.error!=QJsonParseError::NoError || !doc.isObject() || !onlyKeys(root,{"version","accounts"})
    || root["version"]!=1 || !root["accounts"].isArray())return fail(error,"账户配置格式无效");
 QList<MailAccount> next;QSet<QString> ids;
 for(const auto &v:root["accounts"].toArray()){MailAccount a;if(next.size()>=100 || !parseAccount(v,&a)||ids.contains(a.id))return fail(error,"账户配置包含无效或不支持的账户");
  ids.insert(a.id);next.append(a);}
 accounts_=next;return true;
}
bool MailAccounts::write(const QList<MailAccount>&accounts,QString *error){
 QJsonArray list;for(const auto &a:accounts)list.append(accountJson(a));
 QSaveFile f(path_);if(!f.open(QIODevice::WriteOnly))return fail(error,"无法写入邮件账户配置");
 const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"accounts",list}}).toJson();
 if(f.write(bytes)!=bytes.size()||!f.commit())return fail(error,"保存邮件账户配置失败");
 if(error)error->clear();return true;
}
bool MailAccounts::upsert(const MailAccount &a,QString *error){
 const auto validation=validateMailAccount(a);if(!validation.isEmpty())return fail(error,validation.join("；"));
 auto next=accounts_;bool replaced=false;
 for(auto &old:next)if(old.id==a.id){old=a;replaced=true;break;}
 if(!replaced){if(next.size()>=100)return fail(error,"账户数量超过限制");next.append(a);}
 if(!write(next,error))return false;accounts_=next;return true;
}
bool MailAccounts::remove(const QString &id,QString *error){
 if(!contains(id))return fail(error,"账户不存在");
 if(!credentials_->remove(id,error))return false;
 auto next=accounts_;for(qsizetype i=0;i<next.size();++i)if(next[i].id==id){next.removeAt(i);break;}
 if(!write(next,error))return false;accounts_=next;return true;
}
bool MailAccounts::saveCredential(const QString &id,const QByteArray &secret,QString *error){
 if(!contains(id))return fail(error,"账户不存在");return credentials_->save(id,secret,error);
}
bool MailAccounts::loadCredential(const QString &id,QByteArray *secret,QString *error){
 if(!secret)return fail(error,"凭据输出参数无效");
 secret->clear();if(!contains(id))return fail(error,"账户不存在");return credentials_->load(id,secret,error);
}
