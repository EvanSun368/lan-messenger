#include "node.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QTimer>
#include <iostream>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc != 2)
        return 2;
    Node n("C++ peer", QString::fromLocal8Bit(argv[1]));
    QObject::connect(&n, &Node::event, &app, [&n](QString kind, QJsonObject d) {
        if (kind == "message" && d["direction"] == "in") {
            QString path = d["image_path"].toString();
            if (!path.isEmpty()) {
                QFile f(path);
                if (f.open(QIODevice::ReadOnly))
                    n.sendImage(d["peer_id"].toString(), f.readAll());
            } else
                n.sendMessage(d["peer_id"].toString(), "echo:" + d["text"].toString());
        }
    });
    if (!n.start(0, 0))
        return 3;
    std::cout << QJsonDocument(n.hello()).toJson(QJsonDocument::Compact).constData() << std::endl;
    QTimer::singleShot(30000, &app, &QCoreApplication::quit);
    return app.exec();
}
