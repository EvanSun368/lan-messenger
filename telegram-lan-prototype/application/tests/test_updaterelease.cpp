#include "updaterelease.h"
#include <QtTest>

static UpdateChannel windowsLab() {
    return {"telegram-mail-vpn", "windows-x64"};
}

static QByteArray existingReleases() {
    return QByteArray(
        "["
        "{\"tag_name\":\"v0.5.0-windows-x64\",\"html_url\":\"https://example/r/0.5.0\",\"prerelease\":true,"
        "\"assets\":[{\"name\":\"LanLink-0.5.0-cpp-windows-x64.zip\",\"url\":\"https://api.example/a50\","
        "\"browser_download_url\":\"https://example/a50\",\"size\":10}]},"
        "{\"tag_name\":\"v0.5.1-windows-x64\",\"html_url\":\"https://example/r/0.5.1\",\"prerelease\":true,"
        "\"assets\":[{\"name\":\"LanLink-0.5.1-cpp-windows-x64.zip\",\"url\":\"https://api.example/a51\","
        "\"browser_download_url\":\"https://example/a51\",\"size\":11}]},"
        "{\"tag_name\":\"v0.6.0-telegram-mail-vpn-windows-x64\",\"html_url\":\"https://example/r/0.6.0\",\"prerelease\":true,"
        "\"assets\":[{\"name\":\"LanLink-0.6.0-telegram-mail-vpn-windows-x64.zip\",\"url\":\"https://api.example/a60\","
        "\"browser_download_url\":\"https://example/a60\","
        "\"digest\":\"sha256:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\",\"size\":12}]}"
        "]");
}

class UpdateReleaseTest : public QObject {
    Q_OBJECT
private slots:
    void matchesOnlyTheTelegramLabPackage() {
        QVERIFY(assetMatchesChannel("LanLink-0.6.0-telegram-mail-vpn-windows-x64.zip", windowsLab()));
        QVERIFY(!assetMatchesChannel("LanLink-0.5.1-cpp-windows-x64.zip", windowsLab()));
        QVERIFY(!assetMatchesChannel("LanLink-0.6.0-telegram-mail-vpn-linux-x64.zip", windowsLab()));
        QVERIFY(!assetMatchesChannel("LanLink-0.6.0-telegram-mail-vpn-windows-x64.zip.sha256", windowsLab()));
        QVERIFY(!assetMatchesChannel("notes.txt", windowsLab()));
    }
    void existingPrereleasesSelectTheLabZipNotCpp() {
        QString error;
        const auto releases = parseReleasesJson(existingReleases(), &error);
        QVERIFY(releases);
        QVERIFY(error.isEmpty());
        QCOMPARE(releases->size(), 3);
        const auto from051 = selectLatestUpdate(*releases, AppVersion{0, 5, 1}, windowsLab());
        QVERIFY(from051);
        QCOMPARE(from051->version, (AppVersion{0, 6, 0}));
        QCOMPARE(from051->package.name, QString("LanLink-0.6.0-telegram-mail-vpn-windows-x64.zip"));
        QCOMPARE(from051->package.apiUrl, QString("https://api.example/a60"));
        QCOMPARE(digestSha256(from051->package.digest),
                 QString("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
        QVERIFY((!selectLatestUpdate(*releases, AppVersion{0, 6, 0}, windowsLab())));
    }
    void prefersNewestMatchingReleaseAndSkipsDraftsAndCppOnly() {
        const auto json = QByteArray(
            "["
            "{\"tag_name\":\"v0.6.2-telegram-mail-vpn-windows-x64\",\"draft\":true,\"assets\":["
            "{\"name\":\"LanLink-0.6.2-telegram-mail-vpn-windows-x64.zip\",\"url\":\"https://api.example/d\",\"size\":1}]},"
            "{\"tag_name\":\"v0.7.0-windows-x64\",\"assets\":["
            "{\"name\":\"LanLink-0.7.0-cpp-windows-x64.zip\",\"url\":\"https://api.example/c\",\"size\":1}]},"
            "{\"tag_name\":\"v0.6.1-telegram-mail-vpn-windows-x64\",\"html_url\":\"https://example/r/0.6.1\",\"assets\":["
            "{\"name\":\"LanLink-0.6.1-telegram-mail-vpn-windows-x64.zip\",\"url\":\"https://api.example/n\",\"size\":2},"
            "{\"name\":\"LanLink-0.6.1-telegram-mail-vpn-windows-x64.zip.sha256\",\"url\":\"https://api.example/s\",\"size\":80}]},"
            "{\"tag_name\":\"v0.6.0-telegram-mail-vpn-windows-x64\",\"assets\":["
            "{\"name\":\"LanLink-0.6.0-telegram-mail-vpn-windows-x64.zip\",\"url\":\"https://api.example/o\",\"size\":1}]}"
            "]");
        const auto releases = parseReleasesJson(json);
        QVERIFY(releases);
        const auto selected = selectLatestUpdate(*releases, AppVersion{0, 6, 0}, windowsLab());
        QVERIFY(selected);
        QCOMPARE(selected->version, (AppVersion{0, 6, 1}));
        QCOMPARE(selected->tag, QString("v0.6.1-telegram-mail-vpn-windows-x64"));
        QCOMPARE(selected->checksum.name, QString("LanLink-0.6.1-telegram-mail-vpn-windows-x64.zip.sha256"));
        QVERIFY((!selectLatestUpdate(*releases, AppVersion{0, 6, 0}, UpdateChannel{"telegram-mail-vpn", "linux-x64"})));
    }
    void skipsAmbiguousPackagesAndInvalidJson() {
        QString error;
        QVERIFY(!parseReleasesJson("{}", &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!parseReleasesJson("not-json", &error));
        const auto releases = parseReleasesJson(QByteArray(
            "[{\"tag_name\":\"v0.6.2-telegram-mail-vpn-windows-x64\",\"assets\":["
            "{\"name\":\"LanLink-0.6.2-telegram-mail-vpn-windows-x64.zip\",\"url\":\"a\",\"size\":1},"
            "{\"name\":\"LanLink-0.6.2-telegram-mail-vpn-windows-x64-extra.zip\",\"url\":\"b\",\"size\":1}]}]"));
        QVERIFY(releases);
        QVERIFY((!selectLatestUpdate(*releases, AppVersion{0, 6, 0}, windowsLab())));
    }
    void parsesChecksumFilesAndDigests() {
        const auto name = QString("LanLink-0.6.1-telegram-mail-vpn-windows-x64.zip");
        const auto hex = QString("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        QCOMPARE(checksumForAsset(hex + "  " + name + "\n", name), hex);
        QCOMPARE(checksumForAsset(hex.toUpper() + " *" + name + "\n", name), hex);
        QCOMPARE(checksumForAsset("SHA256 (" + name + ") = " + hex + "\n", name), hex);
        QCOMPARE(checksumForAsset(hex + "  other.zip\n", name), QString());
        QCOMPARE(checksumForAsset(hex + "  " + name + "\n" + QString(64, 'a') + "  " + name + "\n", name), QString());
        QCOMPARE(digestSha256("SHA256: " + hex.toUpper()), hex);
        QVERIFY(digestSha256("md5:deadbeef").isEmpty());
    }
};

QTEST_APPLESS_MAIN(UpdateReleaseTest)
#include "test_updaterelease.moc"
