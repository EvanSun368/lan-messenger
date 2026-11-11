#include "mainwindow.h"
#include "chatnotifications.h"
#include <QIcon>
#include "emojipicker.h"
#include "filetransfer.h"
#include "linkpreview.h"
#include "node.h"
#include "screenservice.h"
#include <QtNetwork>
#include <QtWidgets>

namespace {
QString interfaceFontFamily() {
    const auto available = QFontDatabase::families();
    for (const QString &family : {QString("Microsoft YaHei UI"), QString("Microsoft YaHei"), QString("Noto Sans CJK SC"), QString("Noto Sans SC")})
        if (available.contains(family)) return family;
    return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
}
QIcon lineIcon(const QString &name, QColor color = QColor("#73808b")) {
    QIcon icon;
    for (int scale : {1, 2, 3}) {
        QPixmap pix(24*scale,24*scale); pix.fill(Qt::transparent);
        QPainter p(&pix); p.setRenderHint(QPainter::Antialiasing); p.scale(scale,scale);
        p.setPen(QPen(color,1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        if (name == "表情") {
            p.drawEllipse(QRectF(3.5,3.5,17,17)); p.drawPoint(9,9);p.drawPoint(15,9);
            p.drawArc(QRectF(7,8,10,9),200*16,140*16);
        } else if (name == "截图") {
            p.drawEllipse(QRectF(3,14,5,5));p.drawEllipse(QRectF(14,14,5,5));
            p.drawLine(7,15,16,4); p.drawLine(15,15,6,4);
        } else if (name == "图片") {
            p.drawRoundedRect(QRectF(3,4,18,16),1,1);p.drawEllipse(QRectF(6,7,3,3));
            p.drawPolyline(QPolygonF{QPointF(4,18),QPointF(10,12),QPointF(14,16),QPointF(17,12),QPointF(20,16)});
        } else if (name == "文件") {
            p.drawPolyline(QPolygonF{QPointF(14,3),QPointF(6,3),QPointF(6,21),QPointF(19,21),QPointF(19,8),QPointF(14,3),QPointF(14,8),QPointF(19,8)});
            p.drawLine(9,12,16,12);p.drawLine(9,16,16,16);
        } else if (name == "投屏") {
            p.drawRoundedRect(QRectF(2,4,20,14),1,1);p.drawLine(9,21,15,21);p.drawLine(12,18,12,21);
        } else if (name == "历史") {
            p.drawEllipse(QRectF(3,3,18,18));p.drawLine(12,7,12,12);p.drawLine(12,12,16,14);
        } else if (name == "更多") {
            p.setBrush(color);p.setPen(Qt::NoPen);for(int x:{5,12,19}) p.drawEllipse(QPointF(x,12),1.5,1.5);
        } else if (name == "close") {p.drawLine(8,8,16,16);p.drawLine(16,8,8,16);
        } else if (name == "maximize") {p.drawRect(QRectF(7,7,10,10));
        } else if (name == "minimize") {p.drawLine(7,13,17,13);
        } else if (name == "add") {p.drawLine(6,12,18,12);p.drawLine(12,6,12,18);
        } else if (name == "down") {p.drawPolyline(QPolygonF{QPointF(8,10),QPointF(12,14),QPointF(16,10)});}
        p.end();pix.setDevicePixelRatio(scale);icon.addPixmap(pix);
    }
    return icon;
}
QLabel *plainLabel(const QString &s, QWidget *p = nullptr) {
    auto *l = new QLabel(s, p);
    l->setTextFormat(Qt::PlainText);
    return l;
}
bool confirm(QWidget *w, QString title, QString text) {
    QMessageBox b(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::No, w);
    b.setTextFormat(Qt::PlainText);
    b.setDefaultButton(QMessageBox::No);
    return b.exec() == QMessageBox::Yes;
}
class MessageText : public QTextBrowser {
  public:
    MessageText(QString text, QFont font, QWidget *parent=nullptr) : QTextBrowser(parent) {
        setObjectName("messageText");
        setFrameShape(QFrame::NoFrame);
        setOpenLinks(false);setOpenExternalLinks(false);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setStyleSheet("QTextBrowser{background:transparent;border:0;padding:0;}");
        document()->setDocumentMargin(0);
        QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        document()->setDefaultTextOption(option);
        setPlainText(text); applyFont(font);
    }
    void applyFont(const QFont &font) {
        setFont(font);document()->setDefaultFont(font);
        int naturalWidth=0;
        for(const auto &line:toPlainText().split('\n')) naturalWidth=qMax(naturalWidth,QFontMetrics(font).horizontalAdvance(line));
        const int width=qBound(60,naturalWidth+4,320);
        setFixedWidth(width); document()->setTextWidth(width);
        setFixedHeight(qCeil(document()->size().height())+2);
    }
};
class TitleBar : public QWidget {
    QPoint m_offset;
    QWidget *m_window;

  public:
    QLabel *label;
    TitleBar(QWidget *window, QString text) : m_window(window) {
        const bool modern = window->objectName() == "chatWindow";
        setObjectName(modern ? "chatTitleBar" : "titleBar");
        auto *l = new QHBoxLayout(this);
        setFixedHeight(modern ? 56 : 36);
        l->setSpacing(0);
        l->setContentsMargins(12, 0, 0, 0);
        label = plainLabel(text);
        label->setStyleSheet(modern ? "color:#26384b;font-size:16px;font-weight:600;" : "color:white;font-weight:500;");
        l->addWidget(label, 1);
        for (auto s : {QString("−"), QString("□"), QString("×")}) {
            auto *b = new QPushButton;
            b->setIcon(lineIcon(s == "×" ? "close" : s == "□" ? "maximize" : "minimize", modern ? QColor("#7a8897") : QColor(Qt::white)));
            b->setIconSize(QSize(24,24));
            b->setToolTip(s == "×" ? "关闭" : s == "□" ? "最大化 / 还原" : "最小化");
            b->setFixedSize(40, 36);
            b->setObjectName("titleButton");
            l->addWidget(b);
            connect(b, &QPushButton::clicked, this, [this, s] {
                if (s == "×")
                    m_window->close();
                else if (s == "□") {
                    if (m_window->isMaximized()) m_window->showNormal(); else m_window->showMaximized();
                } else
                    m_window->showMinimized();
            });
        }
    }
    void mousePressEvent(QMouseEvent *e) override {
        m_offset = e->globalPosition().toPoint() - m_window->frameGeometry().topLeft();
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if (e->buttons() & Qt::LeftButton)
            m_window->move(e->globalPosition().toPoint() - m_offset);
    }
};
class CaptureOverlay : public QDialog {
    QPixmap m_full;
    QPoint m_start, m_end;
    bool m_select = false;

  public:
    QPixmap selected;
    CaptureOverlay(QScreen *screen) : m_full(screen->grabWindow(0)) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setGeometry(screen->geometry());
        setCursor(Qt::CrossCursor);
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.drawPixmap(rect(), m_full);
        p.fillRect(rect(), QColor(0, 0, 0, 90));
        QRect r = QRect(m_start, m_end).normalized().intersected(rect());
        if (m_select) {
            p.save();
            p.setClipRect(r);
            p.drawPixmap(rect(), m_full);
            p.restore();
            p.setPen(QPen(QColor("#2196ed"), 2));
            p.drawRect(r);
        }
        p.setPen(Qt::white);
        p.drawText(20, 30, "拖动选择截图区域，Esc 取消");
    }
    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton) {
            m_start = m_end = e->position().toPoint();
            m_select = true;
            update();
        }
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if (m_select) {
            m_end = e->position().toPoint();
            update();
        }
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        if (e->button() != Qt::LeftButton)
            return;
        m_end = e->position().toPoint();
        auto r = QRect(m_start, m_end).normalized().intersected(rect());
        if (r.width() < 4 || r.height() < 4) {
            reject();
            return;
        }
        const double sx = double(m_full.width()) / width(), sy = double(m_full.height()) / height();
        selected = m_full.copy(int(r.x() * sx), int(r.y() * sy), int(r.width() * sx), int(r.height() * sy));
        accept();
    }
    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Escape)
            reject();
        else
            QDialog::keyPressEvent(e);
    }
};
} // namespace

MainWindow::MainWindow(QString storage, quint16 port, bool demo, QWidget *parent)
    : QMainWindow(parent), m_settings("LanLink", "LanLinkCpp"), m_demo(demo) {
    if (storage.isEmpty())
        storage = QDir::homePath() + "/.lanlink-cpp";
    m_node = new Node(
        m_settings.value("name", qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER", "我的电脑")))
            .toString(),
        storage, this);
    connect(m_node, &Node::event, this, &MainWindow::event);
    m_enterSends = m_settings.value("enterSends", false).toBool();
    m_chatFont = QFont(m_settings.value("fontFamily", interfaceFontFamily()).toString(),
                       qBound(8, m_settings.value("fontSize", 11).toInt(), 24));
    if (!QFontDatabase::families().contains(m_chatFont.family())) m_chatFont.setFamily(interfaceFontFamily());
    m_chatFont.setStyleStrategy(QFont::PreferAntialias);
    buildUi();
    buildFiles();
    buildSharing();
    setupServices();
    setWindowIcon(QIcon(QStringLiteral(":/icons/lanlink.png")));
    m_notifications = new ChatNotifications(this, !m_demo, windowIcon());
    m_notifications->setEnabled(m_settings.value("notifications", true).toBool());
    connect(m_notifications, &ChatNotifications::unreadChanged, this, [this] {
        refreshContacts();
        setWindowTitle(QString("LanLink C++ 0.5.1%1").arg(m_notifications->totalUnread() ? QString(" · %1 条未读").arg(m_notifications->totalUnread()) : QString()));
    });
    connect(m_notifications, &ChatNotifications::activateConversation, this, [this](QString id) {
        m_search->clear(); m_tabs->setCurrentIndex(0);
        for(int n=0;n<m_contacts->count();++n) if(m_contacts->item(n)->data(Qt::UserRole).toString()==id) {
            m_contacts->setCurrentRow(n); openConversation(); break;
        }
    });
    for (auto v : m_node->peers()) {
        auto p = v.toObject();
        m_peers[p["id"].toString()] = p;
    }
    for (auto v : m_node->groups()) {
        auto g = v.toObject();
        m_groups["group:" + g["id"].toString()] = g;
    }
    refreshContacts();
    if (demo)
        populateDemo();
    else if (m_node->start(port))
        m_address->setText(QString("局域网端口 %1\n接收文件前需确认").arg(m_node->port()));
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        statusBar()->showMessage(
            QString("未加密 · 仅限可信局域网  |  待发 %1").arg(m_node->pendingMessages().size()));
    });
    timer->start(2000);
}
MainWindow::~MainWindow() {
    if (m_captureTimer)
        m_captureTimer->stop();
    m_node->serviceHandler = {};
    if (m_screen) {
        m_screen->stopAll();
        delete m_screen;
        m_screen = nullptr;
    }
    if (m_files) {
        delete m_files;
        m_files = nullptr;
    }
    m_node->stop();
}
void MainWindow::buildUi() {
    setWindowTitle("LanLink · C++ Qt 开发预览");
    setWindowFlag(Qt::FramelessWindowHint);
    QFont uiFont(interfaceFontFamily(), 10);
    uiFont.setStyleStrategy(QFont::PreferAntialias);
    setFont(uiFont);
    resize(362, 800);
    setMinimumSize(340, 600);
    setStyleSheet(
        "QMainWindow,QDialog{background:#ffffff;color:#263442;} "
        "QWidget#chatTitleBar{background:white;border-bottom:1px solid #e9edf2;} "
        "QScrollArea#messages{border:0;background:#f3f5f8;} QWidget#messageCanvas{background:#f3f5f8;} "
        "QWidget#titleBar,QWidget#profileHeader,QWidget#footerBar{background:#2298ee;} "
        "QPushButton{padding:5px 14px;min-height:20px;border:1px solid #cbd6df;border-radius:3px;background:#ffffff;color:#344454;} "
        "QPushButton:hover{background:#edf6fd;border-color:#9dc8e9;} QPushButton:pressed{background:#dceefd;} "
        "QPushButton:disabled{color:#a9b3bc;background:#f7f8fa;border-color:#e4e9ee;} "
        "QPushButton#titleButton{background:transparent;border:0;padding:0;min-height:0;} "
        "QPushButton#titleButton:hover{background:rgba(255,255,255,30);} "
        "QLabel#nickname{font-size:19px;font-weight:600;color:white;} QLabel#profileAddress{color:#e6f4ff;} "
        "QPushButton#profileAction{border:0;color:white;background:transparent;padding:4px 8px;} "
        "QTabBar::tab{padding:12px 16px;background:white;border-bottom:2px solid white;color:#667787;} "
        "QTabBar::tab:selected{border-bottom:2px solid #2298ee;color:#168bdd;} "
        "QListWidget{border:0;background:white;} QListWidget::item{padding:13px 8px;} "
        "QListWidget::item:selected{background:#e7f3fc;color:#164668;} "
        "QLineEdit{padding:7px 9px;border:1px solid #d9e1e8;border-radius:3px;background:#fff;color:#344454;} "
        "QLineEdit:focus{border-color:#87bce5;} "
        "QTextBrowser{border:0;background:white;color:#263442;} QPlainTextEdit{border:0;background:white;color:#263442;selection-background-color:#cae6fb;} "
        "QPushButton#primary{background:#2298ee;border:1px solid #2298ee;color:white;} "
        "QPushButton#primary:hover{background:#1489df;} "
        "QToolButton{border:0;border-radius:3px;background:transparent;padding:0;color:#6c7b89;} "
        "QToolButton:hover{background:#edf5fc;} QToolButton:pressed{background:#dcecf9;} "
        "QToolButton::menu-indicator{image:none;width:0;} "
        "QToolButton#sendOptions{border:1px solid #2298ee;background:#2298ee;border-radius:3px;} "
        "QToolButton#sendOptions:hover{background:#1489df;} "
        "QSplitter::handle{background:#e7ecf1;} "
        "QMenu{background:white;border:1px solid #d4dde5;padding:5px;color:#344454;} "
        "QMenu::item{padding:7px 24px;} QMenu::item:selected{background:#e7f3fc;} "
        "QStatusBar{background:#2298ee;color:white;font-size:11px;}");
    auto *root = new QWidget;
    setCentralWidget(root);
    auto *outer = new QVBoxLayout(root);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(new TitleBar(this, "LanLink 局域网通信"));
    auto *profile = new QWidget;
    profile->setObjectName("profileHeader");
    auto *pl = new QVBoxLayout(profile);
    pl->setContentsMargins(12, 14, 12, 12);
    auto *identity = new QHBoxLayout;
    auto *avatar = plainLabel("L");
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(58, 58);
    avatar->setStyleSheet("background:#d7eeff;color:#168fe0;font-size:30px;");
    identity->addWidget(avatar);
    auto *names = new QVBoxLayout;
    m_nickname = plainLabel(m_node->name());
    m_nickname->setObjectName("nickname");
    names->addWidget(m_nickname);
    m_address = plainLabel("局域网通信");
    m_address->setWordWrap(true);
    m_address->setObjectName("profileAddress");
    names->addWidget(m_address);
    auto *profileActions = new QHBoxLayout;
    auto *settings = new QPushButton("个人资料");
    settings->setObjectName("profileAction");
    connect(settings, &QPushButton::clicked, this, &MainWindow::editSettings);
    profileActions->addWidget(settings);
    profileActions->addStretch();
    names->addLayout(profileActions);
    identity->addLayout(names, 1);
    pl->addLayout(identity);
    auto *searchRow = new QHBoxLayout;
    m_search = new QLineEdit;
    m_search->setObjectName("search");
    m_search->setPlaceholderText("搜索");
    connect(m_search, &QLineEdit::textChanged, this, &MainWindow::filterContacts);
    searchRow->addWidget(m_search, 1);
    auto *add = new QToolButton;
    add->setIcon(lineIcon("add",Qt::white));
    add->setIconSize(QSize(24,24));
    add->setFixedSize(32, 32);
    add->setToolTip("添加联系人或创建讨论组");
    add->setPopupMode(QToolButton::InstantPopup);
    auto *menu = new QMenu(add);
    menu->addAction("添加设备 IP", this, &MainWindow::addPeer);
    menu->addAction("创建讨论组", this, &MainWindow::createGroup);
    add->setMenu(menu);
    searchRow->addWidget(add);
    pl->addLayout(searchRow);
    outer->addWidget(profile);
    m_tabs = new QTabBar;
    m_tabs->setObjectName("rosterTabs");
    m_tabs->addTab("消息");
    m_tabs->addTab("联系人");
    m_tabs->addTab("讨论组");
    m_tabs->setExpanding(true);
    m_tabs->setCurrentIndex(1);
    connect(m_tabs, &QTabBar::currentChanged, this, &MainWindow::filterContacts);
    outer->addWidget(m_tabs);
    m_count = plainLabel("局域网设备");
    m_count->setStyleSheet("background:#f7f8fa;padding:5px;color:#607d8b;");
    outer->addWidget(m_count);
    m_contacts = new QListWidget;
    m_contacts->setObjectName("contacts");
    connect(m_contacts, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *i) { selectConversation(i); });
    connect(m_contacts, &QListWidget::itemDoubleClicked, this, [this] { openConversation(); });
    outer->addWidget(m_contacts, 1);
    auto *footer = new QWidget;
    footer->setObjectName("footerBar");
    auto *fl = new QHBoxLayout(footer);
    fl->setContentsMargins(6, 5, 6, 5);
    for (auto entry : QList<QPair<QString, std::function<void()>>>{{"设置", [this] { editSettings(); }},
                                                                   {"会话", [this] { openConversation(); }},
                                                                   {"传输",
                                                                    [this] {
                                                                        m_transfers->show();
                                                                        m_transfers->raise();
                                                                    }},
                                                                   {"共享", [this] {
                                                                        m_shares->show();
                                                                        m_shares->raise();
                                                                    }}}) {
        auto *b = new QPushButton(entry.first);
        b->setObjectName("profileAction");
        connect(b, &QPushButton::clicked, this, entry.second);
        fl->addWidget(b);
    }
    outer->addWidget(footer);
    statusBar()->showMessage("未加密 · 仅限可信局域网");
    m_chat = new QDialog(this, Qt::Window | Qt::FramelessWindowHint);
    m_chat->setObjectName("chatWindow");
    m_chat->resize(1020, 754);
    m_chat->setMinimumSize(760, 540);
    auto *co = new QVBoxLayout(m_chat);
    co->setContentsMargins(1, 1, 1, 1);
    co->setSpacing(0);
    auto *ct = new TitleBar(m_chat, "会话");
    m_chatTitle = ct->label;
    co->addWidget(ct);
    auto *split = new QSplitter;
    co->addWidget(split, 1);
    auto *conversation = new QWidget;
    auto *cl = new QVBoxLayout(conversation);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);
    m_messages = new QScrollArea;
    m_messages->setObjectName("messages");
    m_messages->setWidgetResizable(true);
    m_messages->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *messageCanvas = new QWidget;
    messageCanvas->setObjectName("messageCanvas");
    m_messageLayout = new QVBoxLayout(messageCanvas);
    m_messageLayout->setContentsMargins(24,20,24,24);
    m_messageLayout->setSpacing(20);
    m_messageLayout->addStretch();
    m_messages->setWidget(messageCanvas);
    m_messages->viewport()->installEventFilter(this);
    connect(m_messages->verticalScrollBar(),&QScrollBar::sliderMoved,this,[this]{++m_scrollGeneration;m_scrollRestorePending=false;});
    connect(m_messages->verticalScrollBar(),&QScrollBar::actionTriggered,this,[this](int){++m_scrollGeneration;m_scrollRestorePending=false;});
    connect(m_messages->verticalScrollBar(),&QScrollBar::rangeChanged,this,[this](int,int) {
        if(!m_scrollRestorePending)return;
        const auto generation=m_scrollGeneration;
        QTimer::singleShot(0,this,[this,generation] {
            if(!m_scrollRestorePending || generation!=m_scrollGeneration)return;
            auto *bar=m_messages->verticalScrollBar();
            bar->setValue(m_scrollFollowBottom?bar->maximum():m_scrollPosition);
        });
    });
    auto *verticalSplit = new QSplitter(Qt::Vertical);
    verticalSplit->setObjectName("composeSplitter");
    verticalSplit->setChildrenCollapsible(false);
    cl->addWidget(verticalSplit, 1);
    verticalSplit->addWidget(m_messages);
    auto *compose = new QWidget;
    compose->setObjectName("composePanel");
    compose->setStyleSheet("QWidget#composePanel{background:white;border-top:1px solid #e7ecf2;}");
    auto *cpl = new QVBoxLayout(compose);
    cpl->setContentsMargins(12, 6, 12, 10);
    cpl->setSpacing(8);
    auto *toolbar = new QHBoxLayout;
    toolbar->setSpacing(4);
    for (auto action : QList<QPair<QString, std::function<void()>>>{{"表情", [this] { chooseEmoji(); }},
            {"截图", [this] { captureScreenshot(); }}, {"图片", [this] { chooseImage(); }},
            {"文件", [this] { sendFile(); }}, {"投屏", [this] { openScreen(); }}}) {
        auto *b = new QToolButton;
        b->setText(action.first);
        b->setAccessibleName(action.first);
        b->setToolTip(action.first);
        b->setIcon(lineIcon(action.first));
        b->setIconSize(QSize(22,22));
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        b->setFixedSize(32,32);
        connect(b, &QToolButton::clicked, this, action.second);
        toolbar->addWidget(b);
    }
    auto *more = new QToolButton;
    more->setText("更多");
    more->setAccessibleName("更多功能");
    more->setToolTip("更多：字体、资料、传输记录");
    more->setIcon(lineIcon("更多"));
    more->setIconSize(QSize(22,22));
    more->setFixedSize(32,32);
    more->setPopupMode(QToolButton::InstantPopup);
    auto *moreMenu = new QMenu(more);
    moreMenu->addAction("聊天字体…", this, &MainWindow::chooseFont);
    moreMenu->addAction("联系人资料…", this, &MainWindow::viewProfile);
    moreMenu->addAction("文件传输记录", this, [this] {m_transfers->show(); m_transfers->raise();});
    more->setMenu(moreMenu);
    toolbar->addWidget(more);
    toolbar->addStretch();
    auto *history = new QToolButton;
    history->setText("聊天记录");
    history->setIcon(lineIcon("历史"));
    history->setIconSize(QSize(18,18));
    history->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    history->setFixedSize(96,32);
    connect(history, &QPushButton::clicked, this,
            [this] { m_historyPanel->setVisible(!m_historyPanel->isVisible()); });
    toolbar->addWidget(history);
    cpl->addLayout(toolbar);
    m_editor = new QPlainTextEdit;
    m_editor->setObjectName("editor");
    m_editor->setPlaceholderText("输入消息…");
    m_editor->setFont(m_chatFont);
    m_editor->setMinimumHeight(72);
    m_editor->setStyleSheet("QPlainTextEdit{background:#f6f8fb;border:1px solid #e8edf2;border-radius:8px;padding:8px;}");
    m_editor->document()->setDocumentMargin(4);
    m_editor->installEventFilter(this);
    m_chat->installEventFilter(this);
    cpl->addWidget(m_editor);
    auto *bottom = new QHBoxLayout;
    m_sendHint = plainLabel(m_enterSends ? "Enter 发送 · Shift+Enter 换行" : "Ctrl+Enter 发送 · Enter 换行");
    m_sendHint->setStyleSheet("color:#929daa;border:0;");
    bottom->addWidget(m_sendHint);
    bottom->addStretch();
    auto *close = new QPushButton("关闭");
    connect(close, &QPushButton::clicked, m_chat, &QDialog::hide);
    close->setFixedSize(78,32);
    bottom->addWidget(close);
    auto *send = new QPushButton("发送");
    send->setFixedSize(78,32);
    send->setObjectName("primary");
    connect(send, &QPushButton::clicked, this, &MainWindow::sendMessage);
    bottom->addWidget(send);
    auto *sendOptions = new QToolButton;
    sendOptions->setIcon(lineIcon("down",Qt::white));
    sendOptions->setFixedSize(28,32);
    sendOptions->setToolTip("发送快捷键");
    sendOptions->setAccessibleName("发送快捷键设置");
    sendOptions->setObjectName("sendOptions");
    sendOptions->setPopupMode(QToolButton::InstantPopup);
    auto *sendMenu = new QMenu(sendOptions);
    auto *sendGroup = new QActionGroup(sendMenu);
    sendGroup->setExclusive(true);
    for (int mode = 0; mode < 2; ++mode) {
        auto *a = sendMenu->addAction(mode ? "Enter 发送" : "Ctrl+Enter 发送");
        a->setCheckable(true); a->setChecked(m_enterSends == bool(mode)); sendGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, mode] {
            m_enterSends = mode; m_settings.setValue("enterSends", m_enterSends);
            m_sendHint->setText(m_enterSends ? "Enter 发送 · Shift+Enter 换行" : "Ctrl+Enter 发送 · Enter 换行");
        });
    }
    sendOptions->setMenu(sendMenu); bottom->addWidget(sendOptions);
    bottom->addWidget(new QSizeGrip(m_chat));
    cpl->addLayout(bottom);
    verticalSplit->addWidget(compose);
    verticalSplit->setSizes({510, 200});
    verticalSplit->setHandleWidth(1);
    verticalSplit->setStretchFactor(0, 3);
    verticalSplit->setStretchFactor(1, 1);
    split->addWidget(conversation);
    split->setHandleWidth(1);
    m_historyPanel = new QWidget;
    auto *hl = new QVBoxLayout(m_historyPanel);
    hl->setContentsMargins(10, 10, 10, 10);
    auto *historyHeader = new QHBoxLayout;
    historyHeader->addWidget(plainLabel("聊天记录"), 1);
    auto *historyClose = new QToolButton; historyClose->setIcon(lineIcon("close")); historyClose->setFixedSize(24,24);
    historyClose->setToolTip("关闭记录侧栏");
    connect(historyClose, &QToolButton::clicked, m_historyPanel, &QWidget::hide);
    historyHeader->addWidget(historyClose); hl->addLayout(historyHeader);
    m_historySearch = new QLineEdit; m_historySearch->setObjectName("historySearch");
    m_historySearch->setPlaceholderText("搜索当前会话记录");
    connect(m_historySearch, &QLineEdit::textChanged, this, &MainWindow::updateHistorySearch);
    hl->addWidget(m_historySearch);
    m_history = new QTextBrowser;
    hl->addWidget(m_history);
    m_historyPanel->setMinimumWidth(220);
    m_historyPanel->setMaximumWidth(280);
    split->addWidget(m_historyPanel);
    m_historyPanel->hide();
    split->setSizes({720, 280});

}
void MainWindow::refreshContacts() {
    QString selected = m_current;
    QSignalBlocker block(m_contacts);
    m_contacts->clear();
    for (auto p : m_peers) {
        auto *i = new QListWidgetItem(p["name"].toString(), m_contacts);
        i->setData(Qt::UserRole, p["id"].toString());
        if (m_notifications && m_notifications->unread(p["id"].toString())) {
            i->setText(i->text()+QString("  (%1)").arg(m_notifications->unread(p["id"].toString())));
            QFont f=i->font(); f.setBold(true);i->setFont(f);
        }
        i->setToolTip(p["host"].toString() + ":" + QString::number(p["port"].toInt()));
    }
    for (auto it = m_groups.begin(); it != m_groups.end(); ++it) {
        auto *i = new QListWidgetItem(it.value()["name"].toString() + "  · 讨论组", m_contacts);
        i->setData(Qt::UserRole, it.key());
        if (m_notifications && m_notifications->unread(it.key())) {
            i->setText(i->text()+QString("  (%1)").arg(m_notifications->unread(it.key())));
            QFont f=i->font(); f.setBold(true);i->setFont(f);
        }
    }
    for (int n = 0; n < m_contacts->count(); ++n)
        if (m_contacts->item(n)->data(Qt::UserRole).toString() == selected)
            m_contacts->setCurrentItem(m_contacts->item(n));
    filterContacts();
}
void MainWindow::filterContacts() {
    int count = 0;
    for (int n = 0; n < m_contacts->count(); ++n) {
        auto *i = m_contacts->item(n);
        bool group = i->data(Qt::UserRole).toString().startsWith("group:");
        bool show = (m_tabs->currentIndex() == 0 || (m_tabs->currentIndex() == 1 && !group) ||
                     (m_tabs->currentIndex() == 2 && group)) &&
                    i->text().contains(m_search->text(), Qt::CaseInsensitive);
        i->setHidden(!show);
        count += show;
    }
    m_count->setText(
        QString("%1 [%2]").arg(m_tabs->currentIndex() == 2 ? "讨论组" : "局域网会话").arg(count));
}
QString MainWindow::currentName() const {
    return m_current.startsWith("group:") ? m_groups.value(m_current)["name"].toString()
                                          : m_peers.value(m_current)["name"].toString();
}
QStringList MainWindow::currentMembers() const {
    QStringList ids;
    if (m_current.startsWith("group:")) {
        for (auto v : m_groups.value(m_current)["members"].toArray())
            if (v.toString() != m_node->id())
                ids << v.toString();
    } else if (!m_current.isEmpty())
        ids << m_current;
    return ids;
}
void MainWindow::selectConversation(QListWidgetItem *item) {
    if (!item)
        return;
    if (!m_current.isEmpty())
        m_drafts[m_current] = m_editor->toPlainText();
    m_current = item->data(Qt::UserRole).toString();
    m_editor->setPlainText(m_drafts.value(m_current));
    m_chatTitle->setText(currentName());
    m_chat->setWindowTitle(currentName());
    renderHistory(false);
    if (m_notifications && m_chat->isVisible() && m_chat->isActiveWindow() && !m_chat->isMinimized()) m_notifications->markRead(m_current);
}
void MainWindow::openConversation() {
    if (m_current.isEmpty()) {
        showError("请先选择联系人或讨论组");
        return;
    }
    m_chat->show();
    m_chat->raise();
    m_chat->activateWindow();
    m_editor->setFocus();
    if(m_notifications) m_notifications->markRead(m_current);
}
void MainWindow::appendMessage(QJsonObject r) {
    auto *bar=m_messages->verticalScrollBar();
    const bool wasAtBottom=bar->maximum()-bar->value()<24;
    const int previousPosition=bar->value();
    const bool mine = r["direction"].toString() == "out";
    const QString who = mine ? "我" : m_peers.value(r["sender_id"].toString(), m_peers.value(m_current))["name"].toString("对方");
    auto *row = new QWidget;
    auto *horizontal = new QHBoxLayout(row); horizontal->setContentsMargins(0,0,0,0); horizontal->setSpacing(10);
    auto *avatar = plainLabel(mine ? "我" : who.left(1));
    avatar->setFixedSize(36,36);avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet(mine ? "background:#2688e8;color:white;border-radius:10px;font-size:15px;" : "background:#dfe9f5;color:#597b9c;border-radius:10px;font-size:15px;");
    auto *column = new QWidget; column->setMaximumWidth(570);
    auto *vertical = new QVBoxLayout(column);vertical->setContentsMargins(0,0,0,0);vertical->setSpacing(6);
    QString time=QDateTime::fromSecsSinceEpoch(qint64(r["timestamp"].toDouble())).toString("hh:mm");
    auto *caption = plainLabel(who+"  "+time); caption->setStyleSheet("color:#8997a6;font-size:12px;");caption->setAlignment(mine?Qt::AlignRight:Qt::AlignLeft);
    vertical->addWidget(caption);
    auto *bubble = new QFrame;
    bubble->setObjectName("messageBubble");
    bubble->setStyleSheet(mine ? "QFrame#messageBubble{background:#dceeff;border:1px solid #cfe5fa;border-radius:10px;}" : "QFrame#messageBubble{background:white;border:1px solid #e6ebf0;border-radius:10px;}");
    auto *body = new QVBoxLayout(bubble);body->setContentsMargins(14,10,14,10);body->setSpacing(8);
    if(!r["text"].toString().isEmpty()) {
        auto *text=new MessageText(r["text"].toString(),m_chatFont);
        body->addWidget(text);
    }
    QImage image;
    if(!r["image_path"].toString().isEmpty()) image.load(r["image_path"].toString());
    if(!image.isNull()) {
        auto *pic=new QToolButton; pic->setObjectName("messageImage");
        auto thumb=QPixmap::fromImage(image).scaled(320,240,Qt::KeepAspectRatio,Qt::SmoothTransformation);
        pic->setIcon(QIcon(thumb));pic->setIconSize(thumb.size());pic->setFixedSize(thumb.size());
        pic->setToolTip("点击查看原图");pic->setCursor(Qt::PointingHandCursor);
        connect(pic,&QToolButton::clicked,this,[this,image] {
            auto *viewer=new QDialog(m_chat);viewer->setAttribute(Qt::WA_DeleteOnClose);
            viewer->setObjectName("imageViewer");viewer->setWindowTitle("图片 · 原始大小");viewer->resize(820,620);
            auto *layout=new QVBoxLayout(viewer);auto *scroll=new QScrollArea;
            auto *original=new QLabel;original->setPixmap(QPixmap::fromImage(image));original->setFixedSize(image.size());
            original->setObjectName("originalImage");scroll->setWidget(original);layout->addWidget(scroll);
            auto *close=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(close);
            connect(close,&QDialogButtonBox::rejected,viewer,&QDialog::close);viewer->show();
        });body->addWidget(pic);
    }
    QRegularExpression rx("https?://[^\\s<>]+",QRegularExpression::CaseInsensitiveOption);
    auto match=rx.globalMatch(r["text"].toString()); int count=0;
    while(match.hasNext()&&count<3) {
        QUrl url(match.next().captured()); if(!url.isValid()||url.host().isEmpty())continue;++count;
        auto *link=new QPushButton(QFontMetrics(font()).elidedText("预览链接 · "+url.host(),Qt::ElideMiddle,270));link->setMaximumWidth(320);link->setToolTip(url.toDisplayString());
        connect(link,&QPushButton::clicked,this,[this,url]{openLink(url);});body->addWidget(link);
    }
    vertical->addWidget(bubble);
    if(mine) horizontal->addStretch(1); else horizontal->addWidget(avatar,0,Qt::AlignTop);
    horizontal->addWidget(column,0,Qt::AlignTop);
    if(mine) horizontal->addWidget(avatar,0,Qt::AlignTop); else horizontal->addStretch(1);
    m_messageLayout->insertWidget(m_messageLayout->count()-1,row);
    if(!m_rebuildingMessages) restoreMessageScroll(previousPosition,wasAtBottom);
}
void MainWindow::restoreMessageScroll(int position, bool bottom) {
    m_scrollRestorePending=true;m_scrollPosition=position;m_scrollFollowBottom=bottom;
    const auto generation=++m_scrollGeneration;
    QTimer::singleShot(0,this,[this,position,bottom,generation] {
        if(generation!=m_scrollGeneration)return;
        m_messageLayout->activate();
        m_messages->widget()->adjustSize();
        QTimer::singleShot(0,this,[this,position,bottom,generation] {
            if(generation!=m_scrollGeneration)return;
            auto *bar=m_messages->verticalScrollBar();bar->setValue(bottom?bar->maximum():position);
        });
    });
}
void MainWindow::renderHistory(bool preservePosition) {
    auto *bar=m_messages->verticalScrollBar();
    const bool layoutPending=m_scrollRestorePending;
    const int oldPosition=layoutPending?m_scrollPosition:bar->value();
    const bool bottom=!preservePosition || (layoutPending?m_scrollFollowBottom:bar->maximum()-oldPosition<24);
    m_rebuildingMessages=true;
    while (m_messageLayout->count()>1) {
        auto *item=m_messageLayout->takeAt(0);delete item->widget();delete item;
    }
    m_history->clear();
    for (auto v : m_node->history(m_current)) {
        auto r = v.toObject();
        appendMessage(r);

    }
    if (m_demo) {
        const auto now=double(QDateTime::currentSecsSinceEpoch());
        appendMessage({{"direction","in"},{"text","下午好，新的聊天界面可以看一下吗？"},{"timestamp",now-120}});
        appendMessage({{"direction","out"},{"text","可以。现在支持群聊、文件共享和截图发送。"},{"timestamp",now-90}});
        appendMessage({{"direction","in"},{"text","收到 👍\n这些是界面演示消息，不会发送到网络。"},{"timestamp",now-60}});
        appendMessage({{"direction","out"},{"text","表情和投屏入口也在输入框上方。"},{"timestamp",now}});
    }
    updateHistorySearch();
    m_rebuildingMessages=false;
    restoreMessageScroll(oldPosition,bottom);
}
bool MainWindow::eventFilter(QObject *object, QEvent *event) {
    if(object==m_messages->viewport() && (event->type()==QEvent::Wheel || event->type()==QEvent::KeyPress)) {++m_scrollGeneration;m_scrollRestorePending=false;}
    if (object == m_chat && event->type() == QEvent::WindowActivate && m_notifications && !m_chat->isMinimized()) m_notifications->markRead(m_current);
    if (object == m_editor && event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            bool send = m_enterSends ? key->modifiers() == Qt::NoModifier
                                     : key->modifiers() == Qt::ControlModifier;
            if (send) { sendMessage(); return true; }
        }
    }
    return QMainWindow::eventFilter(object, event);
}
void MainWindow::updateHistorySearch() {
    m_history->clear();
    const auto query = m_historySearch->text();
    for (auto value : m_node->history(m_current)) {
        auto r = value.toObject();
        QString body = r["text"].toString();
        if (!r["image_path"].toString().isEmpty()) body += " [图片]";
        if (!body.contains(query, Qt::CaseInsensitive)) continue;
        QString who = r["direction"].toString() == "out" ? "我" : m_peers.value(r["sender_id"].toString())["name"].toString("对方");
        QString time = QDateTime::fromSecsSinceEpoch(qint64(r["timestamp"].toDouble())).toString("yyyy-MM-dd hh:mm");
        m_history->append(QString("<p style='color:#7c8b98'>%1 · %2</p><p>%3</p>").arg(who.toHtmlEscaped(),time,body.toHtmlEscaped().replace("\n","<br>")));
    }
    if (m_history->toPlainText().isEmpty()) m_history->setPlainText(query.isEmpty() ? "暂无历史记录" : "没有匹配的记录");
}
void MainWindow::chooseImage() {
    QString path = QFileDialog::getOpenFileName(m_chat, "发送图片", {}, "图片 (*.png *.jpg *.jpeg *.bmp *.webp)");
    if (path.isEmpty()) return;
    QImageReader reader(path);
    QSize size = reader.size();
    if (!size.isValid() || qint64(size.width()) * size.height() > 40000000) {showError("图片无法读取或尺寸过大");return;}
    reader.setScaledSize(size.scaled(1600, 1600, Qt::KeepAspectRatio));
    QImage image = reader.read();
    if (image.isNull()) {showError("无法读取图片");return;}
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"JPEG",80);
    if (bytes.size() > 512*1024) {showError("图片压缩后仍超过512KB，请缩小后发送");return;}
    if (confirm(m_chat,"发送图片",QString("向 %1 发送图片 %2？").arg(currentName(),QFileInfo(path).fileName()))) sendImage(bytes);
}
void MainWindow::sendMessage() {
    if (m_current.isEmpty())
        return;
    QString text = m_editor->toPlainText();
    if (text.trimmed().isEmpty())
        return;
    if (m_demo) {
        appendMessage({{"direction", "out"},
                       {"text", text},
                       {"timestamp", double(QDateTime::currentSecsSinceEpoch())}});
        m_editor->clear();
        return;
    }
    if (!m_node->sendMessage(m_current, text).isEmpty()) {
        m_editor->clear();
        m_drafts.remove(m_current);
        renderHistory();
    }
}
void MainWindow::chooseEmoji() {
    EmojiPicker picker(m_chat);
    connect(&picker, &EmojiPicker::selected, this, [this](QString text) {m_editor->insertPlainText(text);m_editor->setFocus();});
    picker.exec();
}
void MainWindow::chooseFont() {
    bool ok = false;
    auto f = QFontDialog::getFont(&ok, m_chatFont, m_chat, "聊天字体和字号");
    if (!ok)
        return;
    f.setPointSize(qBound(8, f.pointSize(), 24));
    applyChatFont(f);
    m_settings.setValue("fontFamily", f.family());
    m_settings.setValue("fontSize", f.pointSize());
}
void MainWindow::applyChatFont(QFont f) {
    auto *bar=m_messages->verticalScrollBar();const int oldPosition=bar->value();
    const bool bottom=bar->maximum()-oldPosition<24;
    m_chatFont=f;m_editor->setFont(f);
    for(auto *text:m_messages->findChildren<QTextBrowser *>("messageText")) static_cast<MessageText *>(text)->applyFont(f);
    restoreMessageScroll(oldPosition,bottom);
}
void MainWindow::captureScreenshot() {
    if (m_current.isEmpty()) {
        showError("请先选择要发送截图的会话");
        return;
    }
    auto screens = QGuiApplication::screens();
    if (screens.isEmpty())
        return;
    int screenIndex = 0;
    if (screens.size() > 1) {
        QStringList names;
        for (int i = 0; i < screens.size(); ++i)
            names << QString("屏幕 %1 · %2").arg(i + 1).arg(screens[i]->name());
        bool ok = false;
        QString selected = QInputDialog::getItem(m_chat, "截图", "选择屏幕", names, 0, false, &ok);
        if (!ok)
            return;
        screenIndex = names.indexOf(selected);
    }
    m_chat->hide();
    hide();
    QTimer::singleShot(250, this, [this, screen = screens[screenIndex]] {
        CaptureOverlay overlay(screen);
        bool captured = overlay.exec() == QDialog::Accepted;
        show();
        m_chat->show();
        if (!captured)
            return;
        QImage image =
            overlay.selected.toImage().scaled(1600, 1200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QByteArray bytes;
        for (int quality : {85, 70, 50, 35}) {
            bytes.clear();
            QBuffer b(&bytes);
            b.open(QIODevice::WriteOnly);
            image.save(&b, "JPEG", quality);
            if (bytes.size() <= 512 * 1024)
                break;
        }
        if (bytes.size() > 512 * 1024) {
            showError("图片过大，请选择较小区域");
            return;
        }
        QDialog preview(m_chat);
        preview.setWindowTitle("确认发送截图至：" + currentName());
        auto *l = new QVBoxLayout(&preview);
        auto *label = plainLabel("");
        label->setPixmap(
            QPixmap::fromImage(image).scaled(720, 480, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        l->addWidget(label);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Ok)->setText("发送截图");
        connect(buttons, &QDialogButtonBox::accepted, &preview, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &preview, &QDialog::reject);
        l->addWidget(buttons);
        if (preview.exec() == QDialog::Accepted)
            sendImage(bytes);
    });
}
void MainWindow::sendImage(QByteArray jpeg) {
    if (m_demo) {
        showError("演示模式不发送到网络");
        return;
    }
    m_node->sendImage(m_current, jpeg);
    renderHistory();
}
void MainWindow::editSettings() {
    QDialog d(this);
    d.setWindowTitle("设置");
    d.resize(480, 440);
    auto *l = new QVBoxLayout(&d);
    auto *tabs = new QTabWidget;
    l->addWidget(tabs);
    auto *system = new QWidget;
    auto *sl = new QVBoxLayout(system);
    auto *font = new QPushButton("聊天字体和字号…");
    connect(font, &QPushButton::clicked, this, &MainWindow::chooseFont);
    sl->addWidget(font);
    auto *notifications = new QCheckBox("新消息桌面提醒和提示音");
    notifications->setChecked(m_settings.value("notifications",true).toBool());
    connect(notifications, &QCheckBox::toggled, this, [this](bool on) {m_settings.setValue("notifications",on);if(m_notifications)m_notifications->setEnabled(on);});
    sl->addWidget(notifications);
    sl->addWidget(plainLabel("LanLink 0.5.1 · C++ / Qt 6 原生版\n开发预览，非 Python 便携版。"));
    auto *notice = plainLabel("开发预览，未加密、无设备身份认证。\n仅在可信局域网测试，不要将端口映射到公网。"
                              "\n离线待发保存在发送端，双方再次同时在线后送达。");
    notice->setWordWrap(true);
    sl->addWidget(notice);
    sl->addStretch();
    tabs->addTab(system, "系统设置");
    auto *personal = new QWidget;
    auto *fl = new QFormLayout(personal);
    QHash<QString, QLineEdit *> fields;
    for (auto v : QList<QPair<QString, QString>>{{"nickname", "昵称"},
                                                 {"signature", "签名"},
                                                 {"organization", "组织"},
                                                 {"department", "部门"},
                                                 {"location", "所在地"}}) {
        auto *edit = new QLineEdit(m_node->publicProfile()[v.first].toString());
        edit->setMaxLength(v.first == "signature" ? 200 : 80);
        fields[v.first] = edit;
        fl->addRow(v.second, edit);
    }
    auto *visibility = plainLabel("昵称用于局域网发现，以上资料可由局域网联系人请求查看。不要填写敏感信息。");
    visibility->setWordWrap(true);
    fl->addRow(visibility);
    tabs->addTab(personal, "个人设置");
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    l->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &d, [&] {
        QJsonObject p;
        for (auto i = fields.begin(); i != fields.end(); ++i)
            p[i.key()] = i.value()->text();
        if (m_node->updateProfile(p)) {
            m_nickname->setText(m_node->name());
            m_settings.setValue("name", m_node->name());
            d.accept();
        }
    });
    d.exec();
}
void MainWindow::viewProfile() {
    if (m_current.isEmpty() || m_current.startsWith("group:")) {
        showError("请选择单个联系人查看资料");
        return;
    }
    if (m_demo) {
        event("profile_received", {{"peer_id", m_current},
                                   {"profile", QJsonObject{{"nickname", "研发电脑"},
                                                           {"signature", "资料展示为演示数据"},
                                                           {"organization", "研发部"}}}});
        return;
    }
    m_node->fetchProfile(m_current);
}
void MainWindow::addPeer() {
    QDialog d(this);
    d.setWindowTitle("添加设备 IP");
    auto *l = new QFormLayout(&d);
    auto *host = new QLineEdit("192.168.1.");
    auto *port = new QSpinBox;
    port->setRange(1, 65535);
    port->setValue(45870);
    l->addRow("IP 地址", host);
    l->addRow("对方显示的端口", port);
    auto *b = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    l->addRow(b);
    connect(b, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    connect(b, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    if (d.exec() == QDialog::Accepted && !m_demo)
        m_node->addPeer(host->text().trimmed(), quint16(port->value()));
}
void MainWindow::createGroup() {
    QDialog d(this);
    d.setWindowTitle("创建固定成员讨论组");
    auto *l = new QVBoxLayout(&d);
    auto *name = new QLineEdit;
    name->setMaxLength(80);
    name->setPlaceholderText("讨论组名称");
    l->addWidget(name);
    auto *members = new QListWidget;
    for (auto p : m_peers) {
        auto *i = new QListWidgetItem(p["name"].toString(), members);
        i->setData(Qt::UserRole, p["id"].toString());
        if (m_notifications && m_notifications->unread(p["id"].toString())) {
            i->setText(i->text()+QString("  (%1)").arg(m_notifications->unread(p["id"].toString())));
            QFont f=i->font(); f.setBold(true);i->setFont(f);
        }
        i->setCheckState(Qt::Unchecked);
    }
    l->addWidget(members);
    auto *b = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(b, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    connect(b, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    l->addWidget(b);
    if (d.exec() != QDialog::Accepted)
        return;
    QStringList ids;
    for (int i = 0; i < members->count(); ++i)
        if (members->item(i)->checkState() == Qt::Checked)
            ids << members->item(i)->data(Qt::UserRole).toString();
    if (ids.isEmpty()) {
        showError("至少选择一名成员");
        return;
    }
    m_node->createGroup(ids, name->text());
}
void MainWindow::event(QString kind, QJsonObject data) {
    if (kind == "peer") {
        m_peers[data["id"].toString()] = data;
        refreshContacts();
    } else if (kind == "group") {
        m_groups["group:" + data["id"].toString()] = data;
        refreshContacts();
    } else if (kind == "message") {
        const QString conversation = data["peer_id"].toString();
        if(m_notifications && data["direction"].toString()=="in") {
            QString name = conversation.startsWith("group:") ? m_groups.value(conversation)["name"].toString("讨论组") : m_peers.value(conversation)["name"].toString("联系人");
            m_notifications->incoming(conversation, name, conversation==m_current && m_chat->isVisible() && m_chat->isActiveWindow() && !m_chat->isMinimized());
        }
        if (data["peer_id"].toString() == m_current)
            renderHistory();
    } else if (kind == "error")
        showError(data["message"].toString());
    else if (kind == "profile_received") {
        QDialog d(this);
        d.setWindowTitle("联系人资料（未验证身份）");
        auto *l = new QFormLayout(&d);
        auto p = data["profile"].toObject();
        for (auto v : QList<QPair<QString, QString>>{{"nickname", "昵称"},
                                                     {"signature", "签名"},
                                                     {"organization", "组织"},
                                                     {"department", "部门"},
                                                     {"location", "所在地"}}) {
            auto *label = plainLabel(p[v.first].toString());
            label->setWordWrap(true);
            label->setTextInteractionFlags(Qt::TextSelectableByMouse);
            l->addRow(v.second, label);
        }
        auto *b = new QDialogButtonBox(QDialogButtonBox::Close);
        connect(b, &QDialogButtonBox::rejected, &d, &QDialog::reject);
        l->addRow(b);
        d.exec();
    }
}
void MainWindow::showError(QString message) {
    statusBar()->showMessage(message, 15000);
    QMessageBox b(QMessageBox::Information, "LanLink", message, QMessageBox::Ok, this);
    b.setTextFormat(Qt::PlainText);
    b.exec();
}
void MainWindow::populateDemo() {
    m_peers["11111111111111111111111111111111"] = {{"id", "11111111111111111111111111111111"},
                                                   {"name", "研发电脑"},
                                                   {"host", "192.168.1.20"},
                                                   {"port", 45870}};
    m_peers["22222222222222222222222222222222"] = {{"id", "22222222222222222222222222222222"},
                                                   {"name", "测试工作站"},
                                                   {"host", "192.168.1.21"},
                                                   {"port", 45870}};
    m_groups["group:33333333333333333333333333333333"] = {
        {"id", "33333333333333333333333333333333"},
        {"name", "研发讨论组"},
        {"members",
         QJsonArray{m_node->id(), "11111111111111111111111111111111", "22222222222222222222222222222222"}}};
    m_address->setText("C++ / Qt 6 · 演示数据\n未连接网络");
    refreshContacts();
    m_contacts->setCurrentRow(0);
}
void MainWindow::saveDemoScreenshot(QString path, QString view) {
    if (view == "chat" || view == "history") {
        openConversation();
        if (view == "history")
            m_historyPanel->show();
        QApplication::processEvents();
        m_chat->grab().save(path);
    } else if (view == "transfers") {
        m_transfers->show();
        m_transfers->grab().save(path);
    } else
        grab().save(path);
}
void MainWindow::buildFiles() {
    m_transfers = new QDialog(this);
    m_transfers->setWindowTitle("文件传输");
    m_transfers->resize(850, 520);
    auto *l = new QVBoxLayout(m_transfers);
    auto *notice =
        plainLabel("接收前需确认。暂停后可选择同一目录续传；完成后核对 SHA256。不要传输敏感文件。");
    notice->setWordWrap(true);
    l->addWidget(notice);
    m_transferTable = new QTableWidget(0, 4);
    m_transferTable->setHorizontalHeaderLabels({"文件 / 任务", "进度", "状态", "操作"});
    m_transferTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_transferTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_transferTable->setColumnWidth(3, 150);
    m_transferTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    l->addWidget(m_transferTable);
}
void MainWindow::transferProgress(QString id, qint64 done, qint64 total, QString status) {
    if (!m_rows.contains(id)) {
        int row = m_transferTable->rowCount();
        m_transferTable->insertRow(row);
        m_rows[id] = row;
        QString name = m_offers.value(id)["name"].toString(id.left(12));
        m_transferTable->setItem(row, 0, new QTableWidgetItem(name));
        m_transferTable->setItem(row, 1, new QTableWidgetItem);
        m_transferTable->setItem(row, 2, new QTableWidgetItem);
        auto *actions = new QWidget;
        auto *al = new QHBoxLayout(actions);
        al->setContentsMargins(0, 0, 0, 0);
        auto *pause = new QPushButton("暂停");
        connect(pause, &QPushButton::clicked, this, [this, id] { m_files->pauseFile(id); });
        al->addWidget(pause);
        auto *resume = new QPushButton("续传");
        connect(resume, &QPushButton::clicked, this, [this, id] {
            QString dir = m_saveDirs.value(id);
            if (dir.isEmpty())
                dir = QFileDialog::getExistingDirectory(m_transfers, "选择此前的接收目录");
            if (!dir.isEmpty()) {
                m_saveDirs[id] = dir;
                m_files->acceptFile(id, dir);
            }
        });
        al->addWidget(resume);
        m_transferTable->setCellWidget(row, 3, actions);
    }
    int row = m_rows[id];
    m_transferTable->item(row, 1)->setText(
        QString("%1%  %2 / %3").arg(total ? done * 100 / total : 0).arg(done).arg(total));
    m_transferTable->item(row, 2)->setText(status);
}
void MainWindow::offerReceived(QJsonObject offer) {
    QString id = offer["id"].toString();
    m_offers[id] = offer;
    transferProgress(id, 0, qint64(offer["size"].toDouble()), "等待确认");
    QString sender = m_peers.value(offer["sender"].toObject()["id"].toString())["name"].toString("联系人");
    if (!confirm(this, "接收文件",
                 QString("%1 请求发送文件：\n%2\n%3 字节\n是否选择保存目录并接收？")
                     .arg(sender, offer["name"].toString())
                     .arg(qint64(offer["size"].toDouble())))) {
        m_files->rejectFile(id);
        transferProgress(id, 0, qint64(offer["size"].toDouble()), "已拒绝");
        return;
    }
    QString dir = QFileDialog::getExistingDirectory(this, "接收文件保存目录");
    if (dir.isEmpty()) {
        m_files->rejectFile(id);
        return;
    }
    m_saveDirs[id] = dir;
    m_files->acceptFile(id, dir);
    m_transfers->show();
}
void MainWindow::sendFile() {
    auto members = currentMembers();
    if (members.isEmpty()) {
        showError("请先选择联系人或讨论组");
        return;
    }
    QString file = QFileDialog::getOpenFileName(m_chat, "选择发送文件");
    if (file.isEmpty())
        return;
    if (m_demo) {
        showError("演示模式不发送到网络");
        return;
    }
    if (members.size() > 1 &&
        !confirm(m_chat, "群发文件",
                 QString("向当前讨论组的 %1 名成员分别发送文件邀请？").arg(members.size())))
        return;
    for (auto id : members)
        m_files->offerFile(id, file);
    m_transfers->show();
}
void MainWindow::buildSharing() {
    m_shares = new QDialog(this);
    m_shares->setWindowTitle("只读文件共享");
    m_shares->resize(800, 600);
    auto *l = new QVBoxLayout(m_shares);
    auto *notice = plainLabel("访问码持有者可读取已发布目录。仅在可信局域网使用，撤销共享将终止相关下载。");
    notice->setWordWrap(true);
    l->addWidget(notice);
    auto *own = new QGroupBox("我发布的目录（本次运行）");
    auto *ol = new QVBoxLayout(own);
    m_published = new QListWidget;
    ol->addWidget(m_published);
    auto *buttons = new QHBoxLayout;
    auto *publish = new QPushButton("发布目录");
    connect(publish, &QPushButton::clicked, this, [this] {
        QString path = QFileDialog::getExistingDirectory(m_shares, "选择只读共享目录");
        if (path.isEmpty() ||
            !confirm(m_shares, "发布只读共享", "持有访问码的局域网联系人将能读取此目录中的文件。确认发布？"))
            return;
        QString token = m_files->publishShare(path);
        if (token.isEmpty())
            return;
        auto *item = new QListWidgetItem(path + "\n访问码：" + token, m_published);
        item->setData(Qt::UserRole, token);
    });
    buttons->addWidget(publish);
    auto *copy = new QPushButton("复制访问码");
    connect(copy, &QPushButton::clicked, this, [this] {
        if (auto *i = m_published->currentItem())
            QApplication::clipboard()->setText(i->data(Qt::UserRole).toString());
    });
    buttons->addWidget(copy);
    auto *revoke = new QPushButton("撤销共享");
    connect(revoke, &QPushButton::clicked, this, [this] {
        if (auto *i = m_published->currentItem()) {
            m_files->revokeShare(i->data(Qt::UserRole).toString());
            delete m_published->takeItem(m_published->row(i));
        }
    });
    buttons->addWidget(revoke);
    ol->addLayout(buttons);
    l->addWidget(own);
    auto *remote = new QGroupBox("浏览当前联系人共享");
    auto *rl = new QVBoxLayout(remote);
    m_shareCode = new QLineEdit;
    m_shareCode->setPlaceholderText("对方提供的访问码");
    m_sharePath = new QLineEdit;
    m_sharePath->setPlaceholderText("相对目录（留空为根目录）");
    rl->addWidget(m_shareCode);
    rl->addWidget(m_sharePath);
    auto *browse = new QPushButton("浏览当前联系人");
    connect(browse, &QPushButton::clicked, this, &MainWindow::browseShare);
    rl->addWidget(browse);
    m_shareList = new QListWidget;
    connect(m_shareList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *i) {
        auto d = i->data(Qt::UserRole).toJsonObject();
        QString name = d["name"].toString();
        if (d["directory"].toBool() || d["is_dir"].toBool() || d["type"].toString() == "directory") {
            QString p = m_sharePath->text();
            m_sharePath->setText(p.isEmpty() ? name : p + "/" + name);
            browseShare();
        } else
            downloadShare();
    });
    rl->addWidget(m_shareList);
    auto *download = new QPushButton("下载选中文件");
    connect(download, &QPushButton::clicked, this, &MainWindow::downloadShare);
    rl->addWidget(download);
    l->addWidget(remote, 1);
}
void MainWindow::browseShare() {
    if (m_current.isEmpty() || m_current.startsWith("group:")) {
        showError("请在联系人列表选择一台设备");
        return;
    }
    m_sharePeer = m_current;
    m_files->listShare(m_current, m_shareCode->text().trimmed(), m_sharePath->text().trimmed());
}
void MainWindow::downloadShare() {
    auto *i = m_shareList->currentItem();
    if (!i || m_sharePeer.isEmpty())
        return;
    auto d = i->data(Qt::UserRole).toJsonObject();
    if (d["directory"].toBool() || d["is_dir"].toBool() || d["type"].toString() == "directory") {
        showError("请进入目录后选择单个文件");
        return;
    }
    QString dir = QFileDialog::getExistingDirectory(m_shares, "下载文件保存目录");
    if (dir.isEmpty())
        return;
    QString rel = m_sharePath->text().trimmed();
    if (!rel.isEmpty())
        rel += "/";
    rel += d["name"].toString();
    m_files->downloadShare(m_sharePeer, m_shareCode->text().trimmed(), rel, dir);
    m_transfers->show();
}
void MainWindow::setupServices() {
    m_files = new FileTransfer(m_node, this);
    m_screen = new ScreenService(m_node, this);
    m_preview = new LinkPreview(this);
    m_node->serviceHandler = [this](QTcpSocket *s, QJsonObject r, QString h) {
        return m_files->handle(s, r, h) || m_screen->handle(s, r, h);
    };
    connect(m_files, &FileTransfer::offerReceived, this, &MainWindow::offerReceived);
    connect(m_files, &FileTransfer::progress, this, &MainWindow::transferProgress);
    connect(m_files, &FileTransfer::error, this, &MainWindow::showError);
    connect(m_files, &FileTransfer::completed, this, [this](QString id, QString path) {
        statusBar()->showMessage("已接收并校验：" + QFileInfo(path).fileName(), 10000);
    });
    connect(m_files, &FileTransfer::shareListed, this, [this](QJsonArray entries) {
        m_shareList->clear();
        for (auto v : entries) {
            auto d = v.toObject();
            bool dir = d["directory"].toBool() || d["is_dir"].toBool() || d["type"].toString() == "directory";
            auto *i = new QListWidgetItem((dir ? "[目录] " : "") + d["name"].toString(), m_shareList);
            i->setData(Qt::UserRole, d);
        }
    });
    for (auto v : m_files->incomingOffers()) {
        auto o = v.toObject();
        m_offers[o["id"].toString()] = o;
        transferProgress(o["id"].toString(), 0, qint64(o["size"].toDouble()), "待恢复");
    }
    m_captureTimer = new QTimer(this);
    m_captureTimer->setInterval(200);
    connect(m_captureTimer, &QTimer::timeout, this, [this] {
        if (!m_captureScreen || !m_screen->sending())
            return;
        QImage img = m_captureScreen->grabWindow(0).toImage().scaled(1280, 720, Qt::KeepAspectRatio,
                                                                     Qt::SmoothTransformation);
        QByteArray jpeg;
        QBuffer b(&jpeg);
        b.open(QIODevice::WriteOnly);
        img.save(&b, "JPEG", 65);
        m_screen->publish(jpeg);
    });
    connect(m_screen, &ScreenService::event, this, [this](QString kind, QJsonObject d) {
        QString session = d["session"].toString();
        if (kind == "screen_offer") {
            QString peer = m_peers.value(d["peer_id"].toString())["name"].toString("局域网联系人");
            m_screen->decide(session,
                             confirm(this, "接收投屏",
                                     peer + " 请求向你投屏。接收后只显示画面，不允许远程控制。是否接收？"));
        } else if (kind == "screen_started" && d["direction"].toString() == "in") {
            auto *dialog = new QDialog(this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setWindowTitle("接收投屏 · " + m_peers.value(d["peer_id"].toString())["name"].toString());
            dialog->resize(900, 560);
            auto *l = new QVBoxLayout(dialog);
            auto *label = plainLabel("等待画面…");
            label->setAlignment(Qt::AlignCenter);
            label->setMinimumSize(320, 180);
            l->addWidget(label, 1);
            auto *stop = new QPushButton("停止接收");
            l->addWidget(stop);
            m_receivers[session] = dialog;
            m_frameLabels[session] = label;
            connect(stop, &QPushButton::clicked, dialog, &QDialog::close);
            connect(dialog, &QDialog::finished, this, [this, session] {
                m_screen->stop(session);
                m_receivers.remove(session);
                m_frameLabels.remove(session);
            });
            dialog->show();
        } else if (kind == "screen_stopped") {
            if (m_frameLabels.value(session))
                m_frameLabels.value(session)->clear();
            if (m_receivers.value(session))
                m_receivers.value(session)->close();
        } else if (kind == "screen_status")
            statusBar()->showMessage(d["message"].toString(), 10000);
    });
    connect(m_screen, &ScreenService::frameReceived, this, [this](QString session, QByteArray jpeg) {
        if (auto label = m_frameLabels.value(session)) {
            QPixmap pix;
            pix.loadFromData(jpeg, "JPEG");
            label->setPixmap(pix.scaled(label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    });
}
void MainWindow::openScreen() {
    auto members = currentMembers();
    if (members.isEmpty()) {
        showError("请先选择会话");
        return;
    }
    if (m_screenDialog) {
        m_screenDialog->show();
        m_screenDialog->raise();
        return;
    }
    auto *d = new QDialog(m_chat);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->setWindowTitle("会话投屏 · " + currentName());
    d->resize(520, 350);
    m_screenDialog = d;
    auto *l = new QVBoxLayout(d);
    auto *notice = plainLabel(
        "每位成员分别确认后才接收画面。当前为低帧率预览，无声音、无远控。停止或关闭此窗口结束本次投屏。");
    notice->setWordWrap(true);
    l->addWidget(notice);
    auto *list = new QListWidget;
    for (auto id : members)
        new QListWidgetItem(m_peers.value(id)["name"].toString(id), list);
    l->addWidget(list);
    auto *screens = new QComboBox;
    for (auto *screen : QGuiApplication::screens())
        screens->addItem(screen->name());
    l->addWidget(screens);
    auto *state = plainLabel("未开始");
    l->addWidget(state);
    auto *actions = new QHBoxLayout;
    auto *start = new QPushButton("开始投屏");
    auto *stop = new QPushButton("停止投屏");
    stop->setStyleSheet("color:#bd251e;font-weight:bold");
    actions->addWidget(start);
    actions->addWidget(stop);
    l->addLayout(actions);
    connect(start, &QPushButton::clicked, d, [this, d, start, screens, state, members] {
        if (m_demo) {
            state->setText("演示模式，不发送画面");
            return;
        }
        auto all = QGuiApplication::screens();
        if (screens->currentIndex() < 0 || screens->currentIndex() >= all.size())
            return;
        if (!confirm(d, "确认投屏", "所选屏幕上的全部可见内容会传给同意接收的成员。请关闭敏感窗口后开始。"))
            return;
        m_captureScreen = all[screens->currentIndex()];
        auto sessions = m_screen->startMany(members);
        if (!sessions.isEmpty()) {
            m_captureTimer->start();
            state->setText("正在投屏 / 等待成员确认");
            start->setEnabled(false);
        }
    });
    auto stopFn = [this, state, start] {
        m_captureTimer->stop();
        m_screen->stopAll();
        state->setText("已停止");
        start->setEnabled(true);
    };
    connect(stop, &QPushButton::clicked, d, stopFn);
    connect(d, &QDialog::finished, this, [this] {
        m_captureTimer->stop();
        m_screen->stopAll();
        m_captureScreen.clear();
        m_screenDialog.clear();
    });
    auto *a = new QAction(d);
    a->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(a, &QAction::triggered, d, stopFn);
    d->addAction(a);
    d->show();
}
void MainWindow::openLink(const QUrl &url) {
    if (!LinkPreview::allowedUrl(url)) {
        showError("此链接格式不支持预览");
        return;
    }
    auto *d = new QDialog(m_chat);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->setWindowTitle("链接预览");
    auto *l = new QVBoxLayout(d);
    auto *heading = plainLabel(url.host());
    auto *description = plainLabel("正在加载…");
    description->setWordWrap(true);
    description->setMinimumWidth(400);
    l->addWidget(heading);
    l->addWidget(description);
    auto *open = new QPushButton("在浏览器打开");
    l->addWidget(open);
    connect(open, &QPushButton::clicked, d, [this, url] {
        if (confirm(m_chat, "打开网站", QString("将在默认浏览器打开：\n%1").arg(url.toDisplayString())))
            QDesktopServices::openUrl(url);
    });
    d->show();
    QPointer<QLabel> h = heading, p = description;
    m_preview->fetch(url, [h, p](QString title, QString text, QString error) {
        if (!h || !p)
            return;
        h->setText(title);
        p->setText(error.isEmpty() ? text : "无法预览：" + error);
    });
}
