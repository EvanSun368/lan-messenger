#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QSqlDatabase>
#include <functional>
class Node : public QObject {
 Q_OBJECT
public:
 using Reply=std::function<void(QJsonObject,QString)>;
 Node(QString name,QString storageDir,QObject *parent=nullptr);
 ~Node();
 bool start(quint16 port=0,quint16 discoveryPort=45871);
 void stop();
 QString id() const{return m_id;} QString name()const{return m_name;} QString root()const{return m_root;}
 quint16 port()const{return m_server.serverPort();}
 QJsonObject hello()const; QJsonArray peers()const; QJsonArray groups()const;
 QJsonArray history(const QString &conversation,int limit=200)const;
 QJsonArray pendingMessages()const;
 QJsonObject publicProfile()const{return m_profile;}
 bool updateProfile(QJsonObject profile);
 void fetchProfile(QString peerId);
 void addPeer(QString host,quint16 port);
 QString createGroup(QStringList members,QString name);
 QString sendMessage(QString conversation,QString text);
 QString sendImage(QString conversation,QByteArray jpeg);
 void request(QString peerId,QJsonObject payload,Reply callback,int timeoutMs=5000);
 void requestAddress(QString host,quint16 port,QJsonObject payload,Reply callback,int timeoutMs=5000);
 bool remember(QJsonObject peer,QString host);
 static QByteArray frame(QJsonObject object);
 static bool privateAddress(const QHostAddress &address);
 static bool validId(QString id);
 static void writeFrame(QTcpSocket *socket,QJsonObject object);
 // Synchronous signal dispatch: handler must take ownership of socket on recognized command.
 // Return true from this hook to retain the connection for binary/file/screen streams.
 std::function<bool(QTcpSocket*,QJsonObject,QString)> serviceHandler;
signals:
 void event(QString kind,QJsonObject data);
private:
 QString m_id,m_name,m_root,m_connection;
 QJsonObject m_profile;
 QHash<QString,QJsonObject> m_peers,m_groups;
 QSqlDatabase m_db;
 QTcpServer m_server; QUdpSocket m_udp; QTimer m_broadcast,m_retry;
 QSet<QString> m_inflight;
 bool m_running=false; quint16 m_discoveryPort=45871;
 void incoming(); void handle(QTcpSocket*,QJsonObject,QString);
 void announce(); void flush();
 QString queue(QString conversation,QString text,QJsonObject image={});
 bool receiveChat(QJsonObject r,QString *error);
 bool save(QString mid,QString conversation,QString direction,QString text,QString author={},QString image={});
 QString storeImage(QJsonObject payload,QString *error);
 void fail(QString message);
};
