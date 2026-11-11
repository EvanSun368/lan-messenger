#pragma once
#include "updatecredentials.h"
#include "updatestaging.h"
#include <QObject>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class QFile;

class UpdateService : public QObject {
    Q_OBJECT
public:
    UpdateService(QString storage, AppVersion current, std::shared_ptr<UpdateCredentialStore> credentials,
                  QObject *parent = nullptr);
    UpdateStaging *staging() { return &staging_; }
    bool busy() const { return bool(reply_); }
    bool tokenAvailable() const;
    bool hasToken() const;
    QString tokenProblem() const;
    bool saveToken(const QByteArray &token, QString *error);
    bool clearToken(QString *error);
    void check(bool interactive);
    void download(const SelectedUpdate &update, bool interactive);
    static int startupDelayMs() { return 30 * 1000; }
    static int intervalMs() { return 24 * 60 * 60 * 1000; }
    static QString missingTokenMessage();

signals:
    void statusMessage(QString text);
    void upToDate(bool interactive);
    void updateAvailable(SelectedUpdate update, bool interactive);
    void checkFailed(QString message, QString releasesUrl, bool interactive);
    void stageReady(SelectedUpdate update, bool interactive);
    void downloadFailed(QString message, bool interactive);

private:
    QByteArray loadToken() const;
    QNetworkRequest apiRequest(const QUrl &url, const QByteArray &accept, const QByteArray &token) const;
    void finishDownload(const QString &message);
    void followDownloadRedirect(const QUrl &url, const QByteArray &token);
    void listFinished();
    void packageFinished();
    void checksumFinished();
    void stagePackage();
    QString storage_;
    AppVersion current_;
    UpdateChannel channel_;
    UpdateStaging staging_;
    std::shared_ptr<UpdateCredentialStore> credentials_;
    QNetworkAccessManager *network_ = nullptr;
    QNetworkReply *reply_ = nullptr;
    QFile *downloadFile_ = nullptr;
    SelectedUpdate pending_;
    QByteArray checksumBody_;
    bool interactive_ = false;
    enum class Job { Idle, List, Package, Checksum } job_ = Job::Idle;
};
