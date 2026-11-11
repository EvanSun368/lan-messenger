#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QTemporaryDir>
#include <QTimer>
#include <QIcon>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("LanLink");
    QCoreApplication::setApplicationName("LanLinkCpp");
    QCoreApplication::setApplicationVersion("0.5.1");
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/lanlink.png")));
    QCommandLineParser p;
    p.addHelpOption();
    p.addVersionOption();
    p.addOption({"demo", "演示数据，不连接网络"});
    p.addOption({"screenshot", "启动后保存窗口截图", "path"});
    p.addOption({"view", "截图窗口 main/chat/history/transfers", "view", "main"});
    p.addOption({"storage", "数据目录", "path"});
    p.addOption({"port", "TCP 监听端口，0自动分配", "port", "0"});
    p.process(app);
    QTemporaryDir demo;
    QString root = p.value("storage");
    if (p.isSet("demo") && root.isEmpty())
        root = demo.path();
    bool ok = false;
    int port = p.value("port").toInt(&ok);
    if (!ok || port < 0 || port > 65535)
        return 2;
    MainWindow w(root, quint16(port), p.isSet("demo"));
    w.show();
    if (p.isSet("screenshot")) {
        QTimer::singleShot(1200, &w, [&] {
            w.saveDemoScreenshot(p.value("screenshot"), p.value("view"));
            QTimer::singleShot(100, &app, &QApplication::quit);
        });
    }
    return app.exec();
}
