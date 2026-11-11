#include "updatestaging.h"
#include "updatezip.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QScopeGuard>

namespace {
bool fail(QString *error, const QString &text) {
    if (error) *error = text;
    return false;
}

QString join(const QString &dir, const QString &name) { return QDir(dir).filePath(name); }

bool ensureDir(const QString &path, QString *error) {
    if (QDir().mkpath(path)) return true;
    return fail(error, QStringLiteral("无法创建更新目录"));
}

bool removePath(const QString &path) {
    QFileInfo info(path);
    if (!info.exists()) return true;
    if (info.isDir()) return QDir(path).removeRecursively();
    return QFile::remove(path);
}

bool atomicWrite(const QString &path, const QByteArray &bytes, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return fail(error, QStringLiteral("无法写入更新状态"));
    return true;
}

UpdateStageState stateFromName(const QString &name) {
    if (name == QLatin1String("downloading")) return UpdateStageState::Downloading;
    if (name == QLatin1String("downloaded")) return UpdateStageState::Downloaded;
    if (name == QLatin1String("verified")) return UpdateStageState::Verified;
    if (name == QLatin1String("staged")) return UpdateStageState::Staged;
    if (name == QLatin1String("applying")) return UpdateStageState::Applying;
    if (name == QLatin1String("applied")) return UpdateStageState::Applied;
    if (name == QLatin1String("failed")) return UpdateStageState::Failed;
    return UpdateStageState::None;
}

bool sameUpdate(const UpdateManifest &manifest, const SelectedUpdate &update) {
    return manifest.tag == update.tag && manifest.packageName == update.package.name
        && manifest.version == update.version;
}
}

QString UpdateStaging::stateName(UpdateStageState state) {
    switch (state) {
    case UpdateStageState::Downloading: return QStringLiteral("downloading");
    case UpdateStageState::Downloaded: return QStringLiteral("downloaded");
    case UpdateStageState::Verified: return QStringLiteral("verified");
    case UpdateStageState::Staged: return QStringLiteral("staged");
    case UpdateStageState::Applying: return QStringLiteral("applying");
    case UpdateStageState::Applied: return QStringLiteral("applied");
    case UpdateStageState::Failed: return QStringLiteral("failed");
    case UpdateStageState::None: return QStringLiteral("none");
    }
    return QStringLiteral("none");
}

UpdateStaging::UpdateStaging(QString root) : root_(std::move(root)) {}

QString UpdateStaging::partialZipPath() const { return join(root_, QStringLiteral("partial.zip")); }
QString UpdateStaging::packageZipPath() const { return join(root_, QStringLiteral("package.zip")); }
QString UpdateStaging::stagedDir() const { return join(root_, QStringLiteral("staged")); }
QString UpdateStaging::backupDir() const { return join(root_, QStringLiteral("backup")); }
QString UpdateStaging::manifestPath() const { return join(root_, QStringLiteral("state.json")); }
QString UpdateStaging::applyMarkerPath() const { return join(root_, QStringLiteral("applying")); }
QString UpdateStaging::readyMarkerPath() const { return join(root_, QStringLiteral("staged.ready")); }

bool UpdateStaging::stagedReady() const {
    return QFileInfo::exists(readyMarkerPath()) && QDir(stagedDir()).exists();
}

bool UpdateStaging::applying() const { return QFileInfo::exists(applyMarkerPath()); }

bool UpdateStaging::load(QString *error) {
    if (error) error->clear();
    manifest_ = {};
    QFile file(manifestPath());
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly)) return fail(error, QStringLiteral("无法读取更新状态"));
    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) return fail(error, QStringLiteral("更新状态文件损坏"));
    const auto object = document.object();
    if (const auto version = parseAppVersion(object["version"].toString())) manifest_.version = *version;
    manifest_.tag = object["tag"].toString();
    manifest_.packageName = object["packageName"].toString();
    manifest_.sha256 = object["sha256"].toString();
    manifest_.htmlUrl = object["htmlUrl"].toString();
    manifest_.state = stateFromName(object["state"].toString());
    if (applying() && manifest_.state == UpdateStageState::Staged)
        manifest_.state = UpdateStageState::Applying;
    return true;
}

bool UpdateStaging::writeManifest(UpdateStageState state, QString *error) {
    if (!ensureDir(root_, error)) return false;
    manifest_.state = state;
    QJsonObject object;
    object.insert(QStringLiteral("version"), manifest_.version.toString());
    object.insert(QStringLiteral("tag"), manifest_.tag);
    object.insert(QStringLiteral("packageName"), manifest_.packageName);
    object.insert(QStringLiteral("sha256"), manifest_.sha256);
    object.insert(QStringLiteral("htmlUrl"), manifest_.htmlUrl);
    object.insert(QStringLiteral("state"), stateName(state));
    return atomicWrite(manifestPath(), QJsonDocument(object).toJson(), error);
}

bool UpdateStaging::writeApplying(bool on, QString *error) {
    if (!on) return QFile::remove(applyMarkerPath()) || !QFileInfo::exists(applyMarkerPath())
            || fail(error, QStringLiteral("无法清除安装标记"));
    return atomicWrite(applyMarkerPath(), QByteArrayLiteral("1"), error);
}

bool UpdateStaging::beginDownload(const SelectedUpdate &update, QString *error) {
    if (error) error->clear();
    if (!load(error)) return false;
    if (sameUpdate(manifest_, update) && (manifest_.state == UpdateStageState::Staged || manifest_.state == UpdateStageState::Applied) && stagedReady())
        return true;
    if (sameUpdate(manifest_, update) && (manifest_.state == UpdateStageState::Downloaded || manifest_.state == UpdateStageState::Verified)
        && QFileInfo::exists(packageZipPath()))
        return true;
    if (!ensureDir(root_, error)) return false;
    if (!sameUpdate(manifest_, update)) {
        removePath(partialZipPath());
        removePath(packageZipPath());
        removePath(stagedDir());
        removePath(readyMarkerPath());
        removePath(applyMarkerPath());
    }
    manifest_.version = update.version;
    manifest_.tag = update.tag;
    manifest_.packageName = update.package.name;
    manifest_.sha256 = digestSha256(update.package.digest);
    manifest_.htmlUrl = update.htmlUrl;
    return writeManifest(UpdateStageState::Downloading, error);
}

bool UpdateStaging::markDownloaded(qint64 expectedSize, QString *error) {
    if (error) error->clear();
    QFileInfo partial(partialZipPath());
    if (!partial.exists()) return fail(error, QStringLiteral("更新包尚未下载完成"));
    if (expectedSize > 0 && partial.size() != expectedSize)
        return fail(error, QStringLiteral("更新包大小与发布信息不一致"));
    removePath(packageZipPath());
    if (!QFile::rename(partialZipPath(), packageZipPath()))
        return fail(error, QStringLiteral("无法保存已下载的更新包"));
    return writeManifest(UpdateStageState::Downloaded, error);
}

bool UpdateStaging::verifySha256(const QString &expectedHex, QString *error) {
    if (error) error->clear();
    if (!QFileInfo::exists(packageZipPath())) return fail(error, QStringLiteral("没有可校验的更新包"));
    if (expectedHex.isEmpty()) {
        manifest_.sha256.clear();
        return writeManifest(UpdateStageState::Verified, error);
    }
    const auto actual = sha256File(packageZipPath(), error);
    if (actual.isEmpty()) return false;
    if (actual.compare(expectedHex, Qt::CaseInsensitive) != 0) {
        removePath(packageZipPath());
        writeManifest(UpdateStageState::Failed, error);
        return fail(error, QStringLiteral("更新包校验失败，已删除不完整下载"));
    }
    manifest_.sha256 = actual.toLower();
    return writeManifest(UpdateStageState::Verified, error);
}

bool UpdateStaging::extractStaged(QString *error, ZipExtractor extractor) {
    if (error) error->clear();
    if (manifest_.state != UpdateStageState::Downloaded && manifest_.state != UpdateStageState::Verified
        && manifest_.state != UpdateStageState::Staged)
        return fail(error, QStringLiteral("更新包尚未准备好解压"));
    if (stagedReady() && manifest_.state == UpdateStageState::Staged) return true;
    if (!QFileInfo::exists(packageZipPath())) return fail(error, QStringLiteral("没有可解压的更新包"));
    const auto temp = join(root_, QStringLiteral("staged.tmp"));
    removePath(temp);
    if (!ensureDir(temp, error)) return false;
    const ZipExtractor extract = extractor ? extractor : extractZip;
    if (!extract(packageZipPath(), temp, error)) {
        removePath(temp);
        return false;
    }
    removePath(stagedDir());
    removePath(readyMarkerPath());
    if (!QDir().rename(temp, stagedDir())) {
        removePath(temp);
        return fail(error, QStringLiteral("无法保存已解压的更新"));
    }
    if (!atomicWrite(readyMarkerPath(), manifest_.version.toString().toUtf8(), error)) return false;
    return writeManifest(UpdateStageState::Staged, error);
}

namespace {
bool containsPayloadExe(const QDir &dir) {
    const auto entries = dir.entryInfoList({QStringLiteral("LanLink.exe"), QStringLiteral("LanLink")}, QDir::Files);
    return !entries.isEmpty();
}
}

QString UpdateStaging::payloadRoot() const {
    QDir dir(stagedDir());
    if (containsPayloadExe(dir)) return stagedDir();
    const auto entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (entries.size() == 1) {
        const auto nested = dir.filePath(entries.front());
        if (containsPayloadExe(QDir(nested))) return nested;
    }
    return stagedDir();
}

bool UpdateStaging::copyTree(const QString &from, const QString &to, const QString &installDir, bool backupExisting, QString *error) {
    QDir source(from);
    const auto files = source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    if (!ensureDir(to, error)) return false;
    for (const auto &entry : files) {
        const auto dest = join(to, entry.fileName());
        if (entry.isDir()) {
            if (!copyTree(entry.absoluteFilePath(), dest, installDir, backupExisting, error)) return false;
            continue;
        }
        if (backupExisting && QFileInfo::exists(dest)) {
            const auto backup = join(backupDir(), QDir(installDir).relativeFilePath(dest));
            // A retry must not overwrite an original backup with a half-applied file.
            if (!QFileInfo::exists(backup)) {
                if (!ensureDir(QFileInfo(backup).path(), error)) return false;
                if (!QFile::rename(dest, backup)) return fail(error, QStringLiteral("无法备份当前程序文件"));
            } else if (!QFile::remove(dest) && QFileInfo::exists(dest)) {
                // The running executable may only allow a rename. Try that next.
                const auto discarded = dest + QStringLiteral(".old");
                QFile::remove(discarded);
                if (!QFile::rename(dest, discarded)) return fail(error, QStringLiteral("无法替换正在使用的程序文件"));
            }
        }
        if (!QFile::copy(entry.absoluteFilePath(), dest))
            return fail(error, QStringLiteral("无法安装更新文件"));
    }
    return true;
}

bool UpdateStaging::restoreBackup(const QString &installDir, QString *error) {
    if (!QDir(backupDir()).exists()) return fail(error, QStringLiteral("没有可回滚的备份"));
    return copyTree(backupDir(), installDir, installDir, false, error);
}

bool UpdateStaging::applyTo(const QString &installDir, QString *error) {
    if (error) error->clear();
    if (!stagedReady()) return fail(error, QStringLiteral("没有已校验的待安装更新"));
    if (installDir.isEmpty() || QFileInfo(installDir) == QFileInfo(stagedDir()))
        return fail(error, QStringLiteral("安装目录无效"));
    if (!writeApplying(true, error)) return false;
    const auto source = payloadRoot();
    if (!copyTree(source, installDir, installDir, true, error)) return false;
    if (!writeApplying(false, error)) return false;
    return writeManifest(UpdateStageState::Applied, error);
}

bool UpdateStaging::needsRelaunch(const AppVersion &running) const {
    return manifest_.state == UpdateStageState::Applied && running < manifest_.version;
}

bool UpdateStaging::applyPending(const QString &installDir, const AppVersion &running, QString *error) {
    if (error) error->clear();
    if (!load(error)) return false;
    if (stagedReady() && (applying() || running < manifest_.version
                          || manifest_.state == UpdateStageState::Staged
                          || manifest_.state == UpdateStageState::Applying)) {
        if (!applyTo(installDir, error)) return false;
    } else if (applying() && !stagedReady()) {
        return restoreBackup(installDir, error);
    }
    if (manifest_.state == UpdateStageState::Applied && running >= manifest_.version)
        cleanupApplied();
    return true;
}

void UpdateStaging::cleanupApplied() {
    removePath(partialZipPath());
    removePath(packageZipPath());
    removePath(stagedDir());
    removePath(readyMarkerPath());
    removePath(applyMarkerPath());
    removePath(backupDir());
    removePath(join(root_, QStringLiteral("staged.tmp")));
    QFile::remove(manifestPath());
    manifest_ = {};
}
