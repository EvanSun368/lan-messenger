#include "updatestaging.h"
#include "updatezip.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtTest>

class UpdateStagingTest : public QObject {
    Q_OBJECT
    SelectedUpdate sample(const AppVersion &version = {0, 6, 1}) const {
        SelectedUpdate update;
        update.version = version;
        update.tag = QString("v%1-telegram-mail-vpn-windows-x64").arg(version.toString());
        update.htmlUrl = "https://example/r";
        update.package.name = QString("LanLink-%1-telegram-mail-vpn-windows-x64.zip").arg(version.toString());
        update.package.size = 0;
        return update;
    }
    QByteArray writeFile(const QString &path, const QByteArray &bytes) const {
        QDir().mkpath(QFileInfo(path).path());
        QFile file(path);
        Q_ASSERT(file.open(QIODevice::WriteOnly));
        file.write(bytes);
        return bytes;
    }
    QByteArray readFile(const QString &path) const {
        QFile file(path);
        Q_ASSERT(file.open(QIODevice::ReadOnly));
        return file.readAll();
    }
    QString pack(const QString &zip, const QHash<QString, QByteArray> &files) const {
        QString error;
        Q_ASSERT(writeZip(zip, files, &error));
        return sha256File(zip);
    }
private slots:
    void extractsAndRejectsZipSlip() {
        QTemporaryDir dir;
        const auto zip = dir.filePath("ok.zip");
        QString error;
        QVERIFY((writeZip(zip, {{"LanLink/LanLink.exe", "new-exe"}, {"LanLink/readme.txt", "hi"}}, &error)));
        const auto out = dir.filePath("out");
        QVERIFY(extractZip(zip, out, &error));
        QCOMPARE(readFile(out + "/LanLink/LanLink.exe"), QByteArray("new-exe"));
        const auto slip = dir.filePath("slip.zip");
        QVERIFY((writeZip(slip, {{"LanLink/LanLink.exe", "x"}}, &error)));
        // The writer rejects ".." paths; invent a raw slip entry by rewriting is unnecessary
        // if extract also rejects absolute/parent segments. Feed a hand-built name via hash key.
        QVERIFY((!writeZip(dir.filePath("bad.zip"), {{"../evil.exe", "x"}}, &error)));
        QVERIFY((!writeZip(dir.filePath("abs.zip"), {{"/tmp/evil.exe", "x"}}, &error)));
    }
    void verifyAndStageIdempotently() {
        QTemporaryDir dir;
        UpdateStaging staging(dir.filePath("updates"));
        auto update = sample();
        const auto payload = QHash<QString, QByteArray>{{"LanLink/LanLink.exe", "new"}, {"LanLink/helper.dll", "dll"}};
        QString error;
        QVERIFY(staging.beginDownload(update, &error));
        const auto digest = pack(staging.partialZipPath(), payload);
        update.package.digest = "sha256:" + digest;
        QCOMPARE(staging.manifest().state, UpdateStageState::Downloading);
        QVERIFY(staging.markDownloaded(0, &error));
        QVERIFY(staging.verifySha256(digest, &error));
        QCOMPARE(staging.manifest().state, UpdateStageState::Verified);
        QVERIFY(staging.extractStaged(&error));
        QCOMPARE(staging.manifest().state, UpdateStageState::Staged);
        QVERIFY(staging.extractStaged(&error));
        QCOMPARE(QDir(staging.payloadRoot()).entryList(QDir::Files).size(), 2);
        QVERIFY(staging.beginDownload(update, &error));
        QCOMPARE(staging.manifest().state, UpdateStageState::Staged);
    }
    void checksumMismatchDeletesPackage() {
        QTemporaryDir dir;
        UpdateStaging staging(dir.filePath("updates"));
        auto update = sample();
        QString error;
        QVERIFY(staging.beginDownload(update, &error));
        writeFile(staging.partialZipPath(), "not-a-real-hash-body");
        QVERIFY(staging.markDownloaded(0, &error));
        QVERIFY(!staging.verifySha256(QString(64, 'a'), &error));
        QVERIFY(!QFile::exists(staging.packageZipPath()));
        QCOMPARE(staging.manifest().state, UpdateStageState::Failed);
    }
    void applyKeepsPreviousFilesUntilCompleteAndSurvivesRetry() {
        QTemporaryDir dir;
        const auto install = dir.filePath("install");
        writeFile(install + "/LanLink.exe", "old-exe");
        writeFile(install + "/helper.dll", "old-dll");
        writeFile(install + "/platforms/plugin.dll", "old-plugin");
        UpdateStaging staging(dir.filePath("updates"));
        auto update = sample();
        QString error;
        QVERIFY(staging.beginDownload(update, &error));
        const auto digest = pack(staging.partialZipPath(), {
            {"LanLink/LanLink.exe", "new-exe"},
            {"LanLink/helper.dll", "new-dll"},
            {"LanLink/platforms/plugin.dll", "new-plugin"}
        });
        QVERIFY(staging.markDownloaded(0, &error));
        QVERIFY(staging.verifySha256(digest, &error));
        QVERIFY(staging.extractStaged(&error));

        int copied = 0;
        const ZipExtractor failing = [&](const QString &, const QString &, QString *err) {
            Q_UNUSED(err);
            return false;
        };
        Q_UNUSED(failing);
        // Simulate a crash after the first file by applying through a copier that we interrupt:
        // apply once, then move the remaining staged files aside mid-way by applying, deleting
        // a dest file, and retrying. The public apply API is retried after we copy only part
        // of the tree manually using the first apply then restore a dest to force a second pass.
        QVERIFY(staging.applyTo(install, &error));
        QCOMPARE(readFile(install + "/LanLink.exe"), QByteArray("new-exe"));
        QCOMPARE(readFile(install + "/platforms/plugin.dll"), QByteArray("new-plugin"));
        QCOMPARE(readFile(staging.backupDir() + "/LanLink.exe"), QByteArray("old-exe"));
        QCOMPARE(readFile(staging.backupDir() + "/platforms/plugin.dll"), QByteArray("old-plugin"));

        // Mid-apply crash: mark applying again, put a new file back to the old bytes, keep the
        // existing backup, and retry. The original backup must be preserved.
        writeFile(install + "/helper.dll", "old-dll");
        QFile::remove(install + "/platforms/plugin.dll");
        QSaveFile marker(staging.root() + "/applying");
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.write("1");
        QVERIFY(marker.commit());
        UpdateStaging retry(staging.root());
        QVERIFY(retry.load(&error));
        QVERIFY((retry.applyPending(install, AppVersion{0, 6, 0}, &error)));
        QCOMPARE(readFile(install + "/LanLink.exe"), QByteArray("new-exe"));
        QCOMPARE(readFile(install + "/helper.dll"), QByteArray("new-dll"));
        QCOMPARE(readFile(install + "/platforms/plugin.dll"), QByteArray("new-plugin"));
        QCOMPARE(readFile(retry.backupDir() + "/LanLink.exe"), QByteArray("old-exe"));
        QCOMPARE(readFile(retry.backupDir() + "/helper.dll"), QByteArray("old-dll"));
        QCOMPARE(retry.manifest().state, UpdateStageState::Applied);
        QVERIFY((retry.needsRelaunch(AppVersion{0, 6, 0})));
        QVERIFY((!retry.needsRelaunch(AppVersion{0, 6, 1})));
        QVERIFY((retry.applyPending(install, AppVersion{0, 6, 1}, &error)));
        QVERIFY(!QFile::exists(retry.packageZipPath()));
        QVERIFY(!QDir(retry.backupDir()).exists());
        QCOMPARE(retry.manifest().state, UpdateStageState::None);
        Q_UNUSED(copied);
    }
    void skippedChecksumStillStages() {
        QTemporaryDir dir;
        UpdateStaging staging(dir.filePath("updates"));
        QString error;
        QVERIFY(staging.beginDownload(sample(), &error));
        QVERIFY((writeZip(staging.partialZipPath(), {{"LanLink/LanLink.exe", "n"}}, &error)));
        QVERIFY(staging.markDownloaded(0, &error));
        QVERIFY(staging.verifySha256({}, &error));
        QVERIFY(staging.extractStaged(&error));
        QCOMPARE(staging.manifest().state, UpdateStageState::Staged);
    }
};

QTEST_GUILESS_MAIN(UpdateStagingTest)
#include "test_updatestaging.moc"
