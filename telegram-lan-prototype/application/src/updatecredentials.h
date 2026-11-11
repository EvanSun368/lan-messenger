#pragma once
#include <QByteArray>
#include <QString>
#include <memory>

// GitHub tokens use the same Windows Credential Manager service as mail
// secrets, under target "LanLink/Updates/github". Linux/macOS fail closed.
class UpdateCredentialStore {
public:
    virtual ~UpdateCredentialStore() = default;
    virtual bool available() const = 0;
    virtual QString unavailableReason() const = 0;
    virtual bool hasToken() const = 0;
    virtual bool save(const QByteArray &token, QString *error) = 0;
    virtual bool load(QByteArray *token, QString *error) const = 0;
    virtual bool remove(QString *error) = 0;
};

class PlatformUpdateCredentialStore final : public UpdateCredentialStore {
public:
    bool available() const override;
    QString unavailableReason() const override;
    bool hasToken() const override;
    bool save(const QByteArray &token, QString *error) override;
    bool load(QByteArray *token, QString *error) const override;
    bool remove(QString *error) override;
};

std::shared_ptr<UpdateCredentialStore> platformUpdateCredentials();
bool tokenLooksPlausible(const QByteArray &token);
