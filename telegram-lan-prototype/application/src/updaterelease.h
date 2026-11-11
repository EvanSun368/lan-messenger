#pragma once
#include "updateversion.h"
#include <QString>
#include <QVector>
#include <optional>

struct UpdateChannel {
    QString family = QStringLiteral("telegram-mail-vpn");
    QString platform;
};

struct ReleaseAsset {
    QString name;
    QString apiUrl;
    QString browserUrl;
    QString digest;
    qint64 size = 0;
};

struct GithubRelease {
    QString tag;
    QString htmlUrl;
    bool draft = false;
    bool prerelease = false;
    QVector<ReleaseAsset> assets;
};

struct SelectedUpdate {
    AppVersion version;
    QString tag;
    QString htmlUrl;
    ReleaseAsset package;
    ReleaseAsset checksum;
};

UpdateChannel currentUpdateChannel();
bool assetMatchesChannel(const QString &name, const UpdateChannel &channel);
QString digestSha256(const QString &digest);
QString checksumForAsset(const QString &fileBody, const QString &packageName);
std::optional<QVector<GithubRelease>> parseReleasesJson(const QByteArray &json, QString *error = nullptr);
std::optional<SelectedUpdate> selectLatestUpdate(const QVector<GithubRelease> &releases,
                                                 const AppVersion &current,
                                                 const UpdateChannel &channel);
