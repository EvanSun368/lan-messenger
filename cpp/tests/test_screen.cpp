#include "node.h"
#include "screenservice.h"
#include <QBuffer>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
class ScreenTest : public QObject {
    Q_OBJECT
  private slots:
    void approvalAndFrames() {
        QTemporaryDir dir;
        Node a("a", dir.path() + "/a"), b("b", dir.path() + "/b");
        QVERIFY(a.start(0, 0));
        QVERIFY(b.start(0, 0));
        ScreenService sa(&a), sb(&b);
        b.serviceHandler = [&](QTcpSocket *s, QJsonObject r, QString h) { return sb.handle(s, r, h); };
        a.remember(b.hello(), "127.0.0.1");
        QSignalSpy frames(&sb, &ScreenService::frameReceived);
        QString receiver;
        connect(&sb, &ScreenService::event, &sb, [&](QString k, QJsonObject d) {
            if (k == "screen_offer")
                receiver = d["session"].toString();
        });
        auto sessions = sa.startMany({b.id()});
        QCOMPARE(sessions.size(), 1);
        QTRY_VERIFY(!receiver.isEmpty());
        QImage im(100, 100, QImage::Format_RGB32);
        im.fill(Qt::red);
        QByteArray jpg;
        QBuffer buffer(&jpg);
        buffer.open(QIODevice::WriteOnly);
        im.save(&buffer, "JPEG");
        sa.publish(jpg);
        QTest::qWait(20);
        QCOMPARE(frames.size(), 0);
        sb.decide(receiver, true);
        QTest::qWait(30);
        sa.publish(jpg);
        QTRY_COMPARE(frames.size(), 1);
        sb.stopAll();
        QTRY_VERIFY(!sa.sending());
        a.stop();
        b.stop();
    }
    void groupIndependentRejection() {
        QTemporaryDir dir;
        Node a("a", dir.path() + "/a"), b("b", dir.path() + "/b"), c("c", dir.path() + "/c");
        QVERIFY(a.start(0, 0));
        QVERIFY(b.start(0, 0));
        QVERIFY(c.start(0, 0));
        ScreenService sa(&a), sb(&b), sc(&c);
        b.serviceHandler = [&](QTcpSocket *s, QJsonObject r, QString h) { return sb.handle(s, r, h); };
        c.serviceHandler = [&](QTcpSocket *s, QJsonObject r, QString h) { return sc.handle(s, r, h); };
        a.remember(b.hello(), "127.0.0.1");
        a.remember(c.hello(), "127.0.0.1");
        connect(&sb, &ScreenService::event, &sb, [&](QString k, QJsonObject d) {
            if (k == "screen_offer")
                sb.decide(d["session"].toString(), false);
        });
        connect(&sc, &ScreenService::event, &sc, [&](QString k, QJsonObject d) {
            if (k == "screen_offer")
                sc.decide(d["session"].toString(), true);
        });
        QSignalSpy frames(&sc, &ScreenService::frameReceived);
        QCOMPARE(sa.startMany({b.id(), c.id()}).size(), 2);
        QTest::qWait(100);
        QVERIFY(sa.sending());
        QImage im(100, 100, QImage::Format_RGB32);
        im.fill(Qt::red);
        QByteArray jpg;
        QBuffer buffer(&jpg);
        buffer.open(QIODevice::WriteOnly);
        im.save(&buffer, "JPEG");
        sa.publish(jpg);
        QTRY_COMPARE(frames.size(), 1);
        sa.stopAll();
        QVERIFY(!sa.sending());
    }
    void rejection() {
        QTemporaryDir dir;
        Node a("a", dir.path() + "/a"), b("b", dir.path() + "/b");
        QVERIFY(a.start(0, 0));
        QVERIFY(b.start(0, 0));
        ScreenService sa(&a), sb(&b);
        b.serviceHandler = [&](QTcpSocket *s, QJsonObject r, QString h) { return sb.handle(s, r, h); };
        a.remember(b.hello(), "127.0.0.1");
        connect(&sb, &ScreenService::event, &sb, [&](QString k, QJsonObject d) {
            if (k == "screen_offer")
                sb.decide(d["session"].toString(), false);
        });
        sa.startMany({b.id()});
        QTRY_VERIFY(!sa.sending());
    }
};
QTEST_GUILESS_MAIN(ScreenTest)
#include "test_screen.moc"
