#include "filetransfer.h"
#include "node.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <QUuid>
#include <QtConcurrentRun>
#include <QtEndian>
#include <memory>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace {
QString token() {
    return QUuid::createUuid().toString(QUuid::Id128);
}
bool publishNoReplace(const QString &from, const QString &to) {
#ifdef Q_OS_WIN
    return CreateHardLinkW(reinterpret_cast<LPCWSTR>(to.utf16()), reinterpret_cast<LPCWSTR>(from.utf16()),
                           nullptr) != 0;
#else
    return ::link(QFile::encodeName(from).constData(), QFile::encodeName(to).constData()) == 0;
#endif
}
} // namespace
FileTransfer::FileTransfer(Node *n, QObject *p) : QObject(p), m_node(n) {
    QFile f(n->root() + "/cpp-transfers.json");
    if (f.open(QIODevice::ReadOnly)) {
        auto o = QJsonDocument::fromJson(f.readAll()).object();
        m_incoming = o["incoming"].toObject();
        m_outgoing = o["outgoing"].toObject();
    }
    prune();
}
FileTransfer::~FileTransfer() {
    m_stopping = true;
    auto sockets = m_active;
    for (auto s : sockets)
        if (s) {
            QObject::disconnect(s, nullptr, this, nullptr);
            s->abort();
        }
    m_active.clear();
}
void FileTransfer::persist() {
    QSaveFile f(m_node->root() + "/cpp-transfers.json");
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(QJsonObject{{"incoming", m_incoming}, {"outgoing", m_outgoing}}).toJson());
        f.commit();
    }
}
void FileTransfer::prune() {
    const double now = QDateTime::currentSecsSinceEpoch();
    for (auto k : m_outgoing.keys()) {
        auto o = m_outgoing[k].toObject();
        if (!o.contains("share_token") || now - o["created"].toDouble() > 604800 ||
            !o["share_token"].toString().isEmpty())
            m_outgoing.remove(k);
    }
    persist();
}
QJsonObject FileTransfer::peer(QString id) const {
    for (auto p : m_node->peers())
        if (p.toObject()["id"].toString() == id)
            return p.toObject();
    return {};
}
bool FileTransfer::safeName(QString n) {
    static const QRegularExpression reserved("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])($|\\.)",
                                             QRegularExpression::CaseInsensitiveOption);
    if (n.isEmpty() || n.size() > 200 || n == "." || n == ".." || n.endsWith(' ') || n.endsWith('.') ||
        reserved.match(n).hasMatch())
        return false;
    for (auto c : n)
        if (c.unicode() < 32 || QString("/\\:<>\"|?*").contains(c))
            return false;
    return true;
}
QByteArray FileTransfer::sha256(QString p) {
    QFile f(p);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!h.addData(&f))
        return {};
    return h.result().toHex();
}
void FileTransfer::hashAsync(QString path, std::function<void(QByteArray)> cb) {
    if (m_hashJobs >= 4) {
        QTimer::singleShot(0, this, [cb]() { cb({}); });
        return;
    }
    ++m_hashJobs;
    auto watcher = new QFutureWatcher<QByteArray>(this);
    connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [this, watcher, cb]() {
        --m_hashJobs;
        auto value = watcher->result();
        watcher->deleteLater();
        cb(value);
    });
    watcher->setFuture(QtConcurrent::run([path]() { return sha256(path); }));
}
void FileTransfer::metadataAsync(QString host, QString path, QString st,
                                 std::function<void(QJsonObject)> cb) {
    QFileInfo fi(path);
    if (!fi.isFile() || fi.isSymLink() || fi.size() > (1LL << 34) || !safeName(fi.fileName())) {
        QTimer::singleShot(0, this, [cb]() { cb({}); });
        return;
    }
    hashAsync(path, [this, host, path, st, cb](QByteArray hash) {
        if (!st.isEmpty() && !m_shares.contains(st)) {
            cb({});
            return;
        }
        cb(metadata(host, path, st, hash));
    });
}
QJsonObject FileTransfer::metadata(QString host, QString path, QString shareToken, QByteArray hash) {
    // Remove expired ordinary tasks, but preserve currently published share tickets.
    const double now = QDateTime::currentSecsSinceEpoch();
    for (auto k : m_outgoing.keys())
        if (now - m_outgoing[k].toObject()["created"].toDouble() > 604800)
            release(k);
    QFileInfo fi(path);
    if (!fi.isFile() || fi.isSymLink() || fi.size() > (1LL << 34) || !safeName(fi.fileName()) ||
        m_outgoing.size() >= 64)
        return {};
    if (hash.isEmpty())
        return {};
    QString id = token();
    QJsonObject o{
        {"type", "offer"},       {"sender", m_node->hello()}, {"id", id},
        {"name", fi.fileName()}, {"size", double(fi.size())}, {"sha256", QString::fromLatin1(hash)}};
    m_outgoing[id] =
        QJsonObject{{"path", fi.canonicalFilePath()},      {"host", host},   {"size", double(fi.size())},
                    {"sha256", QString::fromLatin1(hash)}, {"created", now}, {"share_token", shareToken}};
    persist();
    return o;
}
void FileTransfer::offerFile(QString pid, QString path) {
    auto p = peer(pid);
    if (p.isEmpty()) {
        emit error("联系人不可达");
        return;
    }
    metadataAsync(p["host"].toString(), path, {}, [this, pid](QJsonObject r) {
        if (r.isEmpty()) {
            emit error("无法发送：文件无效或待发任务过多");
            return;
        }
        QString id = r["id"].toString();
        QPointer<FileTransfer> guard(this);
        m_node->request(pid, r, [this, guard, id](QJsonObject r, QString e) {
            if (!guard || guard->m_stopping)
                return;
            if (!e.isEmpty() || !r["ok"].toBool()) {
                release(id);
                emit error("发送邀请失败：" + e);
            } else
                emit progress(id, 0, m_outgoing[id].toObject()["size"].toDouble(), "等待接收");
        });
    });
}

bool FileTransfer::registerOffer(QJsonObject r, bool notify) {
    QString id = r["id"].toString();
    auto p = peer(r["sender"].toObject()["id"].toString());
    double sz = r["size"].toDouble(-1);
    if (!Node::validId(id) || !safeName(r["name"].toString()) || sz < 0 || sz > (1LL << 34) ||
        sz != qint64(sz) || !QRegularExpression("^[a-f0-9]{64}$").match(r["sha256"].toString()).hasMatch() ||
        p.isEmpty() || m_incoming.size() >= 64 || m_incoming.contains(id))
        return false;
    r["peer"] = p;
    m_incoming[id] = r;
    persist();
    if (notify)
        emit offerReceived(r);
    return true;
}
QJsonArray FileTransfer::incomingOffers() const {
    QJsonArray a;
    for (auto v : m_incoming)
        a.append(v);
    return a;
}
void FileTransfer::reply(QTcpSocket *s, QJsonObject o) {
    Node::writeFrame(s, o);
    s->disconnectFromHost();
}
bool FileTransfer::handle(QTcpSocket *s, QJsonObject r, QString host) {
    auto t = r["type"].toString();
    if (t == "offer") {
        reply(s, {{"ok", registerOffer(r)}});
        return true;
    }
    if (t == "fetch") {
        serveFetch(s, r, host);
        return true;
    }
    if (t == "file_release") {
        auto id = r["id"].toString();
        if (m_outgoing[id].toObject()["host"].toString() == host)
            release(id);
        reply(s, {{"ok", true}});
        return true;
    }
    if (t == "share_list" || t == "share_get") {
        serveShare(s, r, host);
        return true;
    }
    return false;
}
void FileTransfer::release(QString id) {
    m_outgoing.remove(id);
    persist();
    if (m_active.value(id))
        m_active[id]->abort();
}
void FileTransfer::pauseFile(QString id) {
    if (m_active.value(id))
        m_active[id]->abort();
}
void FileTransfer::notifyRelease(QJsonObject item) {
    m_node->request(item["sender"].toObject()["id"].toString(),
                    {{"type", "file_release"}, {"id", item["id"]}}, [](QJsonObject, QString) {});
}
void FileTransfer::rejectFile(QString id) {
    auto item = m_incoming.take(id).toObject();
    persist();
    if (!item.isEmpty())
        notifyRelease(item);
}
void FileTransfer::serveFetch(QTcpSocket *s, QJsonObject r, QString host, QByteArray hash) {
    QString id = r["id"].toString();
    auto item = m_outgoing[id].toObject();
    QString st = item["share_token"].toString();
    double off = r["offset"].toDouble(-1);
    qint64 size = qint64(item["size"].toDouble());
    if (item.isEmpty() || item["host"].toString() != host || (!st.isEmpty() && !m_shares.contains(st)) ||
        off < 0 || off > size || off != qint64(off)) {
        reply(s, {{"ok", false}, {"error", "Offer unavailable"}});
        return;
    }
    if (hash.isEmpty()) {
        QPointer<QTcpSocket> safe(s);
        hashAsync(item["path"].toString(), [this, safe, r, host](QByteArray h) {
            if (!safe)
                return;
            if (h.isEmpty()) {
                reply(safe, {{"ok", false}, {"error", "Source unavailable"}});
                return;
            }
            serveFetch(safe, r, host, h);
        });
        return;
    }
    auto f = std::make_shared<QFile>(item["path"].toString());
    if (!f->open(QIODevice::ReadOnly) || f->size() != size ||
        QString::fromLatin1(hash) != item["sha256"].toString()) {
        reply(s, {{"ok", false}, {"error", "Source changed"}});
        return;
    }
    f->seek(qint64(off));
    m_active[id] = s;
    auto timeout = new QTimer(s);
    timeout->setSingleShot(true);
    timeout->setInterval(30000);
    connect(timeout, &QTimer::timeout, s, &QTcpSocket::abort);
    connect(s, &QTcpSocket::bytesWritten, timeout, [timeout](qint64) { timeout->start(); });
    timeout->start();
    Node::writeFrame(s, {{"ok", true}});
    auto pump = std::make_shared<std::function<void()>>();
    QPointer<QTcpSocket> safe(s);
    *pump = [this, safe, f, id, size]() {
        if (!safe)
            return;
        if (!m_outgoing.contains(id)) {
            safe->abort();
            return;
        }
        while (safe->bytesToWrite() < 262144 && !f->atEnd()) {
            auto b = f->read(65536);
            if (b.isEmpty()) {
                safe->abort();
                return;
            }
            safe->write(b);
            emit progress(id, f->pos(), size, "发送中");
        }
        if (f->atEnd() && safe->bytesToWrite() == 0) {
            emit progress(id, size, size, "已发送，等待对方校验");
            safe->disconnectFromHost();
        }
    };
    connect(s, &QTcpSocket::bytesWritten, this, [pump](qint64) {
        auto fn = *pump;
        if (fn)
            fn();
    });
    connect(s, &QTcpSocket::disconnected, this, [this, id, pump]() {
        m_active.remove(id);
        *pump = {};
    });
    auto fn = *pump;
    if (fn)
        fn();
}
void FileTransfer::acceptFile(QString id, QString dir) {
    if (m_active.value(id) || m_verifying.contains(id) || !m_incoming.contains(id))
        return;
    auto item = m_incoming[id].toObject();
    QFileInfo di(dir);
    QString name = item["name"].toString();
    if (!di.isDir() || !safeName(name)) {
        emit error("保存目录无效");
        return;
    }
    dir = di.canonicalFilePath();
    QString dest = dir + "/" + name, part = dir + "/.lanlink-" + id + ".part";
    if (QFileInfo::exists(dest) || QFileInfo(dest).isSymLink() || QFileInfo(part).isSymLink()) {
        emit error("目标已存在或部分文件不安全");
        return;
    }
    auto file = std::make_shared<QFile>(part);
    qint64 total = qint64(item["size"].toDouble());
    if (!file->open(QIODevice::ReadWrite) || file->size() > total) {
        emit error("无法打开部分文件");
        return;
    }
    file->seek(file->size());
    auto p = peer(item["sender"].toObject()["id"].toString());
    if (p.isEmpty())
        p = item["peer"].toObject();
    QHostAddress host(p["host"].toString());
    if (!Node::privateAddress(host)) {
        emit error("仅限可信局域网地址");
        return;
    }
    auto s = new QTcpSocket(this);
    s->setProxy(QNetworkProxy::NoProxy);
    s->setReadBufferSize(1048580);
    m_active[id] = s;
    auto timer = new QTimer(s);
    timer->setSingleShot(true);
    timer->setInterval(30000);
    timer->start();
    auto header = std::make_shared<QByteArray>();
    auto accepted = std::make_shared<bool>(false);
    auto done = std::make_shared<bool>(false);
    connect(s, &QTcpSocket::connected, s, [s, id, file]() {
        Node::writeFrame(s, {{"type", "fetch"}, {"id", id}, {"offset", double(file->pos())}});
    });
    auto finish = [this, s, file, item, id, part, dest, total, done]() {
        if (*done)
            return;
        *done = true;
        file->flush();
        file->close();
        m_verifying.insert(id);
        emit progress(id, total, total, "校验中");
        QPointer<QTcpSocket> safe(s);
        hashAsync(part, [this, safe, item, id, part, dest, total](QByteArray hash) {
            m_verifying.remove(id);
            if (QString::fromLatin1(hash) != item["sha256"].toString()) {
                QFile::remove(part);
                emit error("SHA256 校验失败，请重试");
                emit progress(id, 0, total, "校验失败");
                if (safe)
                    safe->abort();
                return;
            }
            if (!publishNoReplace(part, dest)) {
                emit error("无法安全发布文件：目标已存在或文件系统不支持硬链接，部分文件保留");
                emit progress(id, total, total, "发布失败，可重试");
                if (safe)
                    safe->abort();
                return;
            }
            QFile::remove(part);
            m_incoming.remove(id);
            persist();
            notifyRelease(item);
            emit progress(id, total, total, "完成");
            emit completed(id, dest);
            if (safe)
                safe->disconnectFromHost();
        });
    };
    connect(
        s, &QTcpSocket::readyRead, this, [this, s, file, header, accepted, done, timer, id, total, finish]() {
            timer->start();
            if (!*accepted) {
                header->append(s->readAll());
                if (header->size() < 4)
                    return;
                quint32 n = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(header->constData()));
                if (n > 1048576) {
                    s->abort();
                    return;
                }
                if (header->size() < qsizetype(n + 4))
                    return;
                auto r = QJsonDocument::fromJson(header->mid(4, n)).object();
                if (!r["ok"].toBool()) {
                    emit error(r["error"].toString("文件邀请不可用"));
                    s->abort();
                    return;
                }
                auto remainder = header->mid(n + 4);
                header->clear();
                *accepted = true;
                if (remainder.size() > total - file->pos() || file->write(remainder) != remainder.size()) {
                    s->abort();
                    return;
                }
            }
            while (s->bytesAvailable() > 0) {
                auto b = s->read(qMin<qint64>(65536, total - file->pos()));
                if (b.isEmpty() || file->write(b) != b.size()) {
                    s->abort();
                    return;
                }
                emit progress(id, file->pos(), total, "接收中");
                if (file->pos() == total)
                    break;
            }
            if (file->pos() == total)
                finish();
        });
    auto cleaned = std::make_shared<bool>(false);
    auto cleanup = [this, s, file, id, total, done, timer, cleaned]() {
        if (*cleaned)
            return;
        *cleaned = true;
        timer->stop();
        qint64 n = file->isOpen() ? file->pos() : total;
        file->close();
        m_active.remove(id);
        if (!*done)
            emit progress(id, n, total, "已暂停，可续传");
        s->deleteLater();
    };
    connect(s, &QTcpSocket::disconnected, this, cleanup);
    connect(s, &QTcpSocket::errorOccurred, this, [s, cleanup](QAbstractSocket::SocketError) {
        s->abort();
        cleanup();
    });
    connect(timer, &QTimer::timeout, this, [s, cleanup]() {
        s->abort();
        cleanup();
    });
    s->connectToHost(host, quint16(p["port"].toInt()));
}
QString FileTransfer::publishShare(QString path) {
    QFileInfo f(path);
    if (!f.isDir() || f.isSymLink()) {
        emit error("请选择普通目录");
        return {};
    }
    QString t = token() + token();
    m_shares[t] = f.canonicalFilePath();
    return t;
}
void FileTransfer::revokeShare(QString t) {
    m_shares.remove(t);
    for (auto id : m_outgoing.keys())
        if (m_outgoing[id].toObject()["share_token"].toString() == t)
            release(id);
}
QString FileTransfer::sharePath(QString t, QString rel) const {
    if (!m_shares.contains(t) || rel.contains('\\') || rel.contains(':') || rel.contains(QChar(0)) ||
        QDir::isAbsolutePath(rel))
        return {};
    QString root = m_shares[t].toString(), p = root;
    for (auto part : rel.split('/', Qt::SkipEmptyParts)) {
        if (part == "..")
            return {};
        if (part == ".")
            continue;
        p += "/" + part;
        QFileInfo f(p);
        if (f.isSymLink())
            return {};
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
        if (f.isJunction())
            return {};
#endif
    }
    QString canonical = QFileInfo(p).canonicalFilePath();
    if (canonical.isEmpty() || (canonical != root && !canonical.startsWith(root + "/")))
        return {};
    return canonical;
}
void FileTransfer::serveShare(QTcpSocket *s, QJsonObject r, QString host) {
    QString t = r["token"].toString(), p = sharePath(t, r["path"].toString());
    if (p.isEmpty()) {
        reply(s, {{"ok", false}, {"error", "Share unavailable or invalid path"}});
        return;
    }
    if (r["type"].toString() == "share_list") {
        if (!QFileInfo(p).isDir()) {
            reply(s, {{"ok", false}});
            return;
        }
        QJsonArray a;
        for (auto f : QDir(p).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
            if (f.isSymLink())
                continue;
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
            if (f.isJunction())
                continue;
#endif
            a.append(QJsonObject{{"name", f.fileName()},
                                 {"directory", f.isDir()},
                                 {"size", double(f.isFile() ? f.size() : 0)}});
            if (a.size() > 2000) {
                reply(s, {{"ok", false}, {"error", "Too many entries"}});
                return;
            }
        }
        reply(s, {{"ok", true}, {"items", a}});
    } else {
        QPointer<QTcpSocket> safe(s);
        metadataAsync(host, p, t, [this, safe](QJsonObject o) {
            if (safe)
                reply(safe, {{"ok", !o.isEmpty()}, {"offer", o}});
            else if (!o.isEmpty())
                release(o["id"].toString());
        });
    }
}
void FileTransfer::listShare(QString pid, QString t, QString rel) {
    QPointer<FileTransfer> guard(this);
    m_node->request(pid, {{"type", "share_list"}, {"token", t}, {"path", rel}},
                    [this, guard](QJsonObject r, QString e) {
                        if (!guard || guard->m_stopping)
                            return;
                        if (!e.isEmpty() || !r["ok"].toBool())
                            emit error("无法查看共享：" + e);
                        else
                            emit shareListed(r["items"].toArray());
                    });
}
void FileTransfer::downloadShare(QString pid, QString t, QString rel, QString dir) {
    QPointer<FileTransfer> guard(this);
    m_node->request(pid, {{"type", "share_get"}, {"token", t}, {"path", rel}},
                    [this, guard, dir](QJsonObject r, QString e) {
                        if (!guard || guard->m_stopping)
                            return;
                        auto offer = r["offer"].toObject();
                        if (!e.isEmpty() || !r["ok"].toBool() || !registerOffer(offer, false)) {
                            emit error("无法取得共享文件：" + e);
                            return;
                        }
                        acceptFile(offer["id"].toString(), dir);
                    });
}
