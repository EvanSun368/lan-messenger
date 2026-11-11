#pragma once
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QSet>
#include <QTcpSocket>
#include <functional>
class Node;
class QTimer;
class FileTransfer : public QObject {
    Q_OBJECT
  public:
    explicit FileTransfer(Node *node, QObject *parent = nullptr);
    ~FileTransfer();
    bool handle(QTcpSocket *, QJsonObject, QString host);
    void offerFile(QString peerId, QString path, QString conversationId = {});
    void acceptFile(QString transferId, QString saveDir);
    void rejectFile(QString transferId);
    void pauseFile(QString transferId);
    QString publishShare(QString path);
    void revokeShare(QString token);
    void listShare(QString peerId, QString token, QString relative = {});
    void downloadShare(QString peerId, QString token, QString relative, QString saveDir);
    QJsonArray incomingOffers() const;
    QJsonObject publishedShares() const {
        return m_shares;
    }
    QJsonObject transferSnapshot(QString id) const;
    QString persistenceError() const { return m_persistFailure; }
    QJsonArray transferSnapshots() const;
    void retryFile(QString id);
    static bool safeName(QString name);
    static QByteArray sha256(QString path);
  signals:
    void transferChanged(QJsonObject snapshot);
    void offerReceived(QJsonObject offer);
    void progress(QString id, qint64 done, qint64 total, QString status);
    void completed(QString id, QString path);
    void shareListed(QJsonArray entries);
    void shareListedFor(QString peer, QString token, QString path, QJsonArray entries);
    void error(QString message);

  private:
    friend class FileTests;
    Node *m_node;
    QString m_storagePath, m_persistFailure;
    QJsonObject m_incoming, m_outgoing, m_shares, m_records;
    QTimer *m_recordTimer = nullptr;
    QSet<QString> m_recordNotificationsPending;
    void notifyRecord(QString id);
    void recordTransfer(QString id, QJsonObject offer, QString peerId, QString direction, QString path = {}, QString conversationId = {});
    void updateRecord(QString id, QJsonObject fields, bool durable = true);
    void recordProgress(QString id, qint64 done, qint64 total, QString status);
    QHash<QString, QPointer<QTcpSocket>> m_active;
    QSet<QString> m_verifying, m_cancelledVerification;
    int m_hashJobs = 0;
    QQueue<QPair<QString, std::function<void(QByteArray)>>> m_hashQueue;
    void runNextHash();
    bool m_stopping = false;
    bool persist();
    void prune();
    QJsonObject peer(QString id) const;
    QJsonObject metadata(QString host, QString path, QString shareToken, QByteArray hash);
    void metadataAsync(QString host, QString path, QString shareToken,
                       std::function<void(QJsonObject)> callback);
    void hashAsync(QString path, std::function<void(QByteArray)> callback);
    bool registerOffer(QJsonObject offer, bool notify = true, QString *error = nullptr);
    void release(QString id);
    void notifyRelease(QJsonObject item);
    void queueResult(QString id, QString state);
    void sendResults();
    QSet<QString> m_resultSending;
    QString sharePath(QString token, QString relative) const;
    void serveFetch(QTcpSocket *, QJsonObject, QString, QByteArray hash = {});
    void serveShare(QTcpSocket *, QJsonObject, QString);
    void reply(QTcpSocket *, QJsonObject);
};
