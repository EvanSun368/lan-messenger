#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <memory>

enum class MailTls { ImplicitTls, StartTls };
enum class MailAuth { Password, OAuth2 };
struct MailEndpoint {
    QString host;
    int port=0;
    MailTls tls=MailTls::ImplicitTls;
};
struct MailAccount {
    QString id,label,address,provider,userName;
    MailEndpoint imap,smtp;
    MailAuth auth=MailAuth::Password;
    // Public native-app registration only. Secrets belong in MailCredentialStore.
    QString oauthClientId,oauthTenant="common";
};
struct MailProviderTemplate {
    QString id,label,notice;
    MailEndpoint imap,smtp;
    MailAuth auth=MailAuth::Password;
    bool available=false;
};
QList<MailProviderTemplate> mailProviderTemplates();
MailAccount mailAccountFromTemplate(const QString &providerId,const QString &address);
QStringList validateMailAccount(const MailAccount &account);

// Secrets must never be written to the account JSON. A backend failure is
// explicit; callers must not fall back to a plaintext password field.
class MailCredentialStore {
public:
    virtual ~MailCredentialStore()=default;
    virtual bool save(const QString &accountId,const QByteArray &secret,QString *error)=0;
    virtual bool load(const QString &accountId,QByteArray *secret,QString *error)=0;
    virtual bool remove(const QString &accountId,QString *error)=0;
    virtual bool available() const=0;
    virtual QString unavailableReason() const=0;
};
class PlatformMailCredentialStore final : public MailCredentialStore {
public:
    bool save(const QString&,const QByteArray&,QString*) override;
    bool load(const QString&,QByteArray*,QString*) override;
    bool remove(const QString&,QString*) override;
    bool available() const override;
    QString unavailableReason() const override;
};

class MailAccounts {
public:
    explicit MailAccounts(QString configPath,std::shared_ptr<MailCredentialStore> credentials={});
    bool load(QString *error=nullptr);
    QList<MailAccount> accounts() const {return accounts_;}
    bool upsert(const MailAccount&,QString *error=nullptr);
    // Removes the secret first. If that fails, keep the account visible so the
    // user can retry rather than orphaning an unknown credential.
    bool remove(const QString &id,QString *error=nullptr);
    bool saveCredential(const QString &id,const QByteArray &secret,QString *error=nullptr);
    bool loadCredential(const QString &id,QByteArray *secret,QString *error=nullptr);
    bool credentialStorageAvailable() const {return credentials_->available();}
    QString credentialStorageProblem() const {return credentials_->unavailableReason();}
private:
    bool write(const QList<MailAccount>&,QString*);
    bool contains(const QString&) const;
    QString path_;
    QList<MailAccount> accounts_;
    std::shared_ptr<MailCredentialStore> credentials_;
};
