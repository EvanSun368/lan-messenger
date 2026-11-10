#pragma once
#include <QFont>
#include <QHash>
#include <QJsonObject>
#include <QMainWindow>
#include <QPointer>
#include <QSettings>
#include <QUrl>
class Node;
class FileTransfer;
class ScreenService;
class LinkPreview;
class ChatNotifications;
class QTimer;
class QScreen;
class QListWidget;
class QListWidgetItem;
class QTextBrowser;
class QPlainTextEdit;
class QTabBar;
class QLineEdit;
class QLabel;
class QDialog;
class QTableWidget;
class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    MainWindow(QString storage = {}, quint16 port = 0, bool demo = false, QWidget *parent = nullptr);
    ~MainWindow();
    void saveDemoScreenshot(QString path, QString view = "main");
    Node *node() const {
        return m_node;
    }

  private:
    bool eventFilter(QObject *, QEvent *) override;
    Node *m_node;
    ChatNotifications *m_notifications = nullptr;
    FileTransfer *m_files = nullptr;
    ScreenService *m_screen = nullptr;
    LinkPreview *m_preview = nullptr;
    QSettings m_settings;
    bool m_demo;
    QString m_current;
    QHash<QString, QJsonObject> m_peers, m_groups;
    QHash<QString, QString> m_drafts;
    QListWidget *m_contacts;
    QLineEdit *m_search, *m_historySearch;
    QLabel *m_sendHint;
    bool m_enterSends = false;
    QTabBar *m_tabs;
    QLabel *m_nickname, *m_address, *m_count;
    QDialog *m_chat, *m_transfers, *m_shares;
    QTextBrowser *m_messages, *m_history;
    QPlainTextEdit *m_editor;
    QLabel *m_chatTitle;
    QWidget *m_historyPanel;
    QTableWidget *m_transferTable;
    QHash<QString, int> m_rows;
    QHash<QString, QString> m_saveDirs;
    QHash<QString, QJsonObject> m_offers;
    QLineEdit *m_shareCode, *m_sharePath;
    QListWidget *m_shareList, *m_published;
    QString m_sharePeer;
    QFont m_chatFont;
    QPointer<QDialog> m_screenDialog;
    QHash<QString, QPointer<QDialog>> m_receivers;
    QHash<QString, QPointer<QLabel>> m_frameLabels;
    QTimer *m_captureTimer = nullptr;
    QPointer<QScreen> m_captureScreen;
    void buildUi();
    void buildFiles();
    void buildSharing();
    void setupServices();
    void event(QString kind, QJsonObject data);
    void refreshContacts();
    void filterContacts();
    void selectConversation(QListWidgetItem *);
    void openConversation();
    void renderHistory();
    void appendMessage(QJsonObject row);
    void sendMessage();
    void chooseEmoji();
    void chooseFont();
    void chooseImage();
    void updateHistorySearch();
    void captureScreenshot();
    void sendImage(QByteArray jpeg);
    void editSettings();
    void viewProfile();
    void addPeer();
    void createGroup();
    void sendFile();
    void offerReceived(QJsonObject);
    void transferProgress(QString, qint64, qint64, QString);
    void browseShare();
    void downloadShare();
    void openScreen();
    void openLink(const QUrl &);
    void showError(QString);
    QString currentName() const;
    QStringList currentMembers() const;
    void populateDemo();
};
