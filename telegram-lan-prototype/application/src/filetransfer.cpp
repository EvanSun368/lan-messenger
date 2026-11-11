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
FileTransfer::FileTransfer(Node *n, QObject *p) : QObject(p), m_node(n), m_storagePath(n->root()+"/cpp-transfers.json") {
    QFile f(m_storagePath);
    if (f.open(QIODevice::ReadOnly)) {
        auto o = QJsonDocument::fromJson(f.readAll()).object();
        m_incoming = o["incoming"].toObject();
        m_outgoing = o["outgoing"].toObject();
        m_records = o["records"].toObject();
    }
    m_recordTimer = new QTimer(this); m_recordTimer->setSingleShot(true);m_recordTimer->setInterval(250);
    connect(m_recordTimer,&QTimer::timeout,this,&FileTransfer::persist);
    connect(this,&FileTransfer::progress,this,&FileTransfer::recordProgress);
    connect(this,&FileTransfer::completed,this,[this](QString id,QString path){
        updateRecord(id,{{"state","completed"},{"status","已接收并校验"},{"path",path}});
    });
    for(const auto id:m_records.keys()) {
        auto record=m_records[id].toObject();const auto state=record["state"].toString();
        updateRecord(id,{},false); // No active socket/hash lock survives process restart.
        if(state=="completed" || state=="validating") {
            updateRecord(id,{{"state","validating"},{"status","正在检查本地文件"}},false);
            const auto path=record["path"].toString();
            hashAsync(path,[this,id,record](QByteArray hash){
                const bool valid=!QFileInfo(record["path"].toString()).isSymLink() && !hash.isEmpty() && hash==record["sha256"].toString().toLatin1()
                    && QFileInfo(record["path"].toString()).size()==qint64(record["total"].toDouble());
                updateRecord(id,{{"state",valid?"completed":"missing"},{"status",valid?"已接收并校验":"本地文件已丢失或变更"}});
            });
        } else if(state=="running" || state=="verifying") {
            updateRecord(id,{{"state","paused"},{"status","上次传输未完成，可重试"}},false);
        }
    }
    prune();
    auto resultTimer=new QTimer(this);resultTimer->setInterval(5000);
    connect(resultTimer,&QTimer::timeout,this,&FileTransfer::sendResults);
    resultTimer->start();
    QTimer::singleShot(0,this,&FileTransfer::sendResults);
}
FileTransfer::~FileTransfer() {
    m_stopping = true;
    if(m_recordTimer && m_recordTimer->isActive()) persist();
    auto sockets = m_active;
    for (auto s : sockets)
        if (s) {
            QObject::disconnect(s, nullptr, this, nullptr);
            s->abort();
        }
    m_active.clear();
}
bool FileTransfer::persist() {
    QSaveFile file(m_storagePath);
    QString detail;
    if(!file.open(QIODevice::WriteOnly)) detail=file.errorString();
    else {
        const auto data=QJsonDocument(QJsonObject{{"incoming",m_incoming},{"outgoing",m_outgoing},{"records",m_records}}).toJson();
        if(file.write(data)!=data.size()) {detail=file.errorString();file.cancelWriting();}
        else if(file.commit()) {m_persistFailure.clear();return true;}
        else detail=file.errorString();
    }
    if(detail.isEmpty())detail="未知 I/O 错误";
    const auto message=QString("传输状态无法保存，重启后记录可能丢失：%1").arg(detail);
    if(m_persistFailure!=message) {
        m_persistFailure=message;qWarning().noquote()<<message;
        // Don't enter a modal UI error handler in the middle of a state mutation.
        QTimer::singleShot(0,this,[this,message]{if(m_persistFailure==message)emit error(message);});
    }
    return false;
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
    if (m_stopping)
        return;
    // Bound queued work without confusing temporary saturation with corruption.
    if (m_hashQueue.size() >= 128) {
        QTimer::singleShot(0, this, [cb]() { cb({}); });
        return;
    }
    m_hashQueue.enqueue({path, std::move(cb)});
    runNextHash();
}
void FileTransfer::runNextHash() {
    while (!m_stopping && m_hashJobs < 4 && !m_hashQueue.isEmpty()) {
        auto job = m_hashQueue.dequeue();
        ++m_hashJobs;
        auto watcher = new QFutureWatcher<QByteArray>(this);
        connect(watcher, &QFutureWatcher<QByteArray>::finished, this,
                [this, watcher, cb = std::move(job.second)]() {
                    --m_hashJobs;
                    auto value = watcher->result();
                    watcher->deleteLater();
                    QPointer<FileTransfer> guard(this);
                    cb(value);
                    if (guard) runNextHash();
                });
        watcher->setFuture(QtConcurrent::run([path = job.first]() { return sha256(path); }));
    }
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
        {"type", "offer"}, {"result_version",1}, {"sender", m_node->hello()}, {"id", id},
        {"name", fi.fileName()}, {"size", double(fi.size())}, {"sha256", QString::fromLatin1(hash)}};
    if(!shareToken.isEmpty())o.remove("result_version"); // Legacy share tickets have no sender timeline record.
    m_outgoing[id] =
        QJsonObject{{"path", fi.canonicalFilePath()},      {"host", host},   {"size", double(fi.size())},
                    {"sha256", QString::fromLatin1(hash)}, {"created", now}, {"share_token", shareToken}, {"network_mode", m_node->networkMode()}};
    persist();
    return o;
}
void FileTransfer::offerFile(QString pid, QString path, QString conversationId) {
    auto p = peer(pid);
    if (p.isEmpty()) {
        emit error("联系人不可达");
        return;
    }
    QJsonObject groupContext;
    if(conversationId.startsWith("group:")) {
        groupContext=m_node->fileGroupContext(conversationId);
        if(groupContext.isEmpty() || !groupContext["members"].toArray().contains(pid)) {
            emit error("群文件发送失败：本地群信息无效或接收设备不是群成员");return;
        }
    }
    metadataAsync(p["host"].toString(), path, {}, [this, pid, path, conversationId, groupContext](QJsonObject r) {
        if (r.isEmpty()) {
            emit error("无法发送：文件无效或待发任务过多");
            return;
        }
        if(!groupContext.isEmpty())r["group_context"]=groupContext;
        QString id = r["id"].toString();
        recordTransfer(id,r,pid,"out",path,conversationId);
        QPointer<FileTransfer> guard(this);
        m_node->request(pid, r, [this, guard, id, conversationId, groupContext](QJsonObject r, QString e) {
            if (!guard || guard->m_stopping)
                return;
            if(transferSnapshot(id)["state"]=="remote_completed" || transferSnapshot(id)["state"]=="remote_rejected")return;
            if (!e.isEmpty() || !r["ok"].toBool()) {
                if(m_active.value(id)) {emit error("重复邀请未获确认；正在进行的传输保持不变");return;}
                release(id);
                updateRecord(id,{{"state","failed"},{"status","发送邀请失败"}});
                emit error("发送邀请失败：" + (e.isEmpty()?r["error"].toString():e));
            } else {
                updateRecord(id,{{"remote_result_supported",r["result_version"].isDouble() && r["result_version"].toDouble()==1}});
                if(!groupContext.isEmpty()) {
                    const bool supported=r["group_context_version"].isDouble() && r["group_context_version"].toDouble()==1
                        && r["conversation_id"].isString() && r["conversation_id"].toString()==conversationId;
                    updateRecord(id,{{"remote_group_context_supported",supported},{"remote_group_route",supported?"group":"unconfirmed"}});
                }
                emit progress(id, 0, m_outgoing[id].toObject()["size"].toDouble(), "等待接收");
            }
        });
    });
}

bool FileTransfer::registerOffer(QJsonObject r, bool notify, QString *error) {
    if(error)*error="Invalid file offer";
    QString id = r["id"].toString();
    auto p = peer(r["sender"].toObject()["id"].toString());
    double sz = r["size"].toDouble(-1);
    if (r["type"]!="offer" || !Node::validId(id) || !safeName(r["name"].toString()) || sz < 0 || sz > (1LL << 34) ||
        sz != qint64(sz) || !QRegularExpression("^[a-f0-9]{64}$").match(r["sha256"].toString()).hasMatch() ||
        p.isEmpty() || m_incoming.size() >= 64 || m_incoming.contains(id) || m_records.contains(id) || m_outgoing.contains(id))
        return false;
    QString conversation=p["id"].toString();
    if(r.contains("group_context")) {
        if(!r["group_context"].isObject()) {if(error)*error="Invalid group context";return false;}
        QPointer<FileTransfer> owner(this);QPointer<Node> node(m_node);
        conversation=m_node->acceptFileGroupContext(p["id"].toString(),r["group_context"].toObject(),error);
        if(!owner || !node)return false;
        if(conversation.isEmpty())return false; // Never silently downgrade an invalid context.
    }
    r["peer"] = p;
    r["network_mode"] = m_node->networkMode(); // Local metadata, never trust a peer-supplied scope.
    m_incoming[id] = r;
    recordTransfer(id,r,p["id"].toString(),"in",{},conversation);
    if(error)error->clear();
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
        // A UI offer handler may enter a modal event loop. Acknowledge first and
        // defer notification so a peer timeout cannot delete s under handle().
        QString error;QPointer<FileTransfer> owner(this);QPointer<QTcpSocket> socket(s);
        const bool accepted=registerOffer(r,false,&error);
        if(!owner || !socket)return true;
        reply(s, {{"ok", accepted},{"error",error},
            {"conversation_id",accepted?transferSnapshot(r["id"].toString())["conversation_id"]:QJsonValue()},
            {"result_version",1},{"group_context_version",accepted && r.contains("group_context")?1:0}});
        if(accepted) QTimer::singleShot(0,this,[this,id=r["id"].toString()] {
            if(m_incoming.contains(id)) emit offerReceived(m_incoming[id].toObject());
        });
        return true;
    }
    if (t == "fetch") {
        serveFetch(s, r, host);
        return true;
    }
    if(t=="file_result") {
        const auto id=r["id"].toString();const auto old=m_records[id].toObject();
        const auto pid=r["sender"].toObject()["id"].toString();
        const auto sequence=r["sequence"].toDouble(-1);
        const auto state=r["state"].toString();
        const auto previous=old["remote_result_sequence"].toDouble(0);
        const bool terminal=old["remote_result"]=="completed" || old["remote_result"]=="rejected";
        bool valid=m_node->sameNetworkScope(old) && r["version"].isDouble() && r["version"].toDouble()==1
            && old["direction"]=="out" && pid==old["peer_id"].toString()
            && !old["peer_host"].toString().isEmpty() && old["peer_host"].toString()==host
            && r["size"].isDouble() && r["size"]==old["total"]
            && r["sha256"].isString() && r["sha256"]==old["sha256"]
            && r["sequence"].isDouble() && sequence>=1 && sequence<=9007199254740991.0 && sequence==qint64(sequence)
            && QStringList{"completed","rejected","failed","paused"}.contains(state)
            && sequence>=previous && (!terminal || (sequence==previous && state==old["remote_result"].toString()))
            && (sequence!=previous || state==old["remote_result"].toString());
        if(valid) {
            auto next=old;next["remote_result"]=state;next["remote_result_sequence"]=sequence;
            next["remote_result_at"]=QDateTime::currentMSecsSinceEpoch()/1000.0;
            next["state"]="remote_"+state;next["can_retry"]=!m_active.value(id) && (state=="failed" || state=="paused");
            next["status"]=state=="completed"?"对方已接收并校验":state=="rejected"?"对方已拒绝":state=="paused"?"对方已暂停":"对方接收失败，可重试";
            if(state=="completed")next["done"]=old["total"];
            m_records[id]=next;
            valid=persist();
            if(!valid)m_records[id]=old;
            else notifyRecord(id);
        }
        reply(s,{{"ok",valid},{"version",1},{"id",id},{"sequence",sequence}});
        // ACK goes out before closing a still-active sending socket.
        if(valid && (state=="completed" || state=="rejected"))release(id);
        return true;
    }
    if (t == "file_release") {
        auto id = r["id"].toString();
        if (m_node->sameNetworkScope(m_outgoing[id].toObject()) && m_outgoing[id].toObject()["host"].toString() == host)
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
    QPointer<FileTransfer> owner(this);
    const auto snapshot=transferSnapshot(id);
    if(snapshot["direction"]=="out" && m_active.value(id))
        emit progress(id,qint64(snapshot["done"].toDouble()),qint64(snapshot["total"].toDouble()),"已暂停，等待对方续传");
    if(!owner)return;
    if (m_verifying.contains(id)) {
        m_cancelledVerification.insert(id);
        const auto total=qint64(m_incoming[id].toObject()["size"].toDouble());
        emit progress(id,total,total,"已暂停，可续传");
    }
    if(!owner)return;
    if (m_active.value(id))
        m_active[id]->abort();
}
void FileTransfer::notifyRelease(QJsonObject item) {
    if (!m_node->sameNetworkScope(item)) return;
    m_node->request(item["sender"].toObject()["id"].toString(),
                    {{"type", "file_release"}, {"id", item["id"]}}, [](QJsonObject, QString) {});
}
void FileTransfer::rejectFile(QString id) {
    QPointer<FileTransfer> owner(this);
    pauseFile(id);
    if(!owner)return;
    auto item = m_incoming.take(id).toObject();
    auto record=m_records[id].toObject();
    if(record.isEmpty() || item.isEmpty())return;
    record["state"]="rejected";record["status"]="已拒绝";record["can_retry"]=false;
    m_records[id]=record;
    if(item["result_version"].isDouble() && item["result_version"].toDouble()==1)queueResult(id,"rejected");
    else {persist();notifyRelease(item);}
    notifyRecord(id);
}
void FileTransfer::serveFetch(QTcpSocket *s, QJsonObject r, QString host, QByteArray hash) {
    QString id = r["id"].toString();
    auto item = m_outgoing[id].toObject();
    QString st = item["share_token"].toString();
    double off = r["offset"].toDouble(-1);
    qint64 size = qint64(item["size"].toDouble());
    if (item.isEmpty() || !m_node->sameNetworkScope(item) || item["host"].toString() != host || (!st.isEmpty() && !m_shares.contains(st)) ||
        off < 0 || off > size || off != qint64(off)) {
        reply(s, {{"ok", false}, {"error", "Offer unavailable"}});
        return;
    }
    if (m_active.value(id) && m_active.value(id) != s) {
        reply(s, {{"ok", false}, {"error", "Transfer already active"}});
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
    QPointer<FileTransfer> owner(this);
    *pump = [this, owner, safe, f, id, size]() {
        if (!owner || !safe || safe->state() != QAbstractSocket::ConnectedState)
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
            if (safe->write(b) != b.size()) { safe->abort(); return; }
            emit progress(id, f->pos(), size, "发送中");
            if (!owner || !safe || safe->state() != QAbstractSocket::ConnectedState) return;
        }
        if (f->atEnd() && safe->bytesToWrite() == 0) {
            emit progress(id, size, size, "已发送，等待对方校验");
            if (safe) safe->disconnectFromHost();
        }
    };
    connect(s, &QTcpSocket::bytesWritten, this, [pump](qint64) {
        auto fn = *pump;
        if (fn)
            fn();
    });
    connect(s, &QTcpSocket::disconnected, this, [this, id, pump, safe]() {
        if (m_active.value(id) == safe) {m_active.remove(id);updateRecord(id,{});}
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
    if (!m_node->sameNetworkScope(item)) { emit error("文件邀请属于其他网络模式，未建立连接"); return; }
    QFileInfo di(dir);
    QString name = item["name"].toString();
    if (!di.isDir() || !safeName(name)) {
        emit error("保存目录无效");
        return;
    }
    dir = di.canonicalFilePath();
    updateRecord(id,{{"save_dir",dir}});
    QString dest = dir + "/" + name, part = dir + "/.lanlink-" + id + ".part";
    if (QFileInfo::exists(dest) || QFileInfo(dest).isSymLink() || QFileInfo(part).isSymLink()) {
        updateRecord(id,{{"state","failed"},{"status","目标已存在或部分文件不安全"}});
        emit error("目标已存在或部分文件不安全");
        return;
    }
    auto file = std::make_shared<QFile>(part);
    qint64 total = qint64(item["size"].toDouble());
    if (!file->open(QIODevice::ReadWrite) || file->size() > total) {
        updateRecord(id,{{"state","failed"},{"status","无法打开部分文件"}});
        emit error("无法打开部分文件");
        return;
    }
    file->seek(file->size());
    auto p = peer(item["sender"].toObject()["id"].toString());
    if (p.isEmpty())
        p = item["peer"].toObject();
    QHostAddress host(p["host"].toString());
    if (!m_node->peerAllowed(p)) {
        emit error("文件来源不属于当前网络模式或未获地址授权");
        return;
    }
    auto s = new QTcpSocket(this);
    s->setProxy(QNetworkProxy::NoProxy);
    QString policyError;
    if (!m_node->prepareOutgoingSocket(s, host, quint16(p["port"].toInt()), &policyError)) {
        s->deleteLater();
        updateRecord(id,{{"state","failed"},{"status",policyError}});
        emit error(policyError); return;
    }
    s->setReadBufferSize(1048580);
    m_active[id] = s;
    updateRecord(id,{{"state","running"},{"status","正在连接"}});
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
        QPointer<FileTransfer> owner(this);QPointer<QTcpSocket> safe(s);
        emit progress(id, total, total, "校验中");
        if(!owner)return;
        hashAsync(part, [this, safe, item, id, part, dest, total](QByteArray hash) {
            m_verifying.remove(id);
            // A user pause/reject while hashing must not publish the file later.
            if (m_cancelledVerification.remove(id) || !m_incoming.contains(id)) {
                updateRecord(id,{});
                return;
            }
            // Empty hash is an I/O/busy failure, not evidence of bad contents.
            // Keep the fully downloaded part so verification can be retried.
            if (hash.isEmpty()) {
                emit error("暂时无法校验文件，部分文件已保留，请重试");
                emit progress(id, total, total, "待校验，可重试");
                if (safe)
                    safe->abort();
                return;
            }
            if (QString::fromLatin1(hash) != item["sha256"].toString()) {
                QFile::remove(part);
                emit error("SHA256 校验失败，请重试");
                emit progress(id, total, total, "校验失败");
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
            // Commit local publication and pending receipt together in one atomic snapshot.
            auto record=m_records[id].toObject();record["state"]="completed";
            record["status"]="已接收并校验";record["path"]=dest;record["done"]=double(total);
            record["can_retry"]=false;m_records[id]=record;
            if(item["result_version"].isDouble() && item["result_version"].toDouble()==1)queueResult(id,"completed");
            else {persist();notifyRelease(item);}
            notifyRecord(id);
            QPointer<FileTransfer> owner(this);
            emit progress(id, total, total, "完成");
            if(!owner)return;
            emit completed(id, dest);
            if(!owner)return;
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
                    updateRecord(id,{{"state","failed"},{"status",r["error"].toString("文件邀请不可用")}});
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
        metadataAsync(host, p, t, [this, safe, r, p](QJsonObject o) {
            if(!o.isEmpty()) recordTransfer(o["id"].toString(),o,r["sender"].toObject()["id"].toString(),"out",p);
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
                    [this, guard, pid, t, rel](QJsonObject r, QString e) {
                        if (!guard || guard->m_stopping)
                            return;
                        if (!e.isEmpty() || !r["ok"].toBool())
                            emit error("无法查看共享：" + e);
                        else {
                            const auto entries=r["items"].toArray();
                            emit shareListedFor(pid,t,rel,entries);
                            if(guard) emit shareListed(entries);
                        }
                    });
}
void FileTransfer::downloadShare(QString pid, QString t, QString rel, QString dir) {
    QPointer<FileTransfer> guard(this);
    m_node->request(pid, {{"type", "share_get"}, {"token", t}, {"path", rel}},
                    [this, guard, dir, pid](QJsonObject r, QString e) {
                        if (!guard || guard->m_stopping)
                            return;
                        auto offer = r["offer"].toObject();
                        if (!e.isEmpty() || !r["ok"].toBool() || offer["type"]!="offer" ||
                            offer["sender"].toObject()["id"].toString()!=pid || offer.contains("group_context") ||
                            !registerOffer(offer, false)) {
                            if(guard) emit error("无法取得共享文件：" + e);
                            return;
                        }
                        if(!guard)return;
                        acceptFile(offer["id"].toString(), dir);
                    });
}

QJsonObject FileTransfer::transferSnapshot(QString id) const {
    auto record=m_records[id].toObject();
    if(!record.isEmpty()) record["persistence_error"]=m_persistFailure;
    return record;
}
QJsonArray FileTransfer::transferSnapshots() const {
    QJsonArray result; for(const auto id:m_records.keys()) result.append(transferSnapshot(id)); return result;
}
void FileTransfer::recordTransfer(QString id,QJsonObject offer,QString peerId,QString direction,QString path,QString conversationId) {
    if(m_records.contains(id)) return;
    m_records[id]=QJsonObject{{"id",id},{"network_mode",m_node->networkMode()},{"peer_id",peerId},{"peer_host",peer(peerId)["host"]},{"direction",direction},
        {"conversation_id",conversationId.isEmpty()?peerId:conversationId},{"name",offer["name"]},{"total",offer["size"]},{"done",0},{"state","waiting"},
        {"status",direction=="in"?"等待接收":"等待对方接收"},{"path",path},{"sha256",offer["sha256"]},
        {"result_version",offer["result_version"]},{"timestamp",QDateTime::currentMSecsSinceEpoch()/1000.0}};
    persist();notifyRecord(id);
}
void FileTransfer::updateRecord(QString id,QJsonObject fields,bool durable) {
    if(!m_records.contains(id)) return;
    auto record=m_records[id].toObject();for(auto it=fields.begin();it!=fields.end();++it) record[it.key()]=it.value();
    record["can_retry"]=record["state"]!="remote_completed" && record["state"]!="remote_rejected" && !m_verifying.contains(id) && !m_active.value(id);
    m_records[id]=record;
    if(durable)persist();else if(m_recordTimer && !m_recordTimer->isActive())m_recordTimer->start();
    notifyRecord(id);
}
void FileTransfer::recordProgress(QString id,qint64 done,qint64 total,QString status) {
    if(!m_records.contains(id))return;
    QString state;
    if(status=="发送中" || status=="接收中")state="running";
    else if(status=="校验中" || status=="完成")state="verifying";
    else if(status.contains("失败") || status.contains("待校验"))state="failed";
    else if(status.contains("暂停"))state="paused";
    else if(status.startsWith("已发送"))state="sent";
    else state="waiting";
    const auto old=m_records[id].toObject();
    if(state=="paused" && old["state"]=="failed") {state="failed";status=old["status"].toString();}
    if(old["state"]=="completed" || old["state"]=="rejected" || old["state"]=="remote_completed" || old["state"]=="remote_rejected")return;
    if(old["state"].toString().startsWith("remote_") && state!="running")return;
    // Failure/cancellation keeps the last real byte position for diagnosis.
    if(state=="failed" || state=="paused") done=qMax(done,qint64(old["done"].toDouble()));
    updateRecord(id,{{"done",double(qBound<qint64>(0,done,total))},{"total",double(total)},
        {"state",state},{"status",status}},state!="running");
    if(old["direction"]=="in" && (state=="failed" || state=="paused"))queueResult(id,state);
}
void FileTransfer::retryFile(QString id) {
    const auto record=transferSnapshot(id);
    if (!m_node->sameNetworkScope(record)) { emit error("请回到文件任务原来的网络模式后重试"); return; }
    if(record["state"]=="remote_completed" || record["state"]=="remote_rejected")return;
    if(record["direction"].toString()=="in") {
        if(!m_incoming.contains(id)) {emit error("文件邀请已失效，请重新请求");return;}
        if(record["save_dir"].toString().isEmpty()) {emit error("请先选择保存目录");return;}
        acceptFile(id,record["save_dir"].toString());
    } else if(!record.isEmpty()) offerFile(record["peer_id"].toString(),record["path"].toString(),record["conversation_id"].toString());
}

void FileTransfer::notifyRecord(QString id) {
    if(m_recordNotificationsPending.contains(id))return;
    m_recordNotificationsPending.insert(id);
    QTimer::singleShot(0,this,[this,id] {
        m_recordNotificationsPending.remove(id);
        // Last operation: a UI subscriber may synchronously destroy the service.
        emit transferChanged(transferSnapshot(id));
    });
}

// Results are durable, retryable declarations by the LAN peer, not authenticated receipts.
void FileTransfer::queueResult(QString id,QString state) {
    auto record=m_records[id].toObject();
    if(record["direction"]!="in" || !record["result_version"].isDouble() || record["result_version"].toDouble()!=1)return;
    if(record["result_state"]=="completed" || record["result_state"]=="rejected")return;
    record["result_sequence"]=record["result_sequence"].toDouble(0)+1;
    record["result_state"]=state;record["result_pending"]=true;m_records[id]=record;
    persist();
    QTimer::singleShot(0,this,&FileTransfer::sendResults);
}
void FileTransfer::sendResults() {
    // Never acknowledge a result whose pending state could not be saved.
    if(!persist())return;
    for(const auto id:m_records.keys()) {
        const auto record=m_records[id].toObject();
        if(!m_node->sameNetworkScope(record) || !record["result_pending"].toBool() || m_resultSending.contains(id) || peer(record["peer_id"].toString()).isEmpty())continue;
        const auto seq=record["result_sequence"];
        m_resultSending.insert(id);
        QPointer<FileTransfer> guard(this);
        m_node->request(record["peer_id"].toString(),{{"type","file_result"},{"version",1},
            {"id",id},{"sender",m_node->hello()},{"size",record["total"]},{"sha256",record["sha256"]},
            {"state",record["result_state"]},{"sequence",seq}},[this,guard,id,seq](QJsonObject ack,QString error){
            if(!guard)return;
            m_resultSending.remove(id);
            if(error.isEmpty() && ack["ok"]==true && ack["version"].isDouble() && ack["version"].toDouble()==1
                && ack["id"]==id && ack["sequence"]==seq && m_records[id].toObject()["result_sequence"]==seq) {
                auto record=m_records[id].toObject();record["result_pending"]=false;m_records[id]=record;persist();
            }
        });
    }
}
