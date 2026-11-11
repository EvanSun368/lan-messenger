#include "updatecredentials.h"
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#endif

namespace {
constexpr auto Target = L"LanLink/Updates/github";
bool fail(QString *error, const QString &text) {
    if (error) *error = text;
    return false;
}
QString unavailable() {
    return QStringLiteral("当前平台尚未接入系统凭据服务；不会以明文保存 GitHub 访问令牌。");
}
}

bool tokenLooksPlausible(const QByteArray &token) {
    if (token.size() < 20 || token.size() > 200) return false;
    for (unsigned char ch : token)
        if (ch < 32 || ch > 126) return false;
    return true;
}

bool PlatformUpdateCredentialStore::available() const {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

QString PlatformUpdateCredentialStore::unavailableReason() const {
    return available() ? QString() : unavailable();
}

bool PlatformUpdateCredentialStore::hasToken() const {
    QByteArray token;
    QString ignored;
    const bool ok = load(&token, &ignored);
    token.fill('\0');
    return ok;
}

bool PlatformUpdateCredentialStore::save(const QByteArray &token, QString *error) {
    if (error) error->clear();
    if (!tokenLooksPlausible(token)) return fail(error, QStringLiteral("GitHub 访问令牌无效"));
#ifdef Q_OS_WIN
    if (token.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) return fail(error, QStringLiteral("凭据超过系统大小限制"));
    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(Target);
    credential.CredentialBlobSize = DWORD(token.size());
    credential.CredentialBlob = LPBYTE(const_cast<char *>(token.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (CredWriteW(&credential, 0)) return true;
    return fail(error, QString("Windows凭据保存失败（错误%1）").arg(GetLastError()));
#else
    return fail(error, unavailable());
#endif
}

bool PlatformUpdateCredentialStore::load(QByteArray *token, QString *error) const {
    if (token) token->clear();
    if (error) error->clear();
    if (!token) return fail(error, QStringLiteral("凭据请求无效"));
#ifdef Q_OS_WIN
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(Target, CRED_TYPE_GENERIC, 0, &credential))
        return fail(error, QString("Windows凭据读取失败（错误%1）").arg(GetLastError()));
    *token = QByteArray(reinterpret_cast<const char *>(credential->CredentialBlob), int(credential->CredentialBlobSize));
    CredFree(credential);
    return true;
#else
    return fail(error, unavailable());
#endif
}

bool PlatformUpdateCredentialStore::remove(QString *error) {
    if (error) error->clear();
#ifdef Q_OS_WIN
    if (CredDeleteW(Target, CRED_TYPE_GENERIC, 0)) return true;
    const auto code = GetLastError();
    if (code == ERROR_NOT_FOUND) return true;
    return fail(error, QString("Windows凭据删除失败（错误%1）").arg(code));
#else
    return fail(error, unavailable());
#endif
}

std::shared_ptr<UpdateCredentialStore> platformUpdateCredentials() {
    return std::make_shared<PlatformUpdateCredentialStore>();
}
