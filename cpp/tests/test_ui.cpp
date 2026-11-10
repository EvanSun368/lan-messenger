#include "mainwindow.h"
#include "node.h"
#include "chatnotifications.h"
#include <QtTest>
#include <QtWidgets>
class UiTest : public QObject {
    Q_OBJECT
    QTemporaryDir settingsDir;
  private slots:
    void initTestCase() {
        QVERIFY(settingsDir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDir.path());
    }
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
    void imageScrollAndLongText() {
        QTemporaryDir dir;MainWindow w(dir.path(),0,true);w.show();
        w.saveDemoScreenshot(dir.filePath("chat.png"),"chat");
        auto *scroll=w.findChild<QScrollArea *>("messages");auto *bar=scroll->verticalScrollBar();
        for(int i=0;i<20;++i) w.appendMessage({{"direction","in"},{"text",QString("Message %1").arg(i)}});
        QTRY_VERIFY(bar->maximum()>0);
        QTRY_COMPARE(bar->value(),bar->maximum());
        bar->triggerAction(QAbstractSlider::SliderToMinimum);bar->setValue(40);
        w.appendMessage({{"direction","in"},{"text","new while reading"}});
        QTRY_COMPARE(bar->value(),40);
        // Persisted-history refresh must not jump to bottom while reading older messages.
        const QString peerId="11111111111111111111111111111111";
        QVERIFY(w.node()->remember({{"id",peerId},{"name","Test"},{"port",45870}},"127.0.0.1"));
        w.m_current=peerId;
        for(int i=0;i<25;++i) QVERIFY(!w.node()->sendMessage(peerId,QString("Stored %1").arg(i)).isEmpty());
        QTRY_VERIFY(bar->maximum()>60);
        bar->triggerAction(QAbstractSlider::SliderToMinimum);bar->setValue(60);QCOMPARE(bar->value(),60);
        w.renderHistory();QTRY_COMPARE(bar->value(),60);
        for(int i=0;i<8;++i) w.renderHistory();
        QTRY_COMPARE(bar->value(),60);
        bar->triggerAction(QAbstractSlider::SliderToMaximum);w.renderHistory();QTRY_COMPARE(bar->value(),bar->maximum());
        const QString longText=QString(32768,'x');
        w.appendMessage({{"direction","in"},{"text",longText}});
        w.appendMessage({{"direction","in"},{"text","https://example.com/"+QString(2000,'a')}});
        auto *chat=w.findChild<QDialog *>("chatWindow");chat->resize(760,540);
        w.m_historyPanel->show();QTest::qWait(60);
        for(auto *bubble:scroll->findChildren<QFrame *>("messageBubble")) {
            const auto left=bubble->mapTo(scroll->viewport(),QPoint(0,0)).x();
            QVERIFY(left>=0);QVERIFY(left+bubble->width()<=scroll->viewport()->width());
        }
        auto texts=w.findChildren<QTextBrowser *>("messageText");
        QTextBrowser *large=nullptr;
        for(auto *text:texts){QVERIFY(text->width()<=320);if(text->toPlainText()==longText)large=text;}
        QVERIFY(large);QVERIFY(large->height()>1000);
        QCOMPARE(large->document()->defaultTextOption().wrapMode(),QTextOption::WrapAtWordBoundaryOrAnywhere);
        const int oldHeight=large->height();QFont font=w.m_chatFont;font.setPointSize(20);w.applyChatFont(font);
        QVERIFY(large->height()>oldHeight);QCOMPARE(large->toPlainText(),longText);
        QImage picture(900,700,QImage::Format_RGB32);picture.fill(Qt::blue);
        const auto imagePath=dir.filePath("image.png");QVERIFY(picture.save(imagePath));
        w.appendMessage({{"direction","in"},{"text","image"},{"image_path",imagePath}});
        auto *image=w.findChild<QToolButton *>("messageImage");QVERIFY(image);image->click();
        auto *viewer=w.findChild<QDialog *>("imageViewer");QVERIFY(viewer);QVERIFY(viewer->isVisible());
        auto *original=viewer->findChild<QLabel *>("originalImage");QVERIFY(original);
        QCOMPARE(original->pixmap().size(),QSize(900,700));viewer->close();QTest::qWait(10);
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
        auto texts=w.findChildren<QTextBrowser *>("messageText");
        QVERIFY(texts.size()>=4);
        for(auto *label:texts) {QVERIFY(!label->openExternalLinks());QVERIFY(label->minimumWidth()>=60);}
        QVERIFY(QFileInfo(dir.filePath("chat.png")).size() > 0);
        chat->close();
        QVERIFY(!chat->isVisible());
        w.saveDemoScreenshot(dir.filePath("history.png"), "history");
        QVERIFY(chat->isVisible());
    }
};
QTEST_MAIN(UiTest)
#include "test_ui.moc"
