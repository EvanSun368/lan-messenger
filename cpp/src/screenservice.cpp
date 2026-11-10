#include "screenservice.h"
#include "node.h"
#include <QBuffer>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>
#include <QtEndian>
ScreenService::ScreenService(Node *n, QObject *p) : QObject(p), node(n) {}
ScreenService::~ScreenService() {
    stopAll();
}
bool ScreenService::sending() const {
    for (auto s : sessions)
        if (s->outgoing)
            return true;
    return false;
}
void ScreenService::resetTimeout(Session *s, int ms) {
    s->timeout->start(ms);
}
void ScreenService::attach(QString id, Session *s, int ms) {
    sessions[id] = s;
    s->socket->setParent(this);
    s->socket->setReadBufferSize(MaxJpeg + 4);
    s->timeout = new QTimer(this);
    s->timeout->setSingleShot(true);
    connect(s->timeout, &QTimer::timeout, this, [this, id] { fail(id, QStringLiteral("投屏连接超时")); });
    connect(s->socket, &QTcpSocket::readyRead, this, [this, id] { consume(id); });
    connect(s->socket, &QTcpSocket::disconnected, this, [this, id] { stop(id); });
    connect(s->socket, &QTcpSocket::errorOccurred, this, [this, id](QAbstractSocket::SocketError) {
        if (sessions.contains(id))
            fail(id, sessions[id]->socket->errorString());
    });
    resetTimeout(s, ms);
}
bool ScreenService::handle(QTcpSocket *sock, QJsonObject r, QString host) {
    if (r["type"].toString() != "screen_offer")
        return false;
    for (auto s : sessions)
        if (!s->outgoing) {
            Node::writeFrame(sock, {{"accepted", false}});
            sock->disconnectFromHost();
            sock->deleteLater();
            return true;
        }
    auto s = new Session;
    s->socket = sock;
    s->peer = r["sender"].toObject()["id"].toString();
    QString id = QUuid::createUuid().toString(QUuid::Id128);
    attach(id, s, 30000);
    emit event("screen_offer", {{"session", id}, {"peer_id", s->peer}, {"host", host}});
    return true;
}
QStringList ScreenService::startMany(QStringList peerIds) {
    peerIds.removeDuplicates();
    if (peerIds.isEmpty() || peerIds.size() > 63 || sending()) {
        emit event("screen_status", {{"message", QStringLiteral("请选择 1–63 个接收人，或先停止当前投屏")}});
        return {};
    }
    QHash<QString, QJsonObject> peers;
    for (auto v : node->peers()) {
        auto p = v.toObject();
        peers[p["id"].toString()] = p;
    }
    for (auto id : peerIds) {
        auto p = peers.value(id);
        if (p.isEmpty() || !Node::privateAddress(QHostAddress(p["host"].toString()))) {
            emit event("screen_status", {{"message", QStringLiteral("接收人地址不可用")}});
            return {};
        }
    }
    QStringList result;
    for (auto peer : peerIds) {
        auto s = new Session;
        s->socket = new QTcpSocket(this);
        s->socket->setProxy(QNetworkProxy::NoProxy);
        s->peer = peer;
        s->outgoing = true;
        QString id = QUuid::createUuid().toString(QUuid::Id128);
        attach(id, s, 35000);
        result << id;
        connect(s->socket, &QTcpSocket::connected, this, [this, id] {
            if (!sessions.contains(id))
                return;
            Node::writeFrame(sessions[id]->socket,
                             {{"type", "screen_offer"}, {"sender", node->hello()}, {"session", id}});
        });
        auto p = peers[peer];
        s->socket->connectToHost(p["host"].toString(), quint16(p["port"].toInt()));
    }
    return result;
}
void ScreenService::decide(QString id, bool approved) {
    auto s = sessions.value(id);
    if (!s || s->outgoing || s->approved)
        return;
    Node::writeFrame(s->socket, {{"accepted", approved}});
    if (!approved) {
        s->socket->disconnectFromHost();
        return;
    }
    s->approved = true;
    resetTimeout(s, 10000);
    emit event("screen_started", {{"session", id}, {"direction", "in"}, {"peer_id", s->peer}});
    consume(id);
}
void ScreenService::publish(QByteArray jpeg) {
    if (jpeg.isEmpty() || jpeg.size() > MaxJpeg || !jpeg.startsWith(QByteArray::fromHex("ffd8")))
        return;
    QByteArray frame(4, 0);
    qToBigEndian<quint32>(quint32(jpeg.size()), reinterpret_cast<uchar *>(frame.data()));
    frame += jpeg;
    for (auto id : sessions.keys()) {
        auto s = sessions.value(id);
        if (!s || !s->outgoing || !s->approved || s->waiting || s->delayed)
            continue;
        s->waiting = true;
        s->socket->write(frame);
        resetTimeout(s, 5000);
    }
}
void ScreenService::consume(QString id) {
    auto s = sessions.value(id);
    if (!s)
        return;
    if (!s->outgoing && !s->approved) {
        if (s->socket->bytesAvailable())
            fail(id, QStringLiteral("确认前不接受投屏数据"));
        return;
    }
    if (s->delayed)
        return;
    s->buffer += s->socket->readAll();
    int limit = s->outgoing ? 65536 : MaxJpeg;
    if (s->buffer.size() > limit + 4) {
        fail(id, "投屏数据超限");
        return;
    }
    if (s->buffer.size() < 4)
        return;
    quint32 len = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(s->buffer.constData()));
    if (!len || len > quint32(limit)) {
        fail(id, "无效投屏帧");
        return;
    }
    if (s->buffer.size() < int(len) + 4)
        return;
    auto payload = s->buffer.mid(4, len);
    s->buffer.remove(0, len + 4);
    if (s->outgoing) {
        QJsonParseError err;
        auto d = QJsonDocument::fromJson(payload, &err);
        if (err.error != QJsonParseError::NoError || !d.isObject()) {
            fail(id, "无效投屏应答");
            return;
        }
        auto obj = d.object();
        if (!s->approved) {
            if (!obj["accepted"].toBool()) {
                fail(id, "对方拒绝投屏");
                return;
            }
            s->approved = true;
            resetTimeout(s, 10000);
            emit event("screen_started", {{"session", id}, {"direction", "out"}, {"peer_id", s->peer}});
        } else {
            if (!s->waiting || !obj["ok"].toBool()) {
                fail(id, "投屏应答错误");
                return;
            }
            s->waiting = false;
            s->delayed = true;
            resetTimeout(s, 10000);
            QTimer::singleShot(200, this, [this, id] {
                auto s = sessions.value(id);
                if (s) {
                    s->delayed = false;
                    consume(id);
                }
            });
        }
        s = sessions.value(id);
        if (s && !s->buffer.isEmpty())
            fail(id, "多余投屏应答");
    } else {
        QBuffer b(&payload);
        b.open(QIODevice::ReadOnly);
        QImageReader reader(&b, "JPEG");
        auto size = reader.size();
        if (!payload.startsWith(QByteArray::fromHex("ffd8")) || !size.isValid() || size.width() > 1920 ||
            size.height() > 1080) {
            fail(id, "无效投屏图片");
            return;
        }
        s->delayed = true;
        resetTimeout(s, 10000);
        emit frameReceived(id, payload);
        QTimer::singleShot(200, this, [this, id] {
            auto s = sessions.value(id);
            if (!s)
                return;
            s->delayed = false;
            Node::writeFrame(s->socket, {{"ok", true}});
            consume(id);
        });
    }
}
void ScreenService::fail(QString id, QString message) {
    if (!sessions.contains(id))
        return;
    emit event("screen_status", {{"session", id}, {"message", message}});
    stop(id);
}
void ScreenService::stop(QString id) {
    auto s = sessions.take(id);
    if (!s)
        return;
    auto out = s->outgoing;
    s->socket->disconnect(this);
    s->socket->abort();
    s->socket->deleteLater();
    s->timeout->stop();
    s->timeout->deleteLater();
    delete s;
    emit event("screen_stopped", {{"session", id}, {"direction", out ? "out" : "in"}});
}
void ScreenService::stopAll() {
    for (auto id : sessions.keys())
        stop(id);
}
