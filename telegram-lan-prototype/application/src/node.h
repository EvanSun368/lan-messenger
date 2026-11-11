#pragma once
#include "networkpolicy.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSqlDatabase>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>
#include <functional>
struct HistoryCursor {
    QString conversation, id;
    double timestamp = 0;
    bool valid = false;
};
struct HistoryPage {
    QJsonArray records;
    HistoryCursor nextBefore;
    bool hasMore = false;
    QString error;
};
class Node : public QObject {
    Q_OBJECT
  public:
    static constexpr qsizetype MaxTextBytes = 128 * 1024;
    static constexpr qsizetype MaxFrameBytes = 1024 * 1024;
    using Reply = std::function<void(QJsonObject, QString)>;
    Node(QString name, QString storageDir, QObject *parent = nullptr);
    ~Node();
    bool start(quint16 port = 0, quint16 discoveryPort = 45871);
    void stop();
    // Configure only while stopped. VPN mode has no discovery or LAN fallback.
    bool setNetworkPolicy(const NetworkPolicy &policy, QString *error = nullptr);
    const NetworkPolicy &networkPolicy() const { return m_networkPolicy; }
    QString networkMode() const { return m_networkPolicy.mode(); }
    bool addressAllowed(const QHostAddress &address) const { return m_networkPolicy.allows(address); }
    bool peerAllowed(const QJsonObject &peer) const;
    bool sameNetworkScope(const QJsonObject &record) const;
    // All direct file/screen sockets must pass this gate before connectToHost.
    bool prepareOutgoingSocket(QTcpSocket *socket, const QHostAddress &address, quint16 port, QString *error);
    QJsonObject networkDiagnostics() const;
    void diagnoseAddress(QString host, quint16 port, Reply callback, int timeoutMs = 5000);
    QString id() const {
        return m_id;
    }
    QString name() const {
        return m_name;
    }
    QString root() const {
        return m_root;
    }
    quint16 port() const {
        return m_server.serverPort();
    }
    QJsonObject hello() const;
    QJsonArray peers() const;
    QJsonArray groups() const;
    QJsonArray history(const QString &conversation, int limit = 200) const;
    // Stable exclusive keyset cursor (time,id), scoped to this conversation.
    // records are chronological; page size is bounded, total history is not.
    HistoryPage historyPage(const QString &conversation, HistoryCursor before = {}, int limit = 200) const;
    QJsonObject historyRecord(const QString &conversation,const QString &messageId) const;
    QJsonArray pendingMessages() const;
    // "received" means a matching receiver persistence ACK, never read by a user.
    // Messages predating the delivery ledger return known=false.
    QJsonObject deliveryStatus(const QString &messageId) const;
    bool retryMessage(const QString &messageId);
    // Local-only timeline event, never sent as a chat protocol packet.
    bool recordTransferEvent(const QJsonObject &snapshot);
    // TOFU LAN declaration only: member consistency, not authenticated identity.
    QJsonObject fileGroupContext(const QString &conversation) const;
    QString acceptFileGroupContext(const QString &senderId,const QJsonObject &context,QString *error);
    QJsonObject publicProfile() const {
        return m_profile;
    }
    bool updateProfile(QJsonObject profile);
    void fetchProfile(QString peerId);
    void addPeer(QString host, quint16 port);
    QString createGroup(QStringList members, QString name);
    QString sendMessage(QString conversation, QString text);
    QString sendImage(QString conversation, QByteArray jpeg);
    void request(QString peerId, QJsonObject payload, Reply callback, int timeoutMs = 5000);
    void requestAddress(QString host, quint16 port, QJsonObject payload, Reply callback,
                        int timeoutMs = 5000);
    bool remember(QJsonObject peer, QString host);
    static QByteArray frame(QJsonObject object);
    static bool privateAddress(const QHostAddress &address);
    static bool validId(QString id);
    static void writeFrame(QTcpSocket *socket, QJsonObject object);
    // Synchronous signal dispatch: handler must take ownership of socket on recognized command.
    // Return true from this hook to retain the connection for binary/file/screen streams.
    std::function<bool(QTcpSocket *, QJsonObject, QString)> serviceHandler;
  signals:
    void event(QString kind, QJsonObject data);

  private:
    friend class NetworkPolicyTest;
    QString m_id, m_name, m_root, m_connection;
    QJsonObject m_profile;
    QHash<QString, QJsonObject> m_peers, m_groups;
    QSqlDatabase m_db;
    QTcpServer m_server;
    QUdpSocket m_udp;
    QTimer m_broadcast, m_retry, m_networkWatch;
    NetworkPolicy m_networkPolicy;
    QSet<QTcpSocket *> m_transportSockets;
    QString m_lastNetworkError;
    void trackSocket(QTcpSocket *socket);
    void checkNetworkAvailability();
    QSet<QString> m_inflight;
    bool m_running = false;
    bool m_queueScopeReady = false;
    quint16 m_discoveryPort = 45871;
    void incoming();
    void handle(QTcpSocket *, QJsonObject, QString);
    void announce();
    void flush();
    QString queue(QString conversation, QString text, QJsonObject image = {});
    bool receiveChat(QJsonObject r, QString *error);
    QJsonObject normalizedGroupDeclaration(const QJsonObject &group,const QString &senderId,QString *error) const;
    bool save(QString mid, QString conversation, QString direction, QString text, QString author = {},
              QString image = {});
    QString storeImage(QJsonObject payload, QString *error);
    void fail(QString message);
};
