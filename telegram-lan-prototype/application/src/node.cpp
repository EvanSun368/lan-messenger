#include "node.h"
#include <cmath>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostInfo>
#include <QImage>
#include <QImageReader>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QtEndian>
#include <algorithm>
namespace {
QString uid() {
    return QUuid::createUuid().toString(QUuid::Id128);
}
QString json(QJsonValue v) {
    return QString::fromUtf8(v.isArray() ? QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact)
                                         : QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
}
QJsonObject obj(QString s) {
    return QJsonDocument::fromJson(s.toUtf8()).object();
}
QJsonArray arr(QString s) {
    return QJsonDocument::fromJson(s.toUtf8()).array();
}
bool profileValid(QJsonObject p) {
    for (auto key : {"nickname", "signature", "organization", "department", "location"}) {
        if (!p.value(key).isString())
            return false;
        QString v = p.value(key).toString();
        if (v.size() > (QString(key) == "signature" ? 200 : 80))
            return false;
        for (QChar c : v)
            if (c.unicode() < 32 || c.unicode() == 127)
                return false;
    }
    return !p["nickname"].toString().trimmed().isEmpty();
}
bool exec(QSqlDatabase db, QString sql, QVariantList args = {}) {
    QSqlQuery q(db);
    q.prepare(sql);
    for (auto a : args)
        q.addBindValue(a);
    return q.exec();
}
} // namespace
Node::Node(QString name, QString storageDir, QObject *parent)
    : QObject(parent), m_name(name.left(80)), m_root(storageDir), m_connection(uid()) {
    m_server.setProxy(QNetworkProxy::NoProxy);
    m_udp.setProxy(QNetworkProxy::NoProxy);
    QDir().mkpath(m_root);
    QFile identity(m_root + "/identity");
    if (identity.open(QIODevice::ReadOnly))
        m_id = QString::fromUtf8(identity.readAll()).trimmed();
    if (!validId(m_id)) {
        m_id = uid();
        QSaveFile f(m_root + "/identity");
        if (f.open(QIODevice::WriteOnly)) {
            f.write(m_id.toUtf8());
            f.commit();
        }
    }
    m_profile = {{"nickname", m_name.isEmpty() ? QStringLiteral("我的电脑") : m_name},
                 {"signature", ""},
                 {"organization", ""},
                 {"department", ""},
                 {"location", ""}};
    QFile pf(m_root + "/profile.json");
    if (pf.open(QIODevice::ReadOnly) && pf.size() < 8192) {
        auto p = QJsonDocument::fromJson(pf.readAll()).object();
        if (profileValid(p))
            m_profile = p;
    }
    m_name = m_profile["nickname"].toString();
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connection);
    m_db.setDatabaseName(m_root + "/history.sqlite");
    if (!m_db.open())
        return;
    for (QString sql :
         {"CREATE TABLE IF NOT EXISTS messages(id TEXT PRIMARY KEY,peer TEXT,direction TEXT,text TEXT,time "
          "REAL)",
          "CREATE TABLE IF NOT EXISTS message_images(id TEXT PRIMARY KEY,path TEXT NOT NULL)",
          "CREATE INDEX IF NOT EXISTS messages_peer_time_id ON messages(peer,time,id)",
          "CREATE TABLE IF NOT EXISTS message_authors(id TEXT PRIMARY KEY,sender TEXT NOT NULL)",
          "CREATE TABLE IF NOT EXISTS known_peers(id TEXT PRIMARY KEY,data TEXT NOT NULL)",
          "CREATE TABLE IF NOT EXISTS groups(id TEXT PRIMARY KEY,name TEXT NOT NULL,members TEXT NOT NULL)",
          "CREATE TABLE IF NOT EXISTS message_transfers(id TEXT PRIMARY KEY,transfer_id TEXT NOT NULL UNIQUE)",
          "CREATE TABLE IF NOT EXISTS delivery_batches(message_id TEXT PRIMARY KEY,recipient_count INTEGER NOT NULL,created_at REAL NOT NULL)",
          "CREATE TABLE IF NOT EXISTS delivery_targets(message_id TEXT NOT NULL,recipient TEXT NOT NULL,name_snapshot TEXT NOT NULL,"
          "state TEXT NOT NULL,attempts INTEGER NOT NULL DEFAULT 0,last_error TEXT NOT NULL DEFAULT '',ack_at REAL,"
          "PRIMARY KEY(message_id,recipient))",
          "CREATE TABLE IF NOT EXISTS outbox(id TEXT,recipient TEXT,payload TEXT NOT NULL,attempts INTEGER "
          "DEFAULT 0,error TEXT DEFAULT '',network_mode TEXT NOT NULL DEFAULT 'lan',PRIMARY KEY(id,recipient))"})
        exec(m_db, sql);
    // Add a local routing column before networking, including existing queues.
    // Its index avoids scanning large dormant-mode payloads every retry tick.
    {
        QSqlQuery columns("PRAGMA table_info(outbox)", m_db);
        while (columns.next()) if (columns.value(1).toString() == "network_mode") m_queueScopeReady = true;
        columns.finish();
        if (!m_queueScopeReady && m_db.transaction()) {
            bool migrated = exec(m_db, "ALTER TABLE outbox ADD COLUMN network_mode TEXT NOT NULL DEFAULT 'lan'");
            if (migrated) {
                // Compatibility with early preview queues that stored only payload metadata.
                QSqlQuery pending("SELECT id,recipient,payload FROM outbox", m_db);
                migrated = pending.isActive();
                while (pending.next()) {
                    if (obj(pending.value(2).toString()).value("_network_mode").toString("lan") == "vpn")
                        migrated = exec(m_db, "UPDATE outbox SET network_mode='vpn' WHERE id=? AND recipient=?",
                            {pending.value(0),pending.value(1)}) && migrated;
                }
                pending.finish();
            }
            if (migrated && m_db.commit()) m_queueScopeReady = true;
            else m_db.rollback();
        }
        if (m_queueScopeReady)
            m_queueScopeReady = exec(m_db, "CREATE INDEX IF NOT EXISTS outbox_network_retry ON outbox(network_mode,attempts,id)");
    }
    {
        QSqlQuery q("SELECT id,data FROM known_peers", m_db);
        while (q.next()) {
            auto p = obj(q.value(1).toString());
            if (validId(q.value(0).toString()))
                m_peers[q.value(0).toString()] = p;
        }
    }
    {
        QSqlQuery q("SELECT id,name,members FROM groups", m_db);
        while (q.next())
            m_groups[q.value(0).toString()] = {{"id", q.value(0).toString()},
                                               {"name", q.value(1).toString()},
                                               {"members", arr(q.value(2).toString())}};
    }
    connect(&m_server, &QTcpServer::newConnection, this, &Node::incoming);
    connect(&m_broadcast, &QTimer::timeout, this, &Node::announce);
    connect(&m_retry, &QTimer::timeout, this, &Node::flush);
    connect(&m_networkWatch, &QTimer::timeout, this, &Node::checkNetworkAvailability);
    connect(&m_udp, &QUdpSocket::readyRead, this, [this] {
        while (m_udp.hasPendingDatagrams()) {
            if (m_udp.pendingDatagramSize() > 4096) {
                m_udp.readDatagram(nullptr, 0);
                continue;
            }
            QByteArray b(4096, Qt::Uninitialized);
            QHostAddress host;
            auto n = m_udp.readDatagram(b.data(), b.size(), &host);
            b.resize(int(n));
            auto p = QJsonDocument::fromJson(b).object();
            if (!m_networkPolicy.vpnOnly && p["protocol"] == "lanlink1" && addressAllowed(host))
                remember(p, host.toString());
        }
    });
}
Node::~Node() {
    stop();
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connection);
}
bool Node::validId(QString id) {
    return QRegularExpression("^[0-9a-f]{32}$").match(id).hasMatch();
}
bool Node::privateAddress(const QHostAddress &a) { return NetworkPolicy::privateAddress(a); }
bool Node::setNetworkPolicy(const NetworkPolicy &policy, QString *error) {
    QString problem;
    if (m_running || !m_transportSockets.isEmpty()) problem = QStringLiteral("请停止所有连接后再修改网络模式");
    else problem = policy.configurationError();
    if (error) *error = problem;
    if (!problem.isEmpty()) return false;
    m_networkPolicy = policy;
    m_lastNetworkError.clear();
    return true;
}
bool Node::sameNetworkScope(const QJsonObject &record) const {
    return record.value("network_mode").toString("lan") == networkMode();
}
bool Node::peerAllowed(const QJsonObject &peer) const {
    const auto port = peer["port"].toInt();
    return sameNetworkScope(peer) && port > 0 && port <= 65535 &&
        addressAllowed(QHostAddress(peer["host"].toString()));
}
void Node::trackSocket(QTcpSocket *socket) {
    if (m_transportSockets.contains(socket)) return;
    m_transportSockets.insert(socket);
    connect(socket, &QObject::destroyed, this, [this, socket] { m_transportSockets.remove(socket); });
}
bool Node::prepareOutgoingSocket(QTcpSocket *socket, const QHostAddress &address, quint16 port, QString *error) {
    QString problem;
    if (!port || address.isNull() || !addressAllowed(address))
        problem = m_networkPolicy.vpnOnly ? QStringLiteral("地址不在明确批准的 VPN 对端列表中")
                                         : QStringLiteral("仅允许数字形式的可信局域网地址");
    else if (!m_running)
        problem = QStringLiteral("本机监听尚未启动或已停止，请检查网络配置并重新启动应用");
    else problem = m_networkPolicy.availabilityError();
    if (problem.isEmpty() && m_networkPolicy.vpnOnly &&
        !socket->bind(m_networkPolicy.localAddress, 0))
        problem = QStringLiteral("无法绑定指定 VPN 本机地址：") + socket->errorString();
    if (error) *error = problem;
    if (!problem.isEmpty()) return false;
    trackSocket(socket);
    return true;
}
void Node::checkNetworkAvailability() {
    if (!m_running || !m_networkPolicy.vpnOnly) return;
    const auto error = m_networkPolicy.availabilityError();
    if (error.isEmpty()) return;
    m_lastNetworkError = error;
    QPointer<Node> alive(this);
    stop(); // Includes sockets handed to FileTransfer/ScreenService.
    if (!alive) return;
    fail(error + QStringLiteral("；连接已停止，请恢复 VPN 后重新启动应用"));
}
QJsonObject Node::networkDiagnostics() const {
    auto result = m_networkPolicy.diagnostics();
    result["listening"] = m_server.isListening();
    result["port"] = int(port());
    result["discovery_enabled"] = m_running && m_discoveryPort != 0;
    result["last_error"] = m_lastNetworkError;
    return result;
}
void Node::diagnoseAddress(QString host, quint16 port, Reply callback, int timeoutMs) {
    requestAddress(host, port, {{"type", "hello"}}, [callback](QJsonObject peer, QString error) {
        if (error.isEmpty() && (!validId(peer["id"].toString()) || peer["port"].toInt() < 1 || peer["port"].toInt() > 65535))
            error = QStringLiteral("TCP 可达，但对方未返回有效 LanLink 应答");
        callback({{"reachable", error.isEmpty()}, {"peer", peer},
            {"identity_verified", false}, {"note", QStringLiteral("仅检查 LanLink 协议可达性，不证明设备身份或 VPN 加密")}}, error);
    }, timeoutMs);
}
QByteArray Node::frame(QJsonObject o) {
    auto b = QJsonDocument(o).toJson(QJsonDocument::Compact);
    QByteArray result(4, Qt::Uninitialized);
    qToBigEndian<quint32>(quint32(b.size()), result.data());
    return result + b;
}
void Node::writeFrame(QTcpSocket *s, QJsonObject o) {
    s->write(frame(o));
}
void Node::fail(QString m) {
    emit event("error", {{"message", m}});
}
bool Node::start(quint16 p, quint16 dp) {
    if (m_running)
        return true;
    if (!m_db.isOpen() || !m_queueScopeReady) {
        fail("History database or network queue migration unavailable");
        return false;
    }
    const auto unavailable = m_networkPolicy.availabilityError();
    if (!unavailable.isEmpty()) { m_lastNetworkError = unavailable; fail(unavailable); return false; }
    const auto bind = m_networkPolicy.vpnOnly ? m_networkPolicy.localAddress : QHostAddress(QHostAddress::AnyIPv4);
    if (m_networkPolicy.vpnOnly && !p) { m_lastNetworkError = "VPN 模式需要固定非零 TCP 端口"; fail(m_lastNetworkError); return false; }
    if (!m_server.listen(bind, p)) {
        m_lastNetworkError = m_server.errorString();
        fail(m_lastNetworkError);
        return false;
    }
    m_discoveryPort = m_networkPolicy.vpnOnly ? 0 : dp;
    dp = m_discoveryPort;
    m_lastNetworkError.clear();
    m_running = true;
    if (dp && !m_udp.bind(QHostAddress::AnyIPv4, dp, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
        fail(m_udp.errorString());
    if (dp) m_broadcast.start(5000);
    if (m_networkPolicy.vpnOnly) m_networkWatch.start(1000);
    m_retry.start(5000);
    announce();
    QTimer::singleShot(0, this, &Node::flush);
    return true;
}
void Node::stop() {
    m_running = false;
    m_broadcast.stop();
    m_retry.stop();
    m_networkWatch.stop();
    m_udp.close();
    m_server.close();
    // Callbacks may destroy sockets or their service. Guard the whole snapshot.
    QList<QPointer<QTcpSocket>> sockets;
    for (auto socket : m_transportSockets) sockets.append(socket);
    for (const auto &socket : sockets) if (socket) socket->abort();
    m_inflight.clear();
}
QJsonObject Node::hello() const {
    return {{"id", m_id}, {"name", m_name}, {"port", int(port())},
            {"max_text_bytes", int(MaxTextBytes)}};
}
QJsonArray Node::peers() const {
    QJsonArray a;
    for (auto p : m_peers)
        if (peerAllowed(p)) a.append(p);
    return a;
}
QJsonArray Node::groups() const {
    QJsonArray a;
    for (auto p : m_groups)
        a.append(p);
    return a;
}
void Node::announce() {
    if (!m_running || !m_discoveryPort)
        return;
    auto h = hello();
    h["protocol"] = "lanlink1";
    m_udp.writeDatagram(QJsonDocument(h).toJson(QJsonDocument::Compact), QHostAddress::Broadcast,
                        m_discoveryPort);
}
bool Node::remember(QJsonObject p, QString host) {
    QString id = p["id"].toString();
    int pt = p["port"].toInt();
    if (!validId(id) || id == m_id || pt < 1 || pt > 65535 || !addressAllowed(QHostAddress(host)) ||
        (!m_peers.contains(id) && m_peers.size() >= 256))
        return false;
    const int maxText = qBound(0, p["max_text_bytes"].toInt(), int(MaxTextBytes));
    p = {{"id", id}, {"name", p["name"].toString().left(80)}, {"host", host}, {"port", pt},
         {"max_text_bytes", maxText}, {"network_mode", networkMode()}};
    m_peers[id] = p;
    exec(m_db, "INSERT OR REPLACE INTO known_peers VALUES(?,?)", {id, json(p)});
    QTimer::singleShot(0,this,[this,p]{emit event("peer",p);});
    return true;
}
void Node::request(QString id, QJsonObject p, Reply cb, int timeout) {
    if (!m_peers.contains(id)) {
        cb({}, "Unknown peer");
        return;
    }
    auto peer = m_peers[id];
    if (!peerAllowed(peer)) { cb({}, "联系人不属于当前网络模式或未获地址授权"); return; }
    requestAddress(peer["host"].toString(), quint16(peer["port"].toInt()), p, cb, timeout);
}
void Node::requestAddress(QString host, quint16 pt, QJsonObject p, Reply cb, int timeout) {
    QHostAddress address(host);
    p["sender"] = hello();
    if (frame(p).size() - 4 > MaxFrameBytes) {
        cb({}, "Message exceeds protocol frame limit");
        return;
    }
    auto s = new QTcpSocket(this);
    s->setReadBufferSize(MaxFrameBytes + 4);
    s->setProxy(QNetworkProxy::NoProxy);
    QString policyError;
    if (!prepareOutgoingSocket(s, address, pt, &policyError)) {
        s->deleteLater(); cb({}, policyError); return;
    }
    auto timer = new QTimer(s);
    timer->setSingleShot(true);
    auto done = std::make_shared<bool>(false);
    auto finish = [s, cb, done](QJsonObject o, QString e) {
        if (*done)
            return;
        *done = true;
        QPointer<QTcpSocket> safe(s);
        cb(o, e);
        // A callback may close its owner (and therefore destroy this socket).
        if(safe) safe->abort();
        if(safe) safe->deleteLater();
    };
    auto buffer = std::make_shared<QByteArray>();
    p["sender"] = hello();
    connect(s, &QTcpSocket::connected, s, [s, p] { writeFrame(s, p); });
    connect(s, &QTcpSocket::readyRead, s, [s, buffer, finish] {
        buffer->append(s->readAll());
        if (buffer->size() < 4)
            return;
        quint32 n = qFromBigEndian<quint32>(buffer->constData());
        if (n > 1048576) {
            finish({}, "Oversized response");
            return;
        }
        if (buffer->size() < 4 + n)
            return;
        QJsonParseError err;
        auto d = QJsonDocument::fromJson(buffer->mid(4, n), &err);
        if (err.error != QJsonParseError::NoError || !d.isObject()) {
            finish({}, "Invalid response");
            return;
        }
        finish(d.object(), {});
    });
    connect(s, &QTcpSocket::errorOccurred, s,
            [finish, s](QAbstractSocket::SocketError) { finish({}, s->errorString()); });
    connect(s, &QTcpSocket::disconnected, s, [finish] { finish({}, "Connection closed"); });
    connect(timer, &QTimer::timeout, s, [finish] { finish({}, "Request timeout"); });
    timer->start(timeout);
    s->connectToHost(address, pt);
}
void Node::addPeer(QString host, quint16 pt) {
    requestAddress(host, pt, {{"type", "hello"}}, [this, host](QJsonObject p, QString e) {
        if (!e.isEmpty())
            fail(e);
        else if (!remember(p, host))
            fail("Invalid peer");
        else
            flush();
    });
}
void Node::incoming() {
    while (m_server.hasPendingConnections()) {
        auto s = m_server.nextPendingConnection();
        s->setReadBufferSize(1048580);
        s->setParent(this);
        if (!addressAllowed(s->peerAddress()) || !m_networkPolicy.availabilityError().isEmpty() ||
            (m_networkPolicy.vpnOnly && s->localAddress() != m_networkPolicy.localAddress) || m_transportSockets.size() >= 64) {
            s->abort(); s->deleteLater();
            continue;
        }
        trackSocket(s);
        auto timer = new QTimer(s);
        timer->setSingleShot(true);
        timer->start(30000);
        connect(timer, &QTimer::timeout, s, [s] {
            s->abort();
            s->deleteLater();
        });
        auto b = std::make_shared<QByteArray>();
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(s, &QTcpSocket::readyRead, s, [this, s, b, conn, timer] {
            if (s->bytesAvailable() < 4)
                return;
            auto header = s->peek(4);
            quint32 n = qFromBigEndian<quint32>(header.constData());
            if (n > 1048576) {
                s->abort();
                s->deleteLater();
                return;
            }
            if (s->bytesAvailable() < n + 4)
                return;
            *b = s->read(n + 4);
            disconnect(*conn);
            timer->stop();
            QJsonParseError e;
            auto d = QJsonDocument::fromJson(b->mid(4, n), &e);
            if (e.error != QJsonParseError::NoError || !d.isObject()) {
                s->abort();
                s->deleteLater();
                return;
            } // requests never include raw bytes until the service replies
            handle(s, d.object(), s->peerAddress().toString());
        });
        connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
    }
}
void Node::handle(QTcpSocket *s, QJsonObject r, QString host) {
    QPointer<Node> owner(this);QPointer<QTcpSocket> socket(s);
    if(r.contains("sender")) {
        const bool remembered=remember(r["sender"].toObject(),host);
        if(!owner || !socket)return;
        if(!remembered) {
            writeFrame(s,{{"error","Invalid sender"}});
            if(socket)socket->disconnectFromHost();
            return;
        }
    }
    QString t = r["type"].toString();
    if (t == "hello")
        writeFrame(s, hello());
    else if (t == "profile_get")
        writeFrame(s, {{"id", m_id}, {"profile", m_profile}});
    else if (t == "message" || t == "group_message") {
        QString error;
        if (receiveChat(r, &error))
            writeFrame(s, {{"ack", r["id"]}});
        else
            writeFrame(s, {{"error", error}});
    } else if (serviceHandler && serviceHandler(s, r, host))
        return;
    else
        writeFrame(s, {{"error", "Unsupported request"}});
    s->disconnectFromHost();
}
QJsonArray Node::history(const QString &conversation, int limit) const {
    return historyPage(conversation, {}, limit).records;
}
HistoryPage Node::historyPage(const QString &conversation, HistoryCursor before, int limit) const {
    HistoryPage page;
    if(conversation.isEmpty()) return page;
    if(before.valid) {
        if(before.conversation!=conversation || before.id.isEmpty() || before.id.size()>256 ||
           before.id.contains(QChar(0)) || !std::isfinite(before.timestamp)) {
            page.error=QStringLiteral("历史游标无效，请重新打开会话"); return page;
        }
        QSqlQuery cursor(m_db);
        cursor.prepare("SELECT 1 FROM messages WHERE peer=? AND time=? AND id=?");
        cursor.addBindValue(conversation);cursor.addBindValue(before.timestamp);cursor.addBindValue(before.id);
        if(!cursor.exec() || !cursor.next()) {
            page.error=QStringLiteral("历史位置已失效，请重新打开会话");return page;
        }
    }
    limit=qBound(1,limit,1000);
    QSqlQuery q(m_db);
    QString sql="SELECT m.id,m.peer,m.direction,m.text,m.time,a.sender,i.path,t.transfer_id FROM messages m "
        "LEFT JOIN message_authors a ON a.id=m.id LEFT JOIN message_images i ON i.id=m.id "
        "LEFT JOIN message_transfers t ON t.id=m.id WHERE m.peer=? ";
    if(before.valid)sql+="AND (m.time<? OR (m.time=? AND m.id<?)) ";
    sql+="ORDER BY m.time DESC,m.id DESC LIMIT ?";
    q.prepare(sql);q.addBindValue(conversation);
    if(before.valid){q.addBindValue(before.timestamp);q.addBindValue(before.timestamp);q.addBindValue(before.id);}
    q.addBindValue(limit+1);
    if(!q.exec()){page.error=QStringLiteral("无法读取历史记录");return page;}
    while(q.next()) {
        if(page.records.size()==limit){page.hasMore=true;break;}
        QJsonObject record{{"id",q.value(0).toString()},{"peer_id",q.value(1).toString()},
            {"direction",q.value(2).toString()},{"text",q.value(3).toString()},
            {"timestamp",q.value(4).toDouble()},{"sender_id",q.value(5).toString()},
            {"image_path",q.value(6).toString()}};
        if(!q.value(7).isNull())record["transfer_id"]=q.value(7).toString();
        page.records.prepend(record);
    }
    if(!page.records.isEmpty()) {
        const auto oldest=page.records.first().toObject();
        page.nextBefore={conversation,oldest["id"].toString(),oldest["timestamp"].toDouble(),true};
    }
    return page;
}
QJsonObject Node::historyRecord(const QString &conversation,const QString &messageId) const {
    QSqlQuery q(m_db);
    q.prepare("SELECT m.id,m.peer,m.direction,m.text,m.time,a.sender,i.path,t.transfer_id FROM messages m "
        "LEFT JOIN message_authors a ON a.id=m.id LEFT JOIN message_images i ON i.id=m.id "
        "LEFT JOIN message_transfers t ON t.id=m.id WHERE m.peer=? AND m.id=?");
    q.addBindValue(conversation);q.addBindValue(messageId);
    if(!q.exec()||!q.next())return {};
    QJsonObject r{{"id",q.value(0).toString()},{"peer_id",q.value(1).toString()},
        {"direction",q.value(2).toString()},{"text",q.value(3).toString()},
        {"timestamp",q.value(4).toDouble()},{"sender_id",q.value(5).toString()},
        {"image_path",q.value(6).toString()}};
    if(!q.value(7).isNull())r["transfer_id"]=q.value(7).toString();return r;
}
QJsonArray Node::pendingMessages() const {
    QSqlQuery q("SELECT id,recipient,attempts,error,network_mode FROM outbox", m_db);
    QJsonArray a;
    while (q.next())
        a.append(QJsonObject{{"id", q.value(0).toString()},
                             {"recipient", q.value(1).toString()},
                             {"attempts", q.value(2).toInt()},
                             {"error", q.value(4).toString() == networkMode() ? q.value(3).toString() : QString("消息保留在原网络模式，不会跨模式重试")}});
    return a;
}
QJsonObject Node::fileGroupContext(const QString &conversation) const {
    if(!conversation.startsWith("group:"))return {};
    const auto group=m_groups.value(conversation.mid(6));
    if(group.isEmpty() || !group["members"].toArray().contains(m_id))return {};
    return {{"version",1},{"id",group["id"]},{"name",group["name"]},{"members",group["members"]}};
}
QJsonObject Node::normalizedGroupDeclaration(const QJsonObject &context,const QString &senderId,QString *error) const {
    const auto reject=[error](const QString &message){if(error)*error=message;return QJsonObject();};
    if(error)error->clear();
    const auto id=context["id"].toString(),name=context["name"].toString();
    const auto members=context["members"].toArray();
    if(!validId(senderId) || !validId(id) || !context["name"].isString() ||
       name.trimmed().isEmpty() || name.size()>80 || !context["members"].isArray() ||
       members.size()<2 || members.size()>64)return reject("Invalid group");
    for(auto character:name)if(character.unicode()<32 || character.unicode()==127)
        return reject("Invalid group");
    QStringList ids;
    for(const auto &member:members) {
        if(!member.isString() || !validId(member.toString()) || ids.contains(member.toString()))
            return reject("Invalid group members");
        ids.append(member.toString());
    }
    if(!ids.contains(senderId) || !ids.contains(m_id))return reject("Not a group member");
    std::sort(ids.begin(),ids.end());
    const auto normalized=QJsonArray::fromStringList(ids);
    if(m_groups.contains(id)) {
        QStringList known;
        for(const auto &value:m_groups.value(id)["members"].toArray())known.append(value.toString());
        std::sort(known.begin(),known.end());
        if(known!=ids)return reject("Group membership mismatch");
        // Neither a file offer nor a chat message may rename an existing group.
        return m_groups.value(id);
    }
    return {{"id",id},{"name",name},{"members",normalized}};
}
QString Node::acceptFileGroupContext(const QString &senderId,const QJsonObject &context,QString *error) {
    const auto reject=[error](const QString &message){if(error)*error=message;return QString();};
    if(error)error->clear();
    if(!context["version"].isDouble() || context["version"].toDouble()!=1)
        return reject("Unsupported file group context version");
    const auto group=normalizedGroupDeclaration(context,senderId,error);
    if(group.isEmpty())return {};
    const auto id=group["id"].toString();
    if(m_groups.contains(id))return "group:"+id;
    // TOFU declaration only, not proof of identity or member consent.
    if(!m_db.transaction())return reject("File group database busy");
    if(!exec(m_db,"INSERT INTO groups(id,name,members) VALUES(?,?,?)",
             {id,group["name"].toString(),json(group["members"])}) || !m_db.commit()) {
        m_db.rollback();return reject("File group persistence failed");
    }
    m_groups[id]=group;
    QTimer::singleShot(0,this,[this,group]{emit event("group",group);});
    return "group:"+id;
}
bool Node::recordTransferEvent(const QJsonObject &snapshot) {
    const auto transfer=snapshot["id"].toString();
    const auto peer=snapshot["peer_id"].toString();
    const auto conversation=snapshot["conversation_id"].toString(peer);
    const auto direction=snapshot["direction"].toString();
    const auto timestamp=snapshot["timestamp"].toDouble();
    const auto name=snapshot["name"].toString();
    const auto knownMembers=m_groups.value(conversation.mid(6))["members"].toArray();
    const bool group=conversation.startsWith("group:") && validId(conversation.mid(6)) &&
        m_groups.contains(conversation.mid(6)) && knownMembers.contains(m_id) && knownMembers.contains(peer);
    if(!validId(transfer) || !validId(peer) || (conversation!=peer && !group) ||
       (direction!="in" && direction!="out") || !(timestamp>0) ||
       !std::isfinite(timestamp) || name.isEmpty()) return false;
    const auto mid="file:"+transfer;
    QSqlQuery existing(m_db);
    existing.prepare("SELECT m.peer,m.direction FROM message_transfers t JOIN messages m ON m.id=t.id WHERE t.transfer_id=?");
    existing.addBindValue(transfer);
    if(!existing.exec())return false;
    if(existing.next()) return existing.value(0).toString()==conversation && existing.value(1).toString()==direction;
    if(!m_db.transaction())return false;
    bool ok=exec(m_db,"INSERT INTO messages(id,peer,direction,text,time) VALUES(?,?,?,?,?)",
                 {mid,conversation,direction,QStringLiteral("[文件] ")+name,timestamp});
    ok=exec(m_db,"INSERT INTO message_transfers(id,transfer_id) VALUES(?,?)",{mid,transfer})&&ok;
    ok=exec(m_db,"INSERT INTO message_authors(id,sender) VALUES(?,?)",
            {mid,direction=="out"?m_id:peer})&&ok;
    if(!ok || !m_db.commit()){m_db.rollback();return false;}
    emit event("local_file",{{"id",mid},{"transfer_id",transfer},{"peer_id",conversation}});
    return true;
}
bool Node::retryMessage(const QString &messageId) {
    if(!validId(messageId) || !m_db.transaction()) return false;
    QSqlQuery pending(m_db);
    pending.prepare("SELECT recipient FROM outbox WHERE id=?");
    pending.addBindValue(messageId);
    if(!pending.exec()) {m_db.rollback();return false;}
    QStringList recipients;
    while(pending.next()) recipients.push_back(pending.value(0).toString());
    pending.finish();
    if(recipients.isEmpty()) {m_db.rollback();return false;}
    // Do not race a pending callback: it may still ACK or reject this ID.
    for(const auto &recipient:recipients)
        if(m_inflight.contains(messageId+recipient)) {m_db.rollback();return false;}
    bool saved=exec(m_db,"UPDATE outbox SET error='' WHERE id=?",{messageId});
    // Only pending recipients change; already received targets retain proof.
    // Legacy messages have no ledger and remain Unknown.
    saved=exec(m_db,"UPDATE delivery_targets SET state='queued',last_error='',ack_at=NULL WHERE message_id=? AND recipient IN(SELECT recipient FROM outbox WHERE id=?)",
               {messageId,messageId}) && saved;
    if(!saved || !m_db.commit()) {m_db.rollback();return false;}
    emit event("delivery_changed",{{"id",messageId}});
    QTimer::singleShot(0,this,&Node::flush);
    return true;
}
QJsonObject Node::deliveryStatus(const QString &messageId) const {
    QJsonObject result{{"known",false},{"message_id",messageId},{"targets",QJsonArray()}};
    QSqlQuery batch(m_db);
    batch.prepare("SELECT recipient_count,created_at FROM delivery_batches WHERE message_id=?");
    batch.addBindValue(messageId);
    if(!batch.exec() || !batch.next()) return result;
    const int total=batch.value(0).toInt();
    if(total<=0)return result;
    QSqlQuery query(m_db);
    query.prepare("SELECT recipient,name_snapshot,state,attempts,last_error,ack_at FROM delivery_targets WHERE message_id=? ORDER BY recipient");
    query.addBindValue(messageId);
    if(!query.exec()) return result;
    QJsonArray targets;int received=0,queued=0,rejected=0;
    while(query.next()) {
        const auto state=query.value(2).toString();
        if(state=="received") { if(query.isNull(5))return result; ++received; }
        else if(state=="rejected") ++rejected;
        else if(state=="queued") ++queued;
        targets.append(QJsonObject{{"recipient",query.value(0).toString()},
            {"name",query.value(1).toString()},{"state",state},{"attempts",query.value(3).toInt()},
            {"error",query.value(4).toString()},
            {"ack_at",query.isNull(5)?QJsonValue(QJsonValue::Null):QJsonValue(query.value(5).toDouble())}});
    }
    // Missing/corrupt ledger rows are unknown, not evidence of receipt.
    if(targets.size()!=total || received+queued+rejected!=total) return result;
    result["known"]=true;result["total"]=total;result["received"]=received;
    result["queued"]=queued;result["rejected"]=rejected;result["targets"]=targets;
    result["created_at"]=batch.value(1).toDouble();
    return result;
}
bool Node::updateProfile(QJsonObject p) {
    if (!profileValid(p)) {
        fail("Invalid profile");
        return false;
    }
    for (auto key : {"nickname", "signature", "organization", "department", "location"})
        p[key] = p[key].toString().trimmed();
    QSaveFile f(m_root + "/profile.json");
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(p).toJson()) < 0 || !f.commit()) {
        fail("Profile save failed");
        return false;
    }
    m_profile = p;
    m_name = p["nickname"].toString();
    announce();
    emit event("profile_updated", {{"profile", p}});
    return true;
}
void Node::fetchProfile(QString id) {
    request(id, {{"type", "profile_get"}}, [this, id](QJsonObject r, QString e) {
        if (!e.isEmpty())
            fail(e);
        else if (r["id"] != id || !profileValid(r["profile"].toObject()))
            fail("Invalid profile response");
        else
            emit event("profile_received", {{"peer_id", id}, {"profile", r["profile"]}});
    });
}
QString Node::createGroup(QStringList members, QString name) {
    members.append(m_id);
    members.removeDuplicates();
    std::sort(members.begin(), members.end());
    if (members.size() < 2 || members.size() > 64) {
        fail("Group requires 2–64 members");
        return {};
    }
    for (auto id : members)
        if (!validId(id) || (id != m_id && !m_peers.contains(id))) {
            fail("Unknown group member");
            return {};
        }
    for(auto character:name)if(character.unicode()<32 || character.unicode()==127) {
        fail("Group name contains control characters");return {};
    }
    QString gid = uid();
    name = name.trimmed().left(80);
    if (name.isEmpty())
        name = QStringLiteral("群聊");
    QJsonObject g{{"id", gid}, {"name", name}, {"members", QJsonArray::fromStringList(members)}};
    if (!exec(m_db, "INSERT INTO groups VALUES(?,?,?)", {gid, name, json(g["members"])})) {
        fail("Group save failed");
        return {};
    }
    m_groups[gid] = g;
    emit event("group", g);
    return gid;
}
bool Node::save(QString mid, QString conversation, QString direction, QString text, QString author,
                QString image) {
    QSqlQuery q(m_db);
    q.prepare("INSERT OR IGNORE INTO messages VALUES(?,?,?,?,?)");
    for (auto v :
         QVariantList{mid, conversation, direction, text, QDateTime::currentMSecsSinceEpoch() / 1000.0})
        q.addBindValue(v);
    if (!q.exec() || !q.numRowsAffected())
        return false;
    if (!author.isEmpty() && !exec(m_db, "INSERT OR REPLACE INTO message_authors VALUES(?,?)", {mid, author}))
        return false;
    if (!image.isEmpty() && !exec(m_db, "INSERT OR REPLACE INTO message_images VALUES(?,?)", {mid, image}))
        return false;
    return true;
}
QString Node::storeImage(QJsonObject p, QString *error) {
    auto bad = [error](QString e) -> QString {
        *error = e;
        return {};
    };
    QString encoded = p["data"].toString(), hash = p["sha256"].toString();
    if (p["format"] != "jpeg" || encoded.size() > 699052 ||
        !QRegularExpression("^[0-9a-f]{64}$").match(hash).hasMatch())
        return bad("Invalid image payload");
    auto b = QByteArray::fromBase64(encoded.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (b.size() < 4 || b.size() > 512 * 1024 || !b.startsWith(QByteArray::fromHex("ffd8")) ||
        !b.endsWith(QByteArray::fromHex("ffd9")) ||
        QCryptographicHash::hash(b, QCryptographicHash::Sha256).toHex() != hash.toLatin1())
        return bad("Invalid image data");
    QBuffer buffer(&b);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "jpeg");
    auto sz = reader.size();
    if (!sz.isValid() || sz.width() > 1600 || sz.height() > 1200 || reader.read().isNull())
        return bad("Invalid image dimensions");
    QString dir = m_root + "/images";
    if (QFileInfo(dir).isSymLink())
        return bad("Unsafe image directory");
    QDir().mkpath(dir);
    QString path = dir + "/" + hash + ".jpg";
    if (QFileInfo(path).isSymLink())
        return bad("Unsafe image path");
    if (QFile::exists(path)) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly) || f.readAll() != b)
            return bad("Existing image differs");
        return path;
    }
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(b) != b.size() || !f.commit())
        return bad("Image write failed");
    return path;
}
QString Node::sendMessage(QString c, QString text) {
    return queue(c, text);
}
QString Node::sendImage(QString c, QByteArray b) {
    QJsonObject image{
        {"format", "jpeg"},
        {"sha256", QString::fromLatin1(QCryptographicHash::hash(b, QCryptographicHash::Sha256).toHex())},
        {"data", QString::fromLatin1(b.toBase64())}};
    return queue(c, QStringLiteral("[截图]"), image);
}
QString Node::queue(QString conversation, QString text, QJsonObject image) {
    if (text.trimmed().isEmpty()) {
        fail(QStringLiteral("消息不能为空"));
        return {};
    }
    if (text.toUtf8().size() > MaxTextBytes) {
        fail(QStringLiteral("消息超过 128 KiB（UTF-8）上限，请拆分发送或作为文件发送；草稿已保留"));
        return {};
    }
    QJsonObject g;
    QStringList recipients;
    if (conversation.startsWith("group:")) {
        g = m_groups.value(conversation.mid(6));
        if (g.isEmpty()) {
            fail("Unknown group");
            return {};
        }
        for (auto v : g["members"].toArray())
            if (v.toString() != m_id)
                recipients.append(v.toString());
    } else {
        if (!m_peers.contains(conversation)) {
            fail("Unknown peer");
            return {};
        }
        recipients.append(conversation);
    }
    for (const auto &id : recipients) {
        const auto peer = m_peers.value(id);
        const int limit = peer["max_text_bytes"].toInt();
        // Older Python/C++ releases have no capability field and reject >10000
        // characters. Conservatively count UTF-16 units for both old clients.
        if ((limit <= 0 && text.size() > 10000) ||
            (limit > 0 && text.toUtf8().size() > limit)) {
            fail(QStringLiteral("对方或群成员尚不支持此长度的消息，请升级双方程序或拆分发送；草稿已保留"));
            return {};
        }
    }
    QSqlQuery count("SELECT count(*) FROM outbox", m_db);
    count.next();
    if (count.value(0).toInt() + recipients.size() > 10000) {
        fail("Outbox full");
        return {};
    }
    QString mid = uid();
    QJsonObject p{{"type", g.isEmpty() ? "message" : "group_message"}, {"id", mid}, {"text", text}};
    QString imagePath;
    if (!image.isEmpty()) {
        QString error;
        imagePath = storeImage(image, &error);
        if (!error.isEmpty()) {
            fail(error);
            return {};
        }
        p["image"] = image;
    }
    if (!g.isEmpty()) {
        p["group"] = g;
        QJsonArray info;
        for (auto id : recipients)
            if (m_peers.contains(id))
                info.append(m_peers[id]);
        p["members_info"] = info;
    }
    // Bound the serialized packet too: control characters expand in JSON and
    // group metadata contributes to the wire size. Never persist an unsendable packet.
    auto packet = p;
    packet["sender"] = hello();
    if (frame(packet).size() - 4 > MaxFrameBytes) {
        fail(QStringLiteral("消息数据超过协议上限，请拆分发送；草稿已保留"));
        return {};
    }
    if (!m_db.transaction()) {
        fail("Database busy");
        return {};
    }
    bool ok = exec(m_db,"INSERT INTO delivery_batches(message_id,recipient_count,created_at) VALUES(?,?,?)",
                   {mid,recipients.size(),QDateTime::currentMSecsSinceEpoch()/1000.0});
    p["_network_mode"] = networkMode(); // Local queue routing metadata, stripped before transmission.
    for (auto id : recipients) {
        ok = exec(m_db, "INSERT INTO outbox(id,recipient,payload,network_mode) VALUES(?,?,?,?)", {mid, id, json(p), networkMode()}) && ok;
        ok = exec(m_db,"INSERT INTO delivery_targets(message_id,recipient,name_snapshot,state) VALUES(?,?,?,'queued')",
                  {mid,id,m_peers.value(id)["name"].toString(id)}) && ok;
    }
    ok = save(mid, conversation, "out", text, m_id, imagePath) && ok;
    if (!ok || !m_db.commit()) {
        m_db.rollback();
        fail("Message save failed");
        return {};
    }
    emit event("message", {{"id", mid},
                           {"peer_id", conversation},
                           {"direction", "out"},
                           {"text", text},
                           {"sender_id", m_id},
                           {"image_path", imagePath},
                           {"timestamp", QDateTime::currentMSecsSinceEpoch() / 1000.0},
                           {"status", "queued"}});
    emit event("queued", {{"id", mid}, {"peer_id", conversation}, {"recipients", recipients.size()}});
    QTimer::singleShot(0, this, &Node::flush);
    return mid;
}
void Node::flush() {
    if (!m_running)
        return;
    // Filter before LIMIT: dormant VPN jobs cannot starve current LAN jobs (or vice versa).
    QSqlQuery q(m_db);
    q.prepare("SELECT id,recipient,payload FROM outbox WHERE network_mode=? AND COALESCE(error,'') NOT LIKE 'rejected:%' ORDER BY attempts,id LIMIT 100");
    q.addBindValue(networkMode());
    if (!q.exec()) { fail("Pending message queue unavailable"); return; }
    while (q.next()) {
        QString mid = q.value(0).toString(), id = q.value(1).toString(), key = mid + id;
        if (m_inflight.contains(key) || m_inflight.size() >= 16) continue;
        auto payload = obj(q.value(2).toString());
        if (payload.value("_network_mode").toString("lan") != networkMode()) continue;
        payload.remove("_network_mode");
        m_inflight.insert(key);
        request(id, payload, [this, mid, id, key](QJsonObject r, QString e) {
            m_inflight.remove(key);
            const bool acknowledged=e.isEmpty() && r["ack"]==mid;
            const bool rejected=!acknowledged && QStringList{
                "Invalid message","Invalid group","Invalid group members",
                "Not a group member","Group membership mismatch","Invalid sender"
            }.contains(r["error"].toString());
            const QString reason=acknowledged?QString():(
                e.isEmpty()?r["error"].toString("Missing acknowledgement").left(200):e.left(200));
            const QString storedError=rejected?"rejected:"+reason:reason;
            if(!m_db.transaction()) {
                emit event("delivery_status_error",{{"id",mid},{"recipient",id},{"message","Database busy; acknowledgement not saved"}});
                return;
            }
            bool saved;
            if(acknowledged) {
                // Persist proof and remove retry row atomically. An old message
                // without a ledger remains Unknown even if this retry now ACKs.
                saved=exec(m_db,"UPDATE delivery_targets SET state='received',attempts=attempts+1,last_error='',ack_at=? WHERE message_id=? AND recipient=?",
                           {QDateTime::currentMSecsSinceEpoch()/1000.0,mid,id});
                QSqlQuery proof(m_db);
                proof.prepare("SELECT NOT EXISTS(SELECT 1 FROM delivery_batches WHERE message_id=?) OR EXISTS(SELECT 1 FROM delivery_targets WHERE message_id=? AND recipient=? AND state='received' AND ack_at IS NOT NULL)");
                proof.addBindValue(mid);proof.addBindValue(mid);proof.addBindValue(id);
                saved=proof.exec() && proof.next() && proof.value(0).toBool() && saved;
                saved=exec(m_db,"DELETE FROM outbox WHERE id=? AND recipient=?",{mid,id}) && saved;
            } else {
                saved=exec(m_db,"UPDATE delivery_targets SET state=?,attempts=attempts+1,last_error=? WHERE message_id=? AND recipient=?",
                           {rejected?"rejected":"queued",reason,mid,id});
                saved=exec(m_db,"UPDATE outbox SET attempts=attempts+1,error=? WHERE id=? AND recipient=?",
                           {storedError,mid,id}) && saved;
            }
            if(!saved || !m_db.commit()) {
                m_db.rollback();
                emit event("delivery_status_error",{{"id",mid},{"recipient",id},{"message","Receipt save failed; retry retained"}});
                return;
            }
            emit event("delivery_changed",{{"id",mid},{"recipient",id}});
            if(acknowledged) emit event("delivered",{{"id",mid},{"recipient",id}});
            else if(rejected)
                fail(QStringLiteral("对方拒收消息：%1。已保留历史，不再自动重试；请确认双方版本后重新发送。").arg(reason));
        });
    }
}
bool Node::receiveChat(QJsonObject r, QString *error) {
    auto bad = [error](QString s) {
        *error = s;
        return false;
    };
    QString mid = r["id"].toString(), text = r["text"].toString(),
            sender = r["sender"].toObject()["id"].toString();
    if (!validId(mid) || !validId(sender) || !r["text"].isString() || text.trimmed().isEmpty() ||
        text.toUtf8().size() > MaxTextBytes)
        return bad("Invalid message");
    QString conv = sender;
    QJsonObject g;
    if (r["type"] == "group_message") {
        g=normalizedGroupDeclaration(r["group"].toObject(),sender,error);
        if(g.isEmpty())return false;
        conv="group:"+g["id"].toString();
    }
    QString path;
    if (r.contains("image")) {
        path = storeImage(r["image"].toObject(), error);
        if (path.isEmpty())
            return false;
        text = QStringLiteral("[截图]");
    }
    QSqlQuery duplicate(m_db);
    duplicate.prepare("SELECT id FROM messages WHERE id=?");
    duplicate.addBindValue(mid);
    duplicate.exec();
    if (duplicate.next())
        return true;
    if (!m_db.transaction())
        return bad("Database busy");
    bool ok = true;
    if (!g.isEmpty())
        ok = exec(m_db, "INSERT OR IGNORE INTO groups VALUES(?,?,?)",
                  {g["id"].toString(), g["name"].toString(), json(g["members"])});
    ok = save(mid, conv, "in", text, sender, path) && ok;
    if (!ok || !m_db.commit()) {
        m_db.rollback();
        return bad("Message persistence failed");
    }
    if(!g.isEmpty())m_groups[g["id"].toString()]=g;
    const auto membersInfo=r["members_info"].toArray();
    const QJsonObject message{{"id",mid},{"peer_id",conv},{"sender_id",sender},{"text",text},
        {"direction","in"},{"image_path",path},{"timestamp",QDateTime::currentMSecsSinceEpoch()/1000.0}};
    // The protocol ACK completes after persistence, before UI callbacks. A
    // callback can destroy the owning Node; never resume receive/handle then.
    QTimer::singleShot(0,this,[this,g,membersInfo,message] {
        QPointer<Node> alive(this);
        if(!g.isEmpty()) {
            for(const auto &value:membersInfo) {
                const auto peer=value.toObject();
                if(g["members"].toArray().contains(peer["id"])) {
                    remember(peer,peer["host"].toString());
                    if(!alive)return;
                }
            }
            emit event("group",g);
            if(!alive)return;
        }
        emit event("message",message);
    });
    return true;
}
