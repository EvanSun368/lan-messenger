#pragma once
#include <QWidget>
#include <QJsonObject>
class QLabel;class QProgressBar;class QPushButton;
class TransferCard final : public QWidget {
 Q_OBJECT
public:
 explicit TransferCard(QWidget *parent=nullptr);
 void setSnapshot(QJsonObject snapshot);
 static QString statusText(const QJsonObject &snapshot);
 QJsonObject snapshot() const {return m_snapshot;}
signals:
 void acceptRequested(QString id);
 void rejectRequested(QString id);
 void pauseRequested(QString id);
 void retryRequested(QString id);
 void openRequested(QString id,QString path);
private:
 QJsonObject m_snapshot;
 QLabel *m_name,*m_status,*m_bytes;
 QProgressBar *m_progress;
 QPushButton *m_accept,*m_reject,*m_pause,*m_retry,*m_open;
};
