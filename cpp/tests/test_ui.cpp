#include "mainwindow.h"
#include "node.h"
#include "chatnotifications.h"
#include <QtTest>
#include <QtWidgets>
class UiTest : public QObject {
    Q_OBJECT
  private slots:
    void layoutAndDrafts() {
        QTemporaryDir dir;
        MainWindow w(dir.path(), 0, true);
        w.show();
        QTest::qWait(50);
        auto *contacts = w.findChild<QListWidget *>("contacts");
        auto *editor = w.findChild<QPlainTextEdit *>("editor");
        auto *tabs = w.findChild<QTabBar *>("rosterTabs");
        auto *search = w.findChild<QLineEdit *>("search");
        QVERIFY(contacts);
        QVERIFY(editor);
        QVERIFY(contacts->count() >= 3);
        contacts->setCurrentRow(0);
        editor->setPlainText("draft one");
        contacts->setCurrentRow(1);
        QCOMPARE(editor->toPlainText(), QString());
        editor->setPlainText("draft two");
        contacts->setCurrentRow(0);
        QCOMPARE(editor->toPlainText(), QString("draft one"));
        search->setText("no match");
        for (int i = 0; i < contacts->count(); ++i)
            QVERIFY(contacts->item(i)->isHidden());
        search->clear();
        tabs->setCurrentIndex(2);
        int count = 0;
        for (int i = 0; i < contacts->count(); ++i)
            if (!contacts->item(i)->isHidden()) {
                ++count;
                QVERIFY(contacts->item(i)->data(Qt::UserRole).toString().startsWith("group:"));
            }
        QCOMPARE(count, 1);
        w.resize(340, 600);
        QTest::qWait(20);
        QVERIFY(contacts->width() > 0);
    }
    void composeAndUnread() {
        QTemporaryDir dir; MainWindow w(dir.path(),0,true); w.show();
        auto *contacts=w.findChild<QListWidget *>("contacts");
        auto *editor=w.findChild<QPlainTextEdit *>("editor");
        auto *split=w.findChild<QSplitter *>("composeSplitter");
        QVERIFY(split); QVERIFY(!split->childrenCollapsible());
        QVERIFY(w.findChild<QLineEdit *>("historySearch"));
        QVERIFY(w.findChild<QToolButton *>("sendOptions"));
        contacts->setCurrentRow(0);
        w.saveDemoScreenshot(dir.filePath("chat.png"),"chat");
        editor->setPlainText("send shortcut");
        auto *options=w.findChild<QToolButton *>("sendOptions");
        options->menu()->actions().at(0)->trigger();
        QTest::keyClick(editor,Qt::Key_Return,Qt::ControlModifier);
        QCOMPARE(editor->toPlainText(),QString());
        options->menu()->actions().at(1)->trigger();
        editor->setPlainText("next line");
        QTest::keyClick(editor,Qt::Key_Return,Qt::ShiftModifier);
        QVERIFY(editor->toPlainText().contains("\n"));
        QTest::keyClick(editor,Qt::Key_Return);
        QCOMPARE(editor->toPlainText(),QString());
        options->menu()->actions().at(0)->trigger();
    }
    void notificationIntegration() {
        QTemporaryDir dir; MainWindow w(dir.path(),0,true); w.show();
        auto *contacts=w.findChild<QListWidget *>("contacts");
        const QString id=contacts->item(0)->data(Qt::UserRole).toString();
        auto *notifications=w.findChild<ChatNotifications *>(); QVERIFY(notifications);
        w.node()->event("message", {{"peer_id",id},{"direction","in"},{"text","new message"}});
        QCOMPARE(notifications->unread(id),1);
        QVERIFY(w.windowTitle().contains("1"));
        contacts->setCurrentRow(0);
        w.saveDemoScreenshot(dir.filePath("read.png"),"chat");
        QCOMPARE(notifications->unread(id),0);
        w.node()->event("message", {{"peer_id",id},{"direction","out"},{"text","outgoing"}});
        QCOMPARE(notifications->unread(id),0);
    }
    void screenshots() {
        QTemporaryDir dir;
        MainWindow w(dir.path(), 0, true);
        w.show();
        QTest::qWait(30);
        w.saveDemoScreenshot(dir.filePath("main.png"));
        QVERIFY(QFileInfo(dir.filePath("main.png")).size() > 0);
        w.saveDemoScreenshot(dir.filePath("chat.png"), "chat");
        auto *chat = w.findChild<QDialog *>("chatWindow");
        QVERIFY(chat);
        QVERIFY(chat->isVisible());
        QVERIFY(QFileInfo(dir.filePath("chat.png")).size() > 0);
        chat->close();
        QVERIFY(!chat->isVisible());
        w.saveDemoScreenshot(dir.filePath("history.png"), "history");
        QVERIFY(chat->isVisible());
    }
};
QTEST_MAIN(UiTest)
#include "test_ui.moc"
