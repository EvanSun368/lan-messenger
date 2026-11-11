#include "mailaccounts.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
class MockSecrets final:public MailCredentialStore {
public:
 QHash<QString,QByteArray> data;bool failWrites=false;
 bool available() const override{return true;}
 QString unavailableReason() const override{return {};}
 bool save(const QString &id,const QByteArray &s,QString *e) override {if(failWrites){if(e)*e="locked";return false;}data[id]=s;return true;}
 bool load(const QString &id,QByteArray *s,QString *e) override {s->clear();if(!data.contains(id)){if(e)*e="missing";return false;}*s=data[id];return true;}
 bool remove(const QString &id,QString *e) override {if(failWrites){if(e)*e="locked";return false;}data.remove(id);return true;}
};
class MailAccountTests:public QObject {
 Q_OBJECT
private slots:
 void templatesAndTlsValidation(){
  for(const auto &p:mailProviderTemplates()){
   auto a=mailAccountFromTemplate(p.id,"person@example.com");
   if(p.id=="manual"){a.imap.host="mail.internal";a.smtp.host="mail.internal";}
   if(a.auth==MailAuth::OAuth2)a.oauthClientId="test-public-client-id";
   QCOMPARE(validateMailAccount(a).isEmpty(),p.available);
   if(!p.available){a.auth=MailAuth::Password;QVERIFY(!validateMailAccount(a).isEmpty());}
  }
  auto a=mailAccountFromTemplate("manual","person@example.com");
  a.imap.host="mail";a.smtp.host="10.1.2.3";QVERIFY(validateMailAccount(a).isEmpty());
  a.imap.host="fd00::1234";a.smtp.host="2001:db8::20";QVERIFY(validateMailAccount(a).isEmpty());
  for(ushort code=0;code<32;++code){
   auto invalid=a;invalid.userName="alice"+QString(QChar(code));QVERIFY(!validateMailAccount(invalid).isEmpty());
   invalid=a;invalid.address="alice"+QString(QChar(code))+"@example.com";QVERIFY(!validateMailAccount(invalid).isEmpty());
  }
  a.smtp.tls=static_cast<MailTls>(99);QVERIFY(!validateMailAccount(a).isEmpty());
  a.smtp.tls=MailTls::StartTls;a.imap.host="https://mail/path";QVERIFY(!validateMailAccount(a).isEmpty());
 }
 void gmailAuthenticationChoicesRemainDistinct(){
  auto oauth=mailAccountFromTemplate("gmail","person@gmail.com");
  QCOMPARE(oauth.auth,MailAuth::OAuth2);
  QVERIFY(!validateMailAccount(oauth).isEmpty());
  oauth.oauthClientId="test-google-client.apps.googleusercontent.com";
  QVERIFY(validateMailAccount(oauth).isEmpty());
  oauth.auth=MailAuth::Password;
  QVERIFY(!validateMailAccount(oauth).isEmpty()); // Can't silently change OAuth template.
  const auto app=mailAccountFromTemplate("gmail-app-password","person@gmail.com");
  QCOMPARE(app.auth,MailAuth::Password);QVERIFY(validateMailAccount(app).isEmpty());
  QCOMPARE(app.imap.host,QString("imap.gmail.com"));QCOMPARE(app.imap.port,993);
  QCOMPARE(app.smtp.tls,MailTls::StartTls);
  bool found=false;
  for(const auto &p:mailProviderTemplates())if(p.id=="gmail-app-password"){
   found=true;QVERIFY(p.available);QVERIFY(p.label.contains("应用专用密码"));
   QVERIFY(p.notice.contains("2FA"));QVERIFY(p.notice.contains("不是普通登录密码"));
   QVERIFY(p.notice.contains("尚未通过真实供应商验收"));
  }
  QVERIFY(found);
  auto outlook=mailAccountFromTemplate("outlook","person@outlook.com");
  QCOMPARE(outlook.auth,MailAuth::OAuth2);QVERIFY(!validateMailAccount(outlook).isEmpty());
  outlook.oauthClientId="test-public-client-id";QVERIFY(validateMailAccount(outlook).isEmpty());
  outlook.imap.host="attacker.example";QVERIFY(!validateMailAccount(outlook).isEmpty());
  outlook=mailAccountFromTemplate("outlook","person@outlook.com");outlook.oauthClientId="test-public-client-id";
  outlook.oauthTenant="common/../../attacker";QVERIFY(!validateMailAccount(outlook).isEmpty());
 }
 void personalOutlookUsesDistinctPinnedEndpoints(){
  auto personal=mailAccountFromTemplate("outlook-personal","fixture@outlook.com");personal.oauthClientId="test-client";
  QCOMPARE(personal.oauthTenant,QString("consumers"));QCOMPARE(personal.imap.host,QString("outlook.office365.com"));QCOMPARE(personal.smtp.host,QString("smtp-mail.outlook.com"));QCOMPARE(personal.smtp.port,587);QCOMPARE(personal.smtp.tls,MailTls::StartTls);QCOMPARE(personal.auth,MailAuth::OAuth2);QVERIFY(validateMailAccount(personal).isEmpty());
  auto business=mailAccountFromTemplate("outlook","fixture@example.invalid");business.oauthClientId="test-client";QCOMPARE(business.oauthTenant,QString("common"));QCOMPARE(business.smtp.host,QString("smtp.office365.com"));QVERIFY(validateMailAccount(business).isEmpty());
  personal.smtp.host=business.smtp.host;QVERIFY(!validateMailAccount(personal).isEmpty());business.smtp.host="smtp-mail.outlook.com";QVERIFY(!validateMailAccount(business).isEmpty());
 }
 void oauthConfigurationPersistsOnlyPublicRegistration(){
  QTemporaryDir dir;auto secrets=std::make_shared<MockSecrets>();const auto path=dir.filePath("accounts.json");
  MailAccounts store(path,secrets);auto account=mailAccountFromTemplate("outlook","test@outlook.com");account.oauthClientId="public-client-id";account.oauthTenant="organizations";
  QVERIFY(store.upsert(account));QVERIFY(store.saveCredential(account.id,"fake-refresh-token-only-in-secret-store"));
  QFile f(path);QVERIFY(f.open(QIODevice::ReadOnly));const auto bytes=f.readAll();QVERIFY(bytes.contains("public-client-id"));QVERIFY(!bytes.contains("fake-refresh"));QVERIFY(!bytes.contains("client_secret"));QVERIFY(!bytes.contains("refresh_token"));
  MailAccounts restored(path,secrets);QVERIFY(restored.load());QCOMPARE(restored.accounts().first().oauthClientId,account.oauthClientId);QCOMPARE(restored.accounts().first().oauthTenant,account.oauthTenant);
 }
 void multiAccountPersistenceNeverContainsSecrets(){
  QTemporaryDir dir;auto secrets=std::make_shared<MockSecrets>();
  MailAccounts store(dir.filePath("accounts.json"),secrets);QString error;QVERIFY(store.load(&error));
  auto a=mailAccountFromTemplate("qq","alice@qq.com");auto b=mailAccountFromTemplate("163","bob@163.com");
  QVERIFY(a.id!=b.id);QVERIFY(store.upsert(a,&error));QVERIFY(store.upsert(b,&error));
  const QByteArray secret="EXAMPLE-not-a-real-credential-5f3a";QVERIFY(store.saveCredential(a.id,secret,&error));
  QFile f(dir.filePath("accounts.json"));QVERIFY(f.open(QIODevice::ReadOnly));auto bytes=f.readAll();f.close();
  QVERIFY(!bytes.contains(secret));QVERIFY(!bytes.contains("credential"));QVERIFY(!bytes.contains("token"));
  MailAccounts restored(dir.filePath("accounts.json"),secrets);QVERIFY(restored.load(&error));QCOMPARE(restored.accounts().size(),2);
  QByteArray result;QVERIFY(restored.loadCredential(a.id,&result,&error));QCOMPARE(result,secret);
  QVERIFY(restored.remove(a.id,&error));QCOMPARE(restored.accounts().size(),1);QVERIFY(!secrets->data.contains(a.id));
 }
 void malformedSecretFieldsAndFailedWritesPreserveState(){
  QTemporaryDir dir;auto secrets=std::make_shared<MockSecrets>();
  const auto path=dir.filePath("accounts.json");MailAccounts store(path,secrets);auto a=mailAccountFromTemplate("qq","a@qq.com");
  QVERIFY(store.upsert(a));QFile f(path);QVERIFY(f.open(QIODevice::ReadOnly));auto root=QJsonDocument::fromJson(f.readAll()).object();f.close();
  auto entries=root["accounts"].toArray();auto entry=entries[0].toObject();entry["password"]="must-not-load";entries[0]=entry;root["accounts"]=entries;
  QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(root).toJson());f.close();
  QString error;QVERIFY(!store.load(&error));QCOMPARE(store.accounts().size(),1);
  secrets->failWrites=true;QVERIFY(!store.saveCredential(a.id,"fake",&error));QVERIFY(!store.remove(a.id,&error));
  QCOMPARE(store.accounts().size(),1);
  MailAccounts invalidPath(dir.filePath("missing/config.json"),secrets);QVERIFY(!invalidPath.upsert(a,&error));QVERIFY(invalidPath.accounts().isEmpty());
 }
 void platformFailsClosedWithoutSupportedBackend(){
#ifndef Q_OS_WIN
  QTemporaryDir dir;MailAccounts store(dir.filePath("accounts.json"));auto a=mailAccountFromTemplate("qq","a@qq.com");
  QVERIFY(store.upsert(a));QVERIFY(!store.credentialStorageAvailable());
  QString error;QVERIFY(!store.saveCredential(a.id,"fake",&error));QVERIFY(!error.isEmpty());
  QByteArray value="old-secret";QVERIFY(!store.loadCredential(a.id,&value,&error));QVERIFY(value.isEmpty());
#else
  QSKIP("Do not write real Windows credential store during unit tests; use mock");
#endif
 }
};
QTEST_MAIN(MailAccountTests)
#include "test_mailaccounts.moc"
