#pragma once
#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QByteArray>
#include <QStringList>
class Node; class QTcpSocket; class QTimer;
class ScreenService: public QObject {
 Q_OBJECT
public:
 explicit ScreenService(Node*,QObject* parent=nullptr);
 ~ScreenService();
 bool handle(QTcpSocket*,QJsonObject,QString);
 QStringList startMany(QStringList peerIds);
 void decide(QString session,bool accepted);
 void publish(QByteArray jpeg);
 void stop(QString session);
 void stopAll();
 bool sending()const;
 static constexpr int MaxJpeg=2*1024*1024;
signals:
 void event(QString kind,QJsonObject data);
 void frameReceived(QString session,QByteArray jpeg);
private:
 struct Session {QTcpSocket* socket=nullptr; QTimer* timeout=nullptr; QByteArray buffer; QString peer; bool outgoing=false; bool approved=false; bool waiting=false; bool delayed=false;};
 Node* node; QHash<QString,Session*> sessions;
 void attach(QString id,Session* s,int timeoutMs);
 void consume(QString id);
 void fail(QString id,QString message);
 void resetTimeout(Session*,int ms);
};
