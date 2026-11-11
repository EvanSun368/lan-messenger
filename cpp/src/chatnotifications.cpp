#include "chatnotifications.h"
#include <QtWidgets>
ChatNotifications::ChatNotifications(QWidget *window, bool desktopEnabled, const QIcon &icon) : QObject(window), m_window(window) {
    if (!desktopEnabled || !QSystemTrayIcon::isSystemTrayAvailable()) return;
    const auto trayIcon = icon.isNull() ? window->style()->standardIcon(QStyle::SP_MessageBoxInformation) : icon;
    m_tray = new QSystemTrayIcon(trayIcon, this);
    m_tray->setToolTip("LanLink");
    auto *menu = new QMenu(window);
    menu->addAction("打开 LanLink", this, [this] { if(m_window) {m_window->showNormal(); m_window->raise(); m_window->activateWindow();} });
    menu->addSeparator();
    menu->addAction("退出", qApp, &QApplication::quit);
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::messageClicked, this, [this] { emit activateConversation(m_latest); });
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if(r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) emit activateConversation(m_latest);
    });
    m_tray->show();
}
int ChatNotifications::totalUnread() const { int n=0; for(int v:m_unread) n+=v; return n; }
void ChatNotifications::incoming(const QString &conversation, const QString &name, bool activelyReading) {
    if(conversation.isEmpty()) return;
    if(activelyReading) {markRead(conversation); return;}
    m_unread[conversation] = qMin(9999, m_unread.value(conversation)+1);
    m_latest=conversation;
    updateBadge();
    emit unreadChanged();
    if(!m_enabled) return;
    if(m_window) QApplication::alert(m_window, 5000);
    auto now=QDateTime::currentMSecsSinceEpoch();
    if(now-m_lastAlert < 1500) return;
    m_lastAlert=now;
    // Avoid disclosing message contents on the lock screen.
    if(m_tray) m_tray->showMessage("LanLink · 新消息", QString("%1 发来消息（共 %2 条未读）").arg(name.left(80)).arg(totalUnread()), QSystemTrayIcon::Information, 5000);
}
void ChatNotifications::markRead(const QString &conversation) {
    if(m_unread.remove(conversation)) { updateBadge(); emit unreadChanged(); }
}
void ChatNotifications::updateBadge() {
    if(m_tray) m_tray->setToolTip(QString("LanLink · %1 条未读").arg(totalUnread()));
}
