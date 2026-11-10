#pragma once
#include <QHostAddress>
#include <QObject>
#include <QUrl>
#include <functional>
class LinkPreview : public QObject {
    Q_OBJECT
  public:
    explicit LinkPreview(QObject *parent = nullptr) : QObject(parent) {}
    using Callback = std::function<void(QString title, QString description, QString error)>;
    void fetch(QUrl url, Callback callback);
    static bool allowedUrl(const QUrl &url);
    static bool publicAddress(const QHostAddress &address);
};
