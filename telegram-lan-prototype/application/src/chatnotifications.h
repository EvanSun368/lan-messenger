#pragma once
#include <QObject>
#include <QHash>
#include <QPointer>
#include <QString>
#include <QIcon>
class QWidget;
class QSystemTrayIcon;
class ChatNotifications : public QObject {
    Q_OBJECT
public:
    explicit ChatNotifications(QWidget *window, bool desktopEnabled = true, const QIcon &icon = {});
    void incoming(const QString &conversation, const QString &name, bool activelyReading);
    void markRead(const QString &conversation);
    int unread(const QString &conversation) const { return m_unread.value(conversation); }
    int totalUnread() const;
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
signals:
    void unreadChanged();
    void activateConversation(QString conversation);
private:
    QPointer<QWidget> m_window;
    QSystemTrayIcon *m_tray = nullptr;
    QHash<QString, int> m_unread;
    QString m_latest;
    bool m_enabled = true;
    qint64 m_lastAlert = 0;
    void updateBadge();
};
