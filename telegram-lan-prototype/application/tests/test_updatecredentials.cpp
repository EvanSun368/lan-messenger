#include "updatecredentials.h"
#include <QtTest>

class UpdateCredentialsTest : public QObject {
    Q_OBJECT
private slots:
    void platformFailsClosedWithoutWindowsStore() {
        PlatformUpdateCredentialStore store;
#ifdef Q_OS_WIN
        QVERIFY(store.available());
        QVERIFY(store.unavailableReason().isEmpty());
#else
        QVERIFY(!store.available());
        QVERIFY(!store.unavailableReason().isEmpty());
        QByteArray token = QByteArray("github_pat_this_is_long_enough_to_pass");
        QString error;
        QVERIFY(!store.save(token, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!error.contains("github_pat"));
        QVERIFY(!store.load(&token, &error));
        QVERIFY(token.isEmpty());
        QVERIFY(!store.hasToken());
        // remove also fails closed rather than pretending a secret was erased.
        QVERIFY(!store.remove(&error));
        QVERIFY(!error.contains("github_pat"));
#endif
    }
    void rejectsImplausibleTokens() {
        QVERIFY(!tokenLooksPlausible({}));
        QVERIFY(!tokenLooksPlausible("short"));
        QVERIFY(!tokenLooksPlausible(QByteArray("token-with-newline-which-is-not-ok!!\n")));
        QVERIFY(tokenLooksPlausible(QByteArray("github_pat_this_is_long_enough_to_pass")));
    }
};

QTEST_APPLESS_MAIN(UpdateCredentialsTest)
#include "test_updatecredentials.moc"
