#pragma once
#include <QFont>
#include "node.h"
#include "telegram_search_field.h"
#include <QSet>
#include "telegram_history_presentation.h"
#include <QHash>
#include <QSet>
#include <QJsonObject>
#include <QMainWindow>
#include <QPointer>
#include <QSettings>
#include <QUrl>
#include "updateservice.h"
namespace TelegramLan { class DialogsView; class TransferList; class SharingPane; }
class MailWindow;
class ContactMailBook;
#include <memory>
class Node;
class FileTransfer;
class ScreenService;
class LinkPreview;
class ChatNotifications;
class QTimer;
class QPushButton;
class QSplitter;
class QResizeEvent;
class QScreen;
class QListWidget;
class QListWidgetItem;
class QTextBrowser;
class QScrollArea;
class QVBoxLayout;
class TelegramInputAdapter;
class QTabBar;
class QLineEdit;
class QLabel;
class QDialog;
class QTableWidget;
class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    MainWindow(QString storage = {}, quint16 port = 0, bool demo = false, QWidget *parent = nullptr, NetworkPolicy network = {});
    ~MainWindow();
    void saveDemoScreenshot(QString path, QString view = "main");
    Node *node() const {
        return m_node;
    }

  private:
    bool eventFilter(QObject *, QEvent *) override;
    void closeEvent(QCloseEvent *) override;
    Node *m_node;
    ChatNotifications *m_notifications = nullptr;
    UpdateService *m_updates = nullptr;
    bool m_unreadRefreshPending = false;
    FileTransfer *m_files = nullptr;
    ScreenService *m_screen = nullptr;
    LinkPreview *m_preview = nullptr;
    QSettings m_settings;
    bool m_demo;
    QString m_current;
    QHash<QString, QJsonObject> m_peers, m_groups;
    QHash<QString, QString> m_drafts;
    QListWidget *m_contacts;
    TelegramLan::DialogsView *m_dialogs = nullptr;
    TelegramSearchField *m_search, *m_historySearch;
    QLabel *m_sendHint;
    bool m_enterSends = false;
    QTabBar *m_tabs;
    QLabel *m_nickname, *m_address, *m_count;
    QDialog *m_chat, *m_transfers=nullptr, *m_shares;
#ifdef LANLINK_FULL_TELEGRAM_UI
    QPointer<TelegramLan::TransferList> m_transferList;
    QPointer<TelegramLan::SharingPane> m_sharingPane;
#endif
    QPointer<MailWindow> m_mailWindow;
    void showMail();
    void exportChatMail();
    ContactMailBook *contactMailBook();
    std::unique_ptr<ContactMailBook> m_contactMailBook;
    void showTransfers();
    void showSharing();
    void refreshSharingRoots();
    void downloadShareFile(QString peer,QString token,QString relative);
    void watchModalOverlay(QWidget *overlay);
    QScrollArea *m_messages;
    QVBoxLayout *m_messageLayout;
    QTextBrowser *m_history;
    TelegramInputAdapter *m_editor;
    QLabel *m_chatTitle;
    QLabel *m_chatStatus = nullptr;
    QWidget *m_historyPanel;
    QTableWidget *m_transferTable;
    QHash<QString, int> m_rows;
    QHash<QString, QString> m_saveDirs;
    QHash<QString, QJsonObject> m_offers;
    QLineEdit *m_shareCode, *m_sharePath;
    QListWidget *m_shareList, *m_published;
    QString m_sharePeer, m_shareToken, m_shareDirectory;
    QLabel *m_shareLocation = nullptr;
    QFont m_chatFont;
    QPointer<QDialog> m_screenDialog;
    QHash<QString, QPointer<QDialog>> m_receivers;
    QHash<QString, QPointer<QLabel>> m_frameLabels;
    QTimer *m_captureTimer = nullptr;
    QPointer<QScreen> m_captureScreen;
    void buildUi();
#ifdef LANLINK_FULL_TELEGRAM_UI
    QSplitter *m_columns = nullptr;
    QWidget *m_rosterPanel = nullptr;
    bool m_showConversation = false;
    double m_dialogsRatio = 0.32;
    void updateColumns();
    void resizeEvent(QResizeEvent *) override;
#endif
    bool conversationVisible() const;
    void buildFiles();
    void buildSharing();
    void setupServices();
    void event(QString kind, QJsonObject data);
    void refreshContacts();
    void filterContacts();
    void selectConversation(QListWidgetItem *);
    void openConversation();
    void renderHistory(bool preservePosition = true,const QString &revealMessageId = {});
    void restoreMessageScroll(int position, bool bottom);
    void loadEarlierHistory();
    bool refreshHistoryWindow(bool reset,const QString &revealMessageId);
    void renderLoadedHistory(int position,bool bottom,const QPair<QString,int> &anchor);
    QPair<QString,int> captureHistoryAnchor() const;
    int historyAnchorPosition(int fallback) const;
    void restoreHistoryAnchor(const QPair<QString,int> &anchor,int fallback);
    QPushButton *m_loadEarlier=nullptr;
    QString m_loadedConversation,m_historyLoadError,m_scrollAnchorId,m_earlierHistoryMessageId;
    int m_scrollAnchorOffset=0;
    QJsonArray m_loadedHistory;
    HistoryCursor m_historyBefore;
    bool m_historyHasMore=false;
    QSet<QString> m_expandedHistoryMessages;
    void applyChatFont(QFont font);
    bool m_rebuildingMessages = false;
    QVector<TelegramLan::HistoryMessage> m_presentedHistory;
    QVector<QPointer<QWidget>> m_presentedHistoryRows;
    void applyHistoryPresentation();
    TelegramLan::HistoryMessage m_previousHistoryMessage;
    bool m_hasPreviousHistoryMessage = false;
    quint64 m_scrollGeneration = 0;
    bool m_scrollRestorePending = false, m_scrollFollowBottom = false;
    int m_scrollPosition = 0;
    friend class UiTest;
    friend class NetworkDiagnosticsUiTest;
    void appendMessage(QJsonObject row);
    QSet<QString> m_timelineTransfers;
    QSet<QString> m_transferHistoryWarnings;
    QString m_pendingOfferStatusId,m_pendingOfferStatusText;
    void transferSnapshotChanged(QJsonObject snapshot);
    void acceptTransferFromCard(const QString &id);
    void sendMessage();
    void refreshDeliveryStatus(const QString &messageId);
    void showDeliveryDetails(const QString &messageId);
    void chooseEmoji();
    void chooseFont();
    void chooseImage();
    void updateHistorySearch();
    void updateComposeHint();
    void captureScreenshot();
    void sendImage(QByteArray jpeg);
    void sendImageTo(const QString &recipient,QByteArray jpeg);
    void editSettings();
    void checkForUpdates();
    void backgroundUpdateCheck();
    void startUpdateChecks();
    void connectUpdateService();
    void scheduleApplyOnQuit();
    void viewProfile();
    void addPeer();
    void showNetworkDiagnostics();
    void createGroup();
    void sendFile();
    void offerReceived(QJsonObject);
    void transferProgress(QString, qint64, qint64, QString);
    void browseShare();
    void browseShareRequest(const QString &peer,const QString &token,const QString &directory);
    void downloadShare();
    void openScreen();
    void openLink(const QUrl &);
    void showError(QString);
    QString currentName() const;
    QStringList currentMembers() const;
    void populateDemo();
};
