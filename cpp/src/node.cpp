#include "node.h"
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
          "CREATE TABLE IF NOT EXISTS message_authors(id TEXT PRIMARY KEY,sender TEXT NOT NULL)",
          "CREATE TABLE IF NOT EXISTS known_peers(id TEXT PRIMARY KEY,data TEXT NOT NULL)",
          "CREATE TABLE IF NOT EXISTS groups(id TEXT PRIMARY KEY,name TEXT NOT NULL,members TEXT NOT NULL)",
          "CREATE TABLE IF NOT EXISTS outbox(id TEXT,recipient TEXT,payload TEXT NOT NULL,attempts INTEGER "
          "DEFAULT 0,error TEXT DEFAULT '',PRIMARY KEY(id,recipient))"})
        exec(m_db, sql);
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
            if (p["protocol"] == "lanlink1" && privateAddress(host))
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
bool Node::privateAddress(const QHostAddress &a) {
    if (a.isLoopback())
        return true;
    return a.isInSubnet(QHostAddress("10.0.0.0"), 8) || a.isInSubnet(QHostAddress("172.16.0.0"), 12) ||
           a.isInSubnet(QHostAddress("192.168.0.0"), 16) || a.isInSubnet(QHostAddress("169.254.0.0"), 16) ||
           a.isInSubnet(QHostAddress("fc00::"), 7) || a.isInSubnet(QHostAddress("fe80::"), 10);
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
    if (!m_db.isOpen()) {
        fail("History database unavailable");
        return false;
    }
    if (!m_server.listen(QHostAddress::AnyIPv4, p)) {
        fail(m_server.errorString());
        return false;
    }
    m_discoveryPort = dp;
    m_running = true;
    if (dp && !m_udp.bind(QHostAddress::AnyIPv4, dp, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
        fail(m_udp.errorString());
    m_broadcast.start(5000);
    m_retry.start(5000);
    announce();
    QTimer::singleShot(0, this, &Node::flush);
    return true;
}
void Node::stop() {
    m_running = false;
    m_broadcast.stop();
    m_retry.stop();
    m_udp.close();
    m_server.close();
    for (auto s : findChildren<QTcpSocket *>())
        s->abort();
    m_inflight.clear();
}
QJsonObject Node::hello() const {
    return {{"id", m_id}, {"name", m_name}, {"port", int(port())}};
}
QJsonArray Node::peers() const {
    QJsonArray a;
    for (auto p : m_peers)
        a.append(p);
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
    if (!validId(id) || id == m_id || pt < 1 || pt > 65535 || !privateAddress(QHostAddress(host)) ||
        (!m_peers.contains(id) && m_peers.size() >= 256))
        return false;
    p = {{"id", id}, {"name", p["name"].toString().left(80)}, {"host", host}, {"port", pt}};
    m_peers[id] = p;
    exec(m_db, "INSERT OR REPLACE INTO known_peers VALUES(?,?)", {id, json(p)});
    emit event("peer", p);
    return true;
}
void Node::request(QString id, QJsonObject p, Reply cb, int timeout) {
    if (!m_peers.contains(id)) {
        cb({}, "Unknown peer");
        return;
    }
    auto peer = m_peers[id];
    requestAddress(peer["host"].toString(), quint16(peer["port"].toInt()), p, cb, timeout);
}
void Node::requestAddress(QString host, quint16 pt, QJsonObject p, Reply cb, int timeout) {
    QHostAddress address(host);
    if (!pt || address.isNull() || !privateAddress(address)) {
        cb({}, "Trusted numeric LAN addresses only");
        return;
    }
    auto s = new QTcpSocket(this);
    s->setReadBufferSize(1048580);
    s->setProxy(QNetworkProxy::NoProxy);
    auto timer = new QTimer(s);
    timer->setSingleShot(true);
    auto done = std::make_shared<bool>(false);
    auto finish = [s, cb, done](QJsonObject o, QString e) {
        if (*done)
            return;
        *done = true;
        cb(o, e);
        s->abort();
        s->deleteLater();
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
        if (!privateAddress(s->peerAddress()) || findChildren<QTcpSocket *>().size() > 64) {
            s->deleteLater();
            continue;
        }
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
    if (r.contains("sender") && !remember(r["sender"].toObject(), host)) {
        writeFrame(s, {{"error", "Invalid sender"}});
        s->disconnectFromHost();
        return;
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
    QSqlQuery q(m_db);
    q.prepare("SELECT m.id,m.peer,m.direction,m.text,m.time,a.sender,i.path FROM messages m LEFT JOIN "
              "message_authors a ON a.id=m.id LEFT JOIN message_images i ON i.id=m.id WHERE m.peer=? ORDER "
              "BY m.time DESC LIMIT ?");
    q.addBindValue(conversation);
    q.addBindValue(qBound(1, limit, 1000));
    q.exec();
    QJsonArray a;
    while (q.next())
        a.prepend(QJsonObject{{"id", q.value(0).toString()},
                              {"peer_id", q.value(1).toString()},
                              {"direction", q.value(2).toString()},
                              {"text", q.value(3).toString()},
                              {"timestamp", q.value(4).toDouble()},
                              {"sender_id", q.value(5).toString()},
                              {"image_path", q.value(6).toString()}});
    return a;
}
QJsonArray Node::pendingMessages() const {
    QSqlQuery q("SELECT id,recipient,attempts,error FROM outbox", m_db);
    QJsonArray a;
    while (q.next())
        a.append(QJsonObject{{"id", q.value(0).toString()},
                             {"recipient", q.value(1).toString()},
                             {"attempts", q.value(2).toInt()},
                             {"error", q.value(3).toString()}});
    return a;
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
    if (text.trimmed().isEmpty() || text.size() > 10000) {
        fail("Message length must be 1–10000");
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
    if (!m_db.transaction()) {
        fail("Database busy");
        return {};
    }
    bool ok = true;
    for (auto id : recipients)
        ok = exec(m_db, "INSERT INTO outbox(id,recipient,payload) VALUES(?,?,?)", {mid, id, json(p)}) && ok;
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
    QSqlQuery q("SELECT id,recipient,payload FROM outbox ORDER BY attempts,id LIMIT 100", m_db);
    while (q.next()) {
        QString mid = q.value(0).toString(), id = q.value(1).toString(), key = mid + id;
        if (m_inflight.contains(key) || m_inflight.size() >= 16)
            continue;
        m_inflight.insert(key);
        request(id, obj(q.value(2).toString()), [this, mid, id, key](QJsonObject r, QString e) {
            m_inflight.remove(key);
            if (e.isEmpty() && r["ack"] == mid) {
                exec(m_db, "DELETE FROM outbox WHERE id=? AND recipient=?", {mid, id});
                emit event("delivered", {{"id", mid}, {"recipient", id}});
            } else
                exec(m_db, "UPDATE outbox SET attempts=attempts+1,error=? WHERE id=? AND recipient=?",
                     {e.isEmpty() ? QString("Missing acknowledgement") : e.left(200), mid, id});
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
        text.size() > 10000)
        return bad("Invalid message");
    QString conv = sender;
    QJsonObject g;
    if (r["type"] == "group_message") {
        g = r["group"].toObject();
        auto members = g["members"].toArray();
        QString gid = g["id"].toString(), name = g["name"].toString();
        if (!validId(gid) || !g["name"].isString() || name.size() > 80 || members.size() < 2 ||
            members.size() > 64)
            return bad("Invalid group");
        QStringList ids;
        for (auto v : members) {
            if (!v.isString() || !validId(v.toString()) || ids.contains(v.toString()))
                return bad("Invalid group members");
            ids.append(v.toString());
        }
        if (!ids.contains(m_id) || !ids.contains(sender))
            return bad("Not a group member");
        std::sort(ids.begin(), ids.end());
        g["members"] = QJsonArray::fromStringList(ids);
        if (m_groups.contains(gid) && m_groups[gid]["members"] != g["members"])
            return bad("Group membership mismatch");
        conv = "group:" + gid;
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
    if (!g.isEmpty()) {
        m_groups[g["id"].toString()] = g;
        for (auto v : r["members_info"].toArray()) {
            auto p = v.toObject();
            if (g["members"].toArray().contains(p["id"]))
                remember(p, p["host"].toString());
        }
        emit event("group", g);
    }
    emit event("message", {{"id", mid},
                           {"peer_id", conv},
                           {"sender_id", sender},
                           {"text", text},
                           {"direction", "in"},
                           {"image_path", path},
                           {"timestamp", QDateTime::currentMSecsSinceEpoch() / 1000.0}});
    return true;
}
