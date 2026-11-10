#include "linkpreview.h"
#include <QtTest>
class Links : public QObject {
    Q_OBJECT
  private slots:
    void validation() {
        QVERIFY(LinkPreview::allowedUrl(QUrl("https://example.com/path")));
        QVERIFY(!LinkPreview::allowedUrl(QUrl("file:///etc/passwd")));
        QVERIFY(!LinkPreview::allowedUrl(QUrl("https://u:p@example.com")));
        QVERIFY(!LinkPreview::allowedUrl(QUrl("https://example.com:9999")));
        QVERIFY(!LinkPreview::allowedUrl(QUrl("https://[broken")));
    }
    void addresses() {
        QVERIFY(!LinkPreview::publicAddress(QHostAddress("127.0.0.1")));
        QVERIFY(!LinkPreview::publicAddress(QHostAddress("10.1.2.3")));
        QVERIFY(!LinkPreview::publicAddress(QHostAddress("169.254.1.1")));
        QVERIFY(!LinkPreview::publicAddress(QHostAddress("172.16.1.1")));
        QVERIFY(!LinkPreview::publicAddress(QHostAddress("192.168.1.1")));
        QVERIFY(!LinkPreview::publicAddress(QHostAddress("100.64.1.1")));
        QVERIFY(LinkPreview::publicAddress(QHostAddress("8.8.8.8")));
    }
};
QTEST_MAIN(Links)
#include "test_links.moc"
