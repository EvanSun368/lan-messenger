#include "updateservice.h"
#include "updateapply.h"
#include "updatezip.h"
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>

namespace {
void wipe(QByteArray &bytes) { bytes.fill('\0'); bytes.clear(); }

QString httpFailure(QNetworkReply *reply) {
    if (!reply) return QStringLiteral("检查更新失败");
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 401 || status == 403)
        return QStringLiteral("GitHub 访问令牌无效或没有读取私有发布的权限。");
    if (status == 404)
        return QStringLiteral("无法读取私有仓库的发布列表。请配置有权限的 GitHub 访问令牌，或打开发布页面手动下载。");
    return QStringLiteral("检查更新失败（HTTP %1）").arg(status ? status : int(reply->error()));
}
}

UpdateService::UpdateService(QString storage, AppVersion current, std::shared_ptr<UpdateCredentialStore> credentials,
                             QObject *parent)
    : QObject(parent), storage_(std::move(storage)), current_(current), channel_(currentUpdateChannel()),
      staging_(updateStagingRoot(storage_)),
      credentials_(credentials ? std::move(credentials) : platformUpdateCredentials()),
      network_(new QNetworkAccessManager(this)) {
    staging_.load();
}

bool UpdateService::tokenAvailable() const { return credentials_ && credentials_->available(); }
bool UpdateService::hasToken() const { return credentials_ && credentials_->hasToken(); }
QString UpdateService::tokenProblem() const {
    return credentials_ ? credentials_->unavailableReason() : QStringLiteral("凭据存储不可用");
}
bool UpdateService::saveToken(const QByteArray &token, QString *error) {
    return credentials_ && credentials_->save(token, error);
}
bool UpdateService::clearToken(QString *error) { return credentials_ && credentials_->remove(error); }

QString UpdateService::missingTokenMessage() {
    return QStringLiteral("仓库是私有的，检查更新需要 GitHub 访问令牌。未配置令牌时不会向 GitHub 发送匿名请求。");
}

QByteArray UpdateService::loadToken() const {
    QByteArray token;
    QString ignored;
    if (credentials_) credentials_->load(&token, &ignored);
    return token;
}

QNetworkRequest UpdateService::apiRequest(const QUrl &url, const QByteArray &accept, const QByteArray &token) const {
    QNetworkRequest request(url);
    request.setRawHeader("Accept", accept);
    request.setRawHeader("User-Agent", "LanLink-Updater");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    if (!token.isEmpty()) request.setRawHeader("Authorization", QByteArray("Bearer ") + token);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(60 * 1000);
    return request;
}

void UpdateService::check(bool interactive) {
    if (busy()) {
        emit statusMessage(QStringLiteral("正在检查或下载更新…"));
        return;
    }
    interactive_ = interactive;
    if (!hasToken()) {
        emit checkFailed(missingTokenMessage(), githubReleasesPageUrl(), interactive_);
        return;
    }
    auto token = loadToken();
    job_ = Job::List;
    reply_ = network_->get(apiRequest(QUrl(githubReleasesApiUrl()), "application/vnd.github+json", token));
    wipe(token);
    connect(reply_, &QNetworkReply::finished, this, &UpdateService::listFinished);
    emit statusMessage(QStringLiteral("正在检查更新…"));
}

void UpdateService::listFinished() {
    QPointer<QNetworkReply> reply = reply_;
    reply_ = nullptr;
    job_ = Job::Idle;
    if (!reply) return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 0) {
        emit checkFailed(QStringLiteral("无法连接 GitHub 检查更新。"), githubReleasesPageUrl(), interactive_);
        return;
    }
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != 200) {
        emit checkFailed(httpFailure(reply), githubReleasesPageUrl(), interactive_);
        return;
    }
    QString parseError;
    const auto releases = parseReleasesJson(reply->readAll(), &parseError);
    if (!releases) {
        emit checkFailed(parseError.isEmpty() ? QStringLiteral("GitHub 发布列表格式无效") : parseError,
                         githubReleasesPageUrl(), interactive_);
        return;
    }
    const auto selected = selectLatestUpdate(*releases, current_, channel_);
    if (!selected) {
        emit upToDate(interactive_);
        return;
    }
    pending_ = *selected;
    emit updateAvailable(pending_, interactive_);
}

void UpdateService::download(const SelectedUpdate &update, bool interactive) {
    if (busy()) {
        emit statusMessage(QStringLiteral("正在检查或下载更新…"));
        return;
    }
    interactive_ = interactive;
    pending_ = update;
    QString error;
    if (!staging_.beginDownload(update, &error)) {
        emit downloadFailed(error, interactive_);
        return;
    }
    if (staging_.manifest().state == UpdateStageState::Staged) {
        emit stageReady(pending_, interactive_);
        return;
    }
    if (staging_.manifest().state == UpdateStageState::Downloaded
        || staging_.manifest().state == UpdateStageState::Verified) {
        stagePackage();
        return;
    }
    if (pending_.package.apiUrl.isEmpty()) {
        emit downloadFailed(QStringLiteral("发布包缺少下载地址"), interactive_);
        return;
    }
    auto token = loadToken();
    if (token.isEmpty()) {
        emit downloadFailed(missingTokenMessage(), interactive_);
        return;
    }
    QFile::remove(staging_.partialZipPath());
    downloadFile_ = new QFile(staging_.partialZipPath(), this);
    if (!downloadFile_->open(QIODevice::WriteOnly)) {
        wipe(token);
        emit downloadFailed(QStringLiteral("无法写入下载的更新包"), interactive_);
        return;
    }
    job_ = Job::Package;
    followDownloadRedirect(QUrl(pending_.package.apiUrl), token);
    wipe(token);
}

void UpdateService::followDownloadRedirect(const QUrl &url, const QByteArray &token) {
    const bool apiHost = url.host().compare(QLatin1String("api.github.com"), Qt::CaseInsensitive) == 0;
    const auto request = apiRequest(url, "application/octet-stream", apiHost ? token : QByteArray());
    reply_ = network_->get(request);
    connect(reply_, &QNetworkReply::readyRead, this, [this] {
        if (downloadFile_ && reply_) downloadFile_->write(reply_->readAll());
    });
    connect(reply_, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0)
            emit statusMessage(QStringLiteral("正在下载更新… %1 / %2 MB")
                                   .arg(received / (1024.0 * 1024.0), 0, 'f', 1)
                                   .arg(total / (1024.0 * 1024.0), 0, 'f', 1));
    });
    connect(reply_, &QNetworkReply::finished, this, &UpdateService::packageFinished);
    emit statusMessage(QStringLiteral("正在下载更新…"));
}

void UpdateService::packageFinished() {
    QPointer<QNetworkReply> reply = reply_;
    reply_ = nullptr;
    if (!reply) return;
    reply->deleteLater();
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
        const auto location = reply->header(QNetworkRequest::LocationHeader).toUrl();
        if (!location.isValid() || location.scheme() != QLatin1String("https")) {
            finishDownload(QStringLiteral("更新包下载重定向无效"));
            return;
        }
        auto token = loadToken();
        followDownloadRedirect(location, token);
        wipe(token);
        return;
    }
    if (downloadFile_) {
        if (reply->bytesAvailable()) downloadFile_->write(reply->readAll());
        downloadFile_->close();
        downloadFile_->deleteLater();
        downloadFile_ = nullptr;
    }
    job_ = Job::Idle;
    if (status != 200) {
        QFile::remove(staging_.partialZipPath());
        finishDownload(httpFailure(reply));
        return;
    }
    QString error;
    if (!staging_.markDownloaded(pending_.package.size, &error)) {
        finishDownload(error);
        return;
    }
    const auto digest = digestSha256(pending_.package.digest);
    if (!digest.isEmpty() || pending_.checksum.apiUrl.isEmpty()) {
        if (!staging_.verifySha256(digest, &error)) {
            finishDownload(error);
            return;
        }
        stagePackage();
        return;
    }
    auto token = loadToken();
    job_ = Job::Checksum;
    checksumBody_.clear();
    reply_ = network_->get(apiRequest(QUrl(pending_.checksum.apiUrl), "application/octet-stream", token));
    wipe(token);
    connect(reply_, &QNetworkReply::finished, this, &UpdateService::checksumFinished);
}

void UpdateService::checksumFinished() {
    QPointer<QNetworkReply> reply = reply_;
    reply_ = nullptr;
    job_ = Job::Idle;
    if (!reply) return;
    reply->deleteLater();
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
        const auto location = reply->header(QNetworkRequest::LocationHeader).toUrl();
        if (!location.isValid() || location.scheme() != QLatin1String("https")) {
            finishDownload(QStringLiteral("校验和下载重定向无效"));
            return;
        }
        auto token = loadToken();
        const bool apiHost = location.host().compare(QLatin1String("api.github.com"), Qt::CaseInsensitive) == 0;
        job_ = Job::Checksum;
        reply_ = network_->get(apiRequest(location, "application/octet-stream", apiHost ? token : QByteArray()));
        wipe(token);
        connect(reply_, &QNetworkReply::finished, this, &UpdateService::checksumFinished);
        return;
    }
    if (status != 200) {
        finishDownload(QStringLiteral("无法获取更新包校验和"));
        return;
    }
    checksumBody_ = reply->readAll();
    const auto expected = checksumForAsset(QString::fromUtf8(checksumBody_), pending_.package.name);
    checksumBody_.fill('\0');
    checksumBody_.clear();
    QString error;
    if (!staging_.verifySha256(expected, &error)) {
        finishDownload(error.isEmpty() ? QStringLiteral("更新包校验失败") : error);
        return;
    }
    stagePackage();
}

void UpdateService::stagePackage() {
    QString error;
    if (!staging_.extractStaged(&error)) {
        finishDownload(error);
        return;
    }
    emit stageReady(pending_, interactive_);
}

void UpdateService::finishDownload(const QString &message) {
    job_ = Job::Idle;
    emit downloadFailed(message, interactive_);
}

