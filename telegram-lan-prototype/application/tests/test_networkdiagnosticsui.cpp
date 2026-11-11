#include "mainwindow.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QtTest>

class NetworkDiagnosticsUiTest : public QObject {
    Q_OBJECT
    QPushButton *button(QDialog *dialog,const QString &text) {
        for(auto *b:dialog->findChildren<QPushButton*>())if(b->text()==text)return b;
        return nullptr;
    }
  private slots:
    void localStatusRefreshAndDemoCannotDial() {
        QTemporaryDir dir;MainWindow window(dir.path(),0,true);
        bool inspected=false;
        QTimer::singleShot(0,&window,[&]{
            auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(!dialog)return;
            QTimer::singleShot(1000,dialog,&QDialog::reject);
            const auto details=dialog->findChild<QPlainTextEdit*>();QVERIFY(details);
            const auto report=QJsonDocument::fromJson(details->toPlainText().toUtf8()).object();
            QCOMPARE(report["mode"].toString(),QString("lan"));QVERIFY(!report["application_identity_verified"].toBool());
            auto *test=button(dialog,"测试连接");QVERIFY(test);QVERIFY(!test->isEnabled());
            auto *refresh=button(dialog,"刷新本机状态");QVERIFY(refresh);refresh->click();
            inspected=true;dialog->reject();
        });
        window.showNetworkDiagnostics();QVERIFY(inspected);
    }
    void pendingTestCanCloseAndReopenWithoutStaleCallback() {
        QTemporaryDir dir;MainWindow window(dir.path(),0,true);
        window.m_demo=false;QVERIFY(window.node()->start(0,0));
        QTcpServer silent;QVERIFY(silent.listen(QHostAddress::LocalHost,0));
        bool started=false;
        QTimer::singleShot(0,&window,[&]{
            auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;
            QTimer::singleShot(1000,dialog,&QDialog::reject);
            dialog->findChild<QLineEdit*>()->setText("127.0.0.1");
            dialog->findChild<QSpinBox*>()->setValue(silent.serverPort());
            auto *test=button(dialog,"测试连接");QVERIFY(test);QVERIFY(test->isEnabled());
            test->click();QVERIFY(!test->isEnabled());
            QTRY_VERIFY_WITH_TIMEOUT(silent.hasPendingConnections(),500);QTest::qWait(10);
            started=true;dialog->reject();
        });
        window.showNetworkDiagnostics();QVERIFY(started);
        bool reopened=false;
        QTimer::singleShot(0,&window,[&]{
            auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;
            QTimer::singleShot(1000,dialog,&QDialog::reject);
            // Finishes the earlier dialog's request while a new dialog is open.
            window.node()->stop();QCoreApplication::processEvents();
            QVERIFY(button(dialog,"测试连接")->isEnabled());
            for(auto *label:dialog->findChildren<QLabel*>())QVERIFY(!label->text().startsWith("连接未成功"));
            reopened=true;dialog->reject();
        });
        window.showNetworkDiagnostics();QVERIFY(reopened);
    }
};
QTEST_MAIN(NetworkDiagnosticsUiTest)
#include "test_networkdiagnosticsui.moc"
