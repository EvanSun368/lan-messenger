#include "mainwindow.h"
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
