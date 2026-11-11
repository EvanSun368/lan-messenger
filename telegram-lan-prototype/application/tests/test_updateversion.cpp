#include "updateversion.h"
#include <QtTest>

class UpdateVersionTest : public QObject {
    Q_OBJECT
private slots:
    void parsesPlainAndPrefixedVersions() {
        const auto plain = parseAppVersion("0.6.1");
        QVERIFY(plain.has_value());
        QCOMPARE(plain->toString(), QString("0.6.1"));
        const auto prefixed = parseAppVersion("v0.6.0");
        QVERIFY(prefixed.has_value());
        QCOMPARE(*prefixed, (AppVersion{0, 6, 0}));
    }
    void parsesVersionFromReleaseTagsAndAssetNames() {
        const auto tag = parseAppVersion("v0.6.0-telegram-mail-vpn-windows-x64");
        QVERIFY(tag.has_value());
        QCOMPARE(*tag, (AppVersion{0, 6, 0}));
        const auto legacy = parseAppVersion("v0.5.1-windows-x64");
        QVERIFY(legacy.has_value());
        QCOMPARE(*legacy, (AppVersion{0, 5, 1}));
        const auto asset = parseAppVersion("LanLink-0.6.1-telegram-mail-vpn-windows-x64.zip");
        QVERIFY(asset.has_value());
        QCOMPARE(*asset, (AppVersion{0, 6, 1}));
    }
    void rejectsIncompleteOrNonVersions() {
        for (const auto &text : QStringList{"", "v", "1.2", "v0.6", "10.20.0.2", "version", "v0.6.0rc1", "1.2.3.4"})
            QVERIFY2(!parseAppVersion(text).has_value(), qPrintable(text));
    }
    void comparesNumerically() {
        QVERIFY((AppVersion{0, 6, 1} > AppVersion{0, 6, 0}));
        QVERIFY((AppVersion{0, 6, 0} > AppVersion{0, 5, 1}));
        QVERIFY((AppVersion{0, 10, 0} > AppVersion{0, 9, 9}));
        QVERIFY((AppVersion{1, 0, 0} > AppVersion{0, 99, 99}));
        QVERIFY((AppVersion{0, 6, 1} == parseAppVersion("0.6.1")));
        QVERIFY((AppVersion{0, 6, 1} >= AppVersion{0, 6, 1}));
        QVERIFY((!(AppVersion{0, 6, 0} > AppVersion{0, 6, 0})));
    }
};

QTEST_APPLESS_MAIN(UpdateVersionTest)
#include "test_updateversion.moc"
