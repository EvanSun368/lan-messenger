#include "updaterelease.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

UpdateChannel currentUpdateChannel() {
    UpdateChannel channel;
#ifdef Q_OS_WIN
    channel.platform = QStringLiteral("windows-x64");
#else
    channel.platform = QStringLiteral("linux-x64");
#endif
    return channel;
}

bool assetMatchesChannel(const QString &name, const UpdateChannel &channel) {
    const auto lower = name.toLower();
    if (!lower.endsWith(QLatin1String(".zip"))) return false;
    if (channel.family.isEmpty() || channel.platform.isEmpty()) return false;
    return lower.contains(channel.family.toLower()) && lower.contains(channel.platform.toLower());
}

QString digestSha256(const QString &digest) {
    auto hex = digest.trimmed();
    static const QRegularExpression prefix(QStringLiteral("^(?i)sha256:"));
    hex.remove(prefix);
    hex = hex.trimmed().toLower();
    static const QRegularExpression sha(QStringLiteral("^[0-9a-f]{64}$"));
    return sha.match(hex).hasMatch() ? hex : QString();
}

namespace {
QString baseName(const QString &path) {
    const auto trimmed = path.trimmed();
    const auto slash = qMax(trimmed.lastIndexOf('/'), trimmed.lastIndexOf('\\'));
    return slash >= 0 ? trimmed.mid(slash + 1) : trimmed;
}

QString checksumFromLine(const QString &line, const QString &packageName) {
    static const QRegularExpression gnu(QStringLiteral(R"(^\s*([0-9a-fA-F]{64})\s+\*?(\S+)\s*$)"));
    static const QRegularExpression bsd(QStringLiteral(R"(^\s*SHA256\s+\((.+)\)\s+=\s+([0-9a-fA-F]{64})\s*$)"));
    if (const auto match = gnu.match(line); match.hasMatch()) {
        if (baseName(match.captured(2)) == packageName) return match.captured(1).toLower();
    }
    if (const auto match = bsd.match(line); match.hasMatch()) {
        if (baseName(match.captured(1)) == packageName) return match.captured(2).toLower();
    }
    return {};
}

std::optional<ReleaseAsset> parseAsset(const QJsonValue &value) {
    if (!value.isObject()) return std::nullopt;
    const auto object = value.toObject();
    if (!object["name"].isString()) return std::nullopt;
    ReleaseAsset asset;
    asset.name = object["name"].toString();
    if (asset.name.isEmpty() || asset.name.size() > 200) return std::nullopt;
    asset.apiUrl = object["url"].toString();
    asset.browserUrl = object["browser_download_url"].toString();
    asset.digest = object["digest"].toString();
    if (object.contains("size") && !object["size"].isDouble()) return std::nullopt;
    if (object["size"].isDouble()) asset.size = qint64(object["size"].toDouble());
    if (asset.size < 0) return std::nullopt;
    return asset;
}

std::optional<GithubRelease> parseRelease(const QJsonValue &value) {
    if (!value.isObject()) return std::nullopt;
    const auto object = value.toObject();
    if (!object["tag_name"].isString()) return std::nullopt;
    GithubRelease release;
    release.tag = object["tag_name"].toString();
    if (release.tag.isEmpty() || release.tag.size() > 200) return std::nullopt;
    release.htmlUrl = object["html_url"].toString();
    release.draft = object["draft"].toBool();
    release.prerelease = object["prerelease"].toBool();
    if (!object.contains("assets")) return release;
    if (!object["assets"].isArray()) return std::nullopt;
    for (const auto &item : object["assets"].toArray())
        if (const auto asset = parseAsset(item)) release.assets.append(*asset);
    return release;
}

ReleaseAsset checksumAsset(const GithubRelease &release, const QString &packageName) {
    const auto expected = packageName + QStringLiteral(".sha256");
    for (const auto &asset : release.assets)
        if (asset.name.compare(expected, Qt::CaseInsensitive) == 0) return asset;
    for (const auto &asset : release.assets) {
        const auto lower = asset.name.toLower();
        if (lower == QLatin1String("sha256sums") || lower == QLatin1String("sha256sums.txt"))
            return asset;
    }
    return {};
}

std::optional<ReleaseAsset> matchingPackage(const GithubRelease &release, const UpdateChannel &channel) {
    std::optional<ReleaseAsset> found;
    for (const auto &asset : release.assets) {
        if (!assetMatchesChannel(asset.name, channel)) continue;
        if (found) return std::nullopt;
        found = asset;
    }
    return found;
}
}

QString checksumForAsset(const QString &fileBody, const QString &packageName) {
    QString found;
    for (const auto &raw : fileBody.split('\n')) {
        const auto hex = checksumFromLine(raw, packageName);
        if (hex.isEmpty()) continue;
        if (!found.isEmpty() && found != hex) return {};
        found = hex;
    }
    return found;
}

std::optional<QVector<GithubRelease>> parseReleasesJson(const QByteArray &json, QString *error) {
    if (error) error->clear();
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        if (error) *error = QStringLiteral("GitHub 发布列表格式无效");
        return std::nullopt;
    }
    QVector<GithubRelease> releases;
    for (const auto &item : document.array())
        if (const auto release = parseRelease(item)) releases.append(*release);
    return releases;
}

std::optional<SelectedUpdate> selectLatestUpdate(const QVector<GithubRelease> &releases,
                                                 const AppVersion &current,
                                                 const UpdateChannel &channel) {
    std::optional<SelectedUpdate> best;
    for (const auto &release : releases) {
        if (release.draft) continue;
        const auto version = parseAppVersion(release.tag);
        if (!version || *version <= current) continue;
        const auto package = matchingPackage(release, channel);
        if (!package) continue;
        if (best && *version <= best->version) continue;
        SelectedUpdate selected;
        selected.version = *version;
        selected.tag = release.tag;
        selected.htmlUrl = release.htmlUrl;
        selected.package = *package;
        selected.checksum = checksumAsset(release, package->name);
        best = selected;
    }
    return best;
}
