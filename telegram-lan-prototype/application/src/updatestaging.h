#pragma once
#include "updaterelease.h"
#include <QString>
#include <functional>
#include <optional>

enum class UpdateStageState {
    None,
    Downloading,
    Downloaded,
    Verified,
    Staged,
    Applying,
    Applied,
    Failed
};

struct UpdateManifest {
    AppVersion version;
    QString tag;
    QString packageName;
    QString sha256;
    QString htmlUrl;
    UpdateStageState state = UpdateStageState::None;
};

using ZipExtractor = std::function<bool(const QString &zipPath, const QString &destination, QString *error)>;

class UpdateStaging {
public:
    explicit UpdateStaging(QString root);
    QString root() const { return root_; }
    QString partialZipPath() const;
    QString packageZipPath() const;
    QString stagedDir() const;
    QString backupDir() const;
    UpdateManifest manifest() const { return manifest_; }

    bool load(QString *error = nullptr);
    bool beginDownload(const SelectedUpdate &update, QString *error = nullptr);
    bool markDownloaded(qint64 expectedSize, QString *error = nullptr);
    bool verifySha256(const QString &expectedHex, QString *error = nullptr);
    bool extractStaged(QString *error = nullptr, ZipExtractor extractor = {});
    bool applyTo(const QString &installDir, QString *error = nullptr);
    bool applyPending(const QString &installDir, const AppVersion &running, QString *error = nullptr);
    bool needsRelaunch(const AppVersion &running) const;
    QString payloadRoot() const;
    void cleanupApplied();
    static QString stateName(UpdateStageState);

private:
    bool writeManifest(UpdateStageState state, QString *error);
    bool writeApplying(bool on, QString *error);
    bool applying() const;
    bool stagedReady() const;
    bool copyTree(const QString &from, const QString &to, const QString &installDir, bool backupExisting, QString *error);
    bool restoreBackup(const QString &installDir, QString *error);
    QString applyMarkerPath() const;
    QString readyMarkerPath() const;
    QString manifestPath() const;
    QString root_;
    UpdateManifest manifest_;
};
