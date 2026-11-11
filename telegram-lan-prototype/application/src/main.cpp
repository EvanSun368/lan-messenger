#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTextStream>
#include <QTimer>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("LanLink");
    QCoreApplication::setApplicationName("LanLinkTelegramLab");
    QCoreApplication::setApplicationVersion("0.6.0-source-lab");
    QCommandLineParser p;
    p.addHelpOption();
    p.addVersionOption();
    p.addOption({"demo", "演示数据，不连接网络"});
    p.addOption({"screenshot", "启动后保存窗口截图", "path"});
    p.addOption({"view", "截图窗口 main/chat/history/transfers", "view", "main"});
    p.addOption({"storage", "数据目录", "path"});
    p.addOption({"port", "TCP 监听端口，0自动分配", "port", "0"});
    p.addOption({"vpn", "启用 VPN 专用模式，关闭广播，无 LAN 回退；默认固定端口 45870"});
    p.addOption({"vpn-interface", "已由用户配置的 VPN 接口系统名称", "name"});
    p.addOption({"vpn-address", "该接口的本机 IPv4 地址", "ipv4"});
    p.addOption({"vpn-peer", "明确允许的对端私网/100.64.0.0/10 IPv4，可重复指定", "ipv4"});
    p.addOption({"list-network-interfaces", "列出本机接口和 IPv4 后退出，不连接对端"});
    p.process(app);
    if (p.isSet("list-network-interfaces")) {
        QTextStream(stdout) << QJsonDocument(NetworkPolicy::interfaces()).toJson(QJsonDocument::Indented);
        return 0;
    }
    NetworkPolicy network;
    network.vpnOnly = p.isSet("vpn");
    if (!network.vpnOnly && (p.isSet("vpn-interface") || p.isSet("vpn-address") || p.isSet("vpn-peer"))) {
        QTextStream(stderr) << "VPN 参数必须与 --vpn 一同启用" << Qt::endl; return 2;
    }
    network.interfaceName = p.value("vpn-interface");
    const auto addressText = p.value("vpn-address");
    if (!addressText.isEmpty()) network.localAddress = QHostAddress(addressText);
    for (const auto &peer : p.values("vpn-peer")) network.allowedPeers.append(QHostAddress(peer));
    const auto invalid = network.configurationError();
    if (!invalid.isEmpty() || (p.isSet("vpn-address") && network.localAddress.isNull())) {
        QTextStream(stderr) << (invalid.isEmpty() ? QString("无效 VPN IPv4 地址") : invalid) << Qt::endl;
        return 2;
    }
    QTemporaryDir demo;
    QString root = p.value("storage");
    if (p.isSet("demo") && root.isEmpty())
        root = demo.path();
    bool ok = false;
    int port = p.value("port").toInt(&ok);
    if (network.vpnOnly && !p.isSet("port")) port = 45870;
    if (!ok || port < 0 || port > 65535)
        return 2;
    if (network.vpnOnly && !port) { QTextStream(stderr) << "VPN 模式需要固定非零 TCP 端口" << Qt::endl; return 2; }
    MainWindow w(root, quint16(port), p.isSet("demo"), nullptr, network);
    w.show();
    if (p.isSet("screenshot")) {
        QTimer::singleShot(1200, &w, [&] {
            w.saveDemoScreenshot(p.value("screenshot"), p.value("view"));
            QTimer::singleShot(100, &app, &QApplication::quit);
        });
    }
    return app.exec();
}
