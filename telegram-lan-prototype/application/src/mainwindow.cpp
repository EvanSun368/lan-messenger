#include "telegram_input_adapter.h"
#include "telegram_dialogs_model.h"
#include "telegram_row_painter.h"
#include "telegram_column_layout.h"
#include "telegram_bubble_frame.h"
#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_message_text.h"
#include "telegram_full_input_adapter.h"
#include "telegram_original_controls.h"
#include "telegram_original_title.h"
#include "telegram_profile_box.h"
#include "telegram_contact_boxes.h"
#include "telegram_transfer_list.h"
#include "telegram_sharing_box.h"
#include "telegram_lan_history_list.h"
#include "telegram_dialogs_inner.h"
#include "ui/widgets/scroll_area.h"
#endif
#include "mainwindow.h"
#include "mailwindow.h"
#include "contactmailbook.h"
#include "chatmailexport.h"
#include "chatnotifications.h"
#include "deliverypresentation.h"
#include "emojipicker.h"
#include "filetransfer.h"
#include "transfercard.h"
#include "linkpreview.h"
#include "node.h"
#include "screenservice.h"
#include <QtNetwork>
#include <QtWidgets>

namespace {
#ifdef LANLINK_FULL_TELEGRAM_UI
// Upstream Menu(QMenu*) takes ownership of the supplied menu. Each transient
// popup must own a copy, not the persistent command model used by the host.
QMenu *popupCommandCopy(QMenu *source) {
    auto *copy = new QMenu;
    copy->setTitle(source->title());
    for(auto *original : source->actions()) {
        if(original->isSeparator()) {copy->addSeparator();continue;}
        QAction *action=nullptr;
        if(original->menu()) {
            auto *submenu=popupCommandCopy(original->menu());
            submenu->setParent(copy);
            action=copy->addMenu(submenu);
        } else {
            action=copy->addAction(original->icon(),original->text());
            const QPointer<QAction> command(original);
            QObject::connect(action,&QAction::triggered,copy,[command]{if(command)command->trigger();});
        }
        action->setEnabled(original->isEnabled());
        action->setVisible(original->isVisible());
        action->setCheckable(original->isCheckable());
        action->setChecked(original->isChecked());
    }
    return copy;
}
#endif
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
enum RosterRole { PreviewRole=Qt::UserRole+20, TimeRole, UnreadRole, GroupRole, ActivityRole, DetailsRole, SortRole };
class RosterItem : public QListWidgetItem {
public:
    using QListWidgetItem::QListWidgetItem;
    bool operator<(const QListWidgetItem &other) const override {
        if(data(SortRole).toBool() && data(ActivityRole).toDouble()!=other.data(ActivityRole).toDouble()) return data(ActivityRole).toDouble()>other.data(ActivityRole).toDouble();
        return QString::localeAwareCompare(text(),other.text())<0;
    }
};
class RosterDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
        return {280, TelegramLan::TelegramDialogRowHeight};
    }
    void paint(QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &i) const override {
        // LAN model -> backend-neutral Telegram PaintRow adaptation. No Session,
        // peer lookup or network access occurs while the viewport is painting.
        const bool recent = o.widget && o.widget->property("rosterMode").toInt() == 0;
        TelegramLan::DialogPresentation row;
        row.id = i.data(Qt::UserRole).toString();
        row.name = i.data(Qt::DisplayRole).toString();
        row.preview = i.data(recent ? PreviewRole : DetailsRole).toString();
        row.date = recent ? i.data(TimeRole).toString() : QString();
        row.unread = i.data(UnreadRole).toInt();
        row.avatarColor = i.data(GroupRole).toBool() ? QColor("#a695e7") : QColor("#65aadd");
        TelegramLan::DialogPaintContext context;
        context.width = o.rect.width();
        context.active = o.state.testFlag(QStyle::State_Selected);
        context.selected = o.state.testFlag(QStyle::State_MouseOver);
        p->save();
        p->setClipRect(o.rect, Qt::IntersectClip);
        p->translate(o.rect.topLeft());
        TelegramLan::PaintTelegramDialogRow(*p, row, context);
        p->restore();
    }
};
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
#ifdef LANLINK_FULL_TELEGRAM_UI
using MessageText = TelegramMessageText;
#else
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
#endif
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
        label->setStyleSheet(modern ? "color:#26384b;font-size:16px;font-weight:600;" : "color:#7c8792;font-weight:500;");
        l->addWidget(label, 1);
        for (auto s : {QString("−"), QString("□"), QString("×")}) {
            auto *b = new QPushButton;
            b->setIcon(lineIcon(s == "×" ? "close" : s == "□" ? "maximize" : "minimize", QColor("#7a8897")));
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

MainWindow::MainWindow(QString storage, quint16 port, bool demo, QWidget *parent, NetworkPolicy network)
    : QMainWindow(parent), m_settings("LanLink", "LanLinkTelegramLab"), m_demo(demo) {
    if (storage.isEmpty())
        storage = QDir::homePath() + "/.lanlink-telegram-lab";
    m_node = new Node(
        m_settings.value("name", qEnvironmentVariable("USERNAME", qEnvironmentVariable("USER", "我的电脑")))
            .toString(),
        storage, this);
    // A rejected requested policy must never cause an implicit LAN startup.
    QString policyError;
    const bool networkConfigured = m_node->setNetworkPolicy(network, &policyError);
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
    m_notifications = new ChatNotifications(this, !m_demo);
    m_notifications->setEnabled(m_settings.value("notifications", true).toBool());
    connect(m_notifications, &ChatNotifications::unreadChanged, this, [this] {
        // Selection may mark a conversation read. Never delete/rebuild its
        // QListWidgetItem synchronously inside currentItemChanged delivery.
        if(!m_unreadRefreshPending) {
            m_unreadRefreshPending=true;
            QTimer::singleShot(0,this,[this]{m_unreadRefreshPending=false;refreshContacts();});
        }
        setWindowTitle(QString("LanLink Telegram 源码集成实验版%1").arg(m_notifications->totalUnread() ? QString(" · %1 条未读").arg(m_notifications->totalUnread()) : QString()));
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
    else if (!networkConfigured)
        QTimer::singleShot(0,this,[this,policyError]{showError(policyError);});
    else if (m_node->start(port))
        m_address->setText(network.vpnOnly
            ? QString("VPN %1:%2\n应用身份未验证").arg(network.localAddress.toString()).arg(m_node->port())
            : QString("局域网端口 %1\n接收文件前需确认").arg(m_node->port()));
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        int waiting=0, rejected=0;
        for (const auto &row : m_node->pendingMessages()) {
            if (row.toObject()["error"].toString().startsWith("rejected:")) ++rejected;
            else ++waiting;
        }
        QString summary=(m_node->networkPolicy().vpnOnly
            ? QString("VPN 专用 · 应用未加密/身份未验证  |  待发 %1")
            : QString("未加密 · 仅限可信局域网  |  待发 %1")).arg(waiting);
        if (m_node->networkPolicy().vpnOnly && !m_node->port()) {
            summary.prepend("连接已停止 · ");
            m_address->setText("VPN 连接已停止\n请恢复 VPN 后重启应用");
        }
        if (rejected) summary += QString(" · 已拒收 %1").arg(rejected);
        statusBar()->showMessage(summary);
    });
    timer->start(2000);
    // Only previously enabled, credential-ready mail accounts poll on startup.
    if (!m_demo) m_mailWindow = new MailWindow({}, this);
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
#ifdef LANLINK_FULL_TELEGRAM_UI
    resize(1080, 760);
    setMinimumSize(380, 600);
#else
    resize(380, 800);
    setMinimumSize(340, 600);
#endif
    setStyleSheet(
        "QMainWindow,QDialog{background:#ffffff;color:#263442;} "
        "QWidget#chatTitleBar{background:white;border-bottom:1px solid #e9edf2;} "
        "QScrollArea#messages{border:0;background:#f3f5f8;} QWidget#messageCanvas{background:#f3f5f8;} "
        "QWidget#titleBar,QWidget#profileHeader,QWidget#footerBar{background:#ffffff;} "
        "QPushButton{padding:5px 14px;min-height:20px;border:1px solid #cbd6df;border-radius:3px;background:#ffffff;color:#344454;} "
        "QPushButton:hover{background:#edf6fd;border-color:#9dc8e9;} QPushButton:pressed{background:#dceefd;} "
        "QPushButton:disabled{color:#a9b3bc;background:#f7f8fa;border-color:#e4e9ee;} "
        "QPushButton#titleButton{background:transparent;border:0;padding:0;min-height:0;} "
        "QPushButton#titleButton:hover{background:rgba(255,255,255,30);} "
        "QLabel#nickname{font-size:19px;font-weight:600;color:white;} QLabel#profileAddress{color:#e6f4ff;} "
        "QPushButton#profileAction{border:0;color:white;background:transparent;padding:4px 8px;} "
        "QTabBar::tab{padding:12px 16px;background:white;border-bottom:2px solid white;color:#667787;} "
        "QTabBar::tab:selected{border-bottom:2px solid #2298ee;color:#168bdd;} "
        "QListWidget{border:0;background:white;} QListWidget::item{padding:0px;} "
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
        "QStatusBar{background:#ffffff;color:#87939e;font-size:11px;border-top:1px solid #eef0f2;}");
    auto *root = new QWidget;
    setCentralWidget(root);
    auto *outer = new QVBoxLayout(root);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
#ifndef LANLINK_FULL_TELEGRAM_UI
    outer->addWidget(new TitleBar(this, "LanLink 局域网通信"));
#endif
    auto *profile = new QWidget;
    profile->setObjectName("profileHeader");
    auto *pl = new QHBoxLayout(profile);
    pl->setContentsMargins(12, 8, 12, 8);
    pl->setSpacing(10);
    // Retain identity labels for settings updates; the compact navigation menu
    // replaces the previous always-visible profile banner.
    m_nickname = plainLabel(m_node->name()); m_nickname->setParent(profile); m_nickname->setObjectName("nickname"); m_nickname->hide();
    m_address = plainLabel("局域网通信"); m_address->setParent(profile); m_address->setObjectName("profileAddress"); m_address->hide();
#ifdef LANLINK_FULL_TELEGRAM_UI
    auto *navigation = telegramHamburgerButton(profile);
#else
    auto *navigation = new QToolButton(profile);
#endif
    navigation->setObjectName("navigationMenu");
#ifndef LANLINK_FULL_TELEGRAM_UI
    navigation->setText(QString::fromUtf8("☰"));
#endif
    navigation->setAccessibleName("主菜单"); navigation->setToolTip("主菜单");
    navigation->setFixedSize(40,40);
#ifndef LANLINK_FULL_TELEGRAM_UI
    navigation->setStyleSheet("font-size:24px;color:#768492;");
    navigation->setPopupMode(QToolButton::InstantPopup);
#endif
    auto *menu = new QMenu(navigation);
    menu->setObjectName("mainNavigationMenu");
    auto *identityAction=menu->addAction(m_node->name()); identityAction->setEnabled(false);
    connect(menu,&QMenu::aboutToShow,this,[this,identityAction]{identityAction->setText(m_node->name());});
    menu->addAction("个人资料与设置…", this, &MainWindow::editSettings);
    menu->addAction("网络连接诊断…", this, &MainWindow::showNetworkDiagnostics);
    menu->addSeparator();
    menu->addAction("联系人 / 局域网设备",this,[this]{m_tabs->setCurrentIndex(1);});
    menu->addAction("添加设备 IP…", this, &MainWindow::addPeer);
    menu->addAction("新建讨论组…", this, &MainWindow::createGroup);
    menu->addSeparator();
    menu->addAction("文件传输记录",this,[this]{showTransfers();});
    menu->addAction("共享文件",this,[this]{showSharing();});
    menu->addAction("邮箱（独立模块）…",this,[this]{showMail();});
#ifdef LANLINK_FULL_TELEGRAM_UI
    navigation->setClickedCallback([this,navigation,menu,identityAction] {
        identityAction->setText(m_node->name());
        auto *popup=new Ui::PopupMenu(navigation,popupCommandCopy(menu));
        popup->deleteOnHide(true);
        popup->popup(navigation->mapToGlobal(QPoint(0,navigation->height())));
    });
#else
    navigation->setMenu(menu);
#endif
    pl->addWidget(navigation);
    m_search = new TelegramSearchField;
    m_search->setObjectName("search");
    m_search->setPlaceholderText("搜索");
    m_search->setClearButtonEnabled(true);
#ifndef LANLINK_FULL_TELEGRAM_UI
    m_search->setStyleSheet("QLineEdit{background:#f1f3f5;border:0;border-radius:18px;padding:9px 14px;}");
#endif
    telegramSearchChanged(m_search,this,[this](const QString &){filterContacts();});
    pl->addWidget(m_search,1);
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
    m_count->setStyleSheet("background:white;padding:6px 18px;color:#8a96a5;font-size:12px;");
    outer->addWidget(m_count);
    m_contacts = new QListWidget;
    m_contacts->setObjectName("contacts");
    m_contacts->setItemDelegate(new RosterDelegate(m_contacts));
    m_contacts->setMouseTracking(true);
    m_contacts->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_contacts->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(m_contacts, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *i) { selectConversation(i); });
    connect(m_contacts, &QListWidget::itemDoubleClicked, this, [this] { openConversation(); });
#ifdef LANLINK_FULL_TELEGRAM_UI
    connect(m_contacts,&QListWidget::itemClicked,this,[this]{openConversation();});
#endif
#ifdef LANLINK_FULL_TELEGRAM_UI
    m_contacts->setParent(root);
    m_contacts->hide();
    m_contacts->setFocusPolicy(Qt::NoFocus);
    m_dialogs=new TelegramLan::DialogsView(root,m_contacts->model(),m_contacts->selectionModel(),
        [this](const QModelIndex &i)->std::optional<TelegramLan::DialogPresentation> {
            auto *item=m_contacts->item(i.row());
            if(!item || item->isHidden())return std::nullopt;
            const bool recent=m_tabs->currentIndex()==0;
            TelegramLan::DialogPresentation row;
            row.id=i.data(Qt::UserRole).toString();row.name=i.data(Qt::DisplayRole).toString();
            row.preview=i.data(recent?PreviewRole:DetailsRole).toString();
            row.date=recent?i.data(TimeRole).toString():QString();row.unread=i.data(UnreadRole).toInt();
            row.avatarColor=i.data(GroupRole).toBool()?QColor("#a695e7"):QColor("#65aadd");
            return row;
        });
    m_dialogs->setObjectName("telegramDialogs");
    m_dialogs->activated=[this](const QModelIndex &){openConversation();};
    outer->addWidget(m_dialogs,1);
#else
    outer->addWidget(m_contacts, 1);
#endif
    statusBar()->showMessage("未加密 · 仅限可信局域网");
    m_chat = new QDialog(this, Qt::Window | Qt::FramelessWindowHint);
    m_chat->setObjectName("chatWindow");
    m_chat->resize(1020, 754);
#ifdef LANLINK_FULL_TELEGRAM_UI
    m_chat->setWindowFlags(Qt::Widget);
    m_chat->setMinimumSize(0, 0);
#else
    m_chat->setMinimumSize(760, 540);
#endif
    auto *co = new QVBoxLayout(m_chat);
    co->setContentsMargins(1, 1, 1, 1);
    co->setSpacing(0);
#ifdef LANLINK_FULL_TELEGRAM_UI
    auto *chatHeading = new QWidget;
    chatHeading->setObjectName("chatTitleBar");
    auto *headingLayout = new QHBoxLayout(chatHeading);
    auto *back = telegramBackButton(chatHeading);
    back->setObjectName("backToConversations");
    back->setToolTip("返回会话列表");
    back->setFixedSize(40,40);
    back->setClickedCallback([this] {m_showConversation=false; updateColumns();});
    headingLayout->addWidget(back);
    m_chatTitle=plainLabel("选择联系人开始聊天");
    auto *headingNames=new QVBoxLayout;
    headingNames->setSpacing(3);
    m_chatTitle->setStyleSheet("font-size:15px;font-weight:600;color:#263442;");
    m_chatTitle->setMinimumWidth(0);
    m_chatTitle->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    m_chatTitle->setToolTip(m_chatTitle->text());
    headingNames->addWidget(m_chatTitle);
    m_chatStatus=plainLabel("局域网聊天 · 文件接收需确认");
    m_chatStatus->setObjectName("chatStatus");
    m_chatStatus->setMinimumWidth(0);
    m_chatStatus->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    m_chatStatus->setToolTip(m_chatStatus->text());
    m_chatStatus->setStyleSheet("font-size:12px;color:#8a98a5;");
    headingNames->addWidget(m_chatStatus);
    headingLayout->addLayout(headingNames,1);
    chatHeading->setMinimumHeight(64);
    auto *searchChat=telegramSearchButton(chatHeading);
    searchChat->setFixedSize(searchChat->st().width,searchChat->st().height);
    searchChat->setObjectName("searchConversation");
    searchChat->setAccessibleName("搜索聊天记录");
    searchChat->setToolTip("搜索聊天记录");
    searchChat->setClickedCallback([this]{m_historyPanel->show();m_historySearch->setFocus();});
    headingLayout->addWidget(searchChat);
    auto *chatMenuButton=telegramMenuButton(chatHeading);
    chatMenuButton->setObjectName("conversationMenu");
    chatMenuButton->setToolTip("会话菜单");chatMenuButton->setFixedSize(40,40);

    auto *chatMenu=new QMenu(chatMenuButton);
    chatMenu->addAction("查看资料",this,&MainWindow::viewProfile);
    chatMenu->addAction("发送文件…",this,&MainWindow::sendFile);
    chatMenu->addAction("投屏…",this,&MainWindow::openScreen);
    chatMenu->addAction("共享文件",this,[this]{showSharing();});
    chatMenu->addAction("选取聊天内容发邮件…",this,[this]{exportChatMail();});
    chatMenu->addAction("聊天字体…",this,&MainWindow::chooseFont);
    chatMenuButton->setClickedCallback([chatMenuButton,chatMenu] {
        auto *popup=new Ui::PopupMenu(chatMenuButton,popupCommandCopy(chatMenu));
        popup->deleteOnHide(true);
        popup->popup(chatMenuButton->mapToGlobal(QPoint(0,chatMenuButton->height())));
    });
    headingLayout->addWidget(chatMenuButton);
    co->addWidget(chatHeading);
#else
    auto *ct = new TitleBar(m_chat, "会话");
    m_chatTitle = ct->label;
    co->addWidget(ct);
#endif
    auto *split = new QSplitter;
    co->addWidget(split, 1);
    auto *conversation = new QWidget;
    auto *cl = new QVBoxLayout(conversation);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);
    m_loadEarlier=new QPushButton("加载更早的消息",conversation);
    m_loadEarlier->setObjectName("loadEarlierHistory");m_loadEarlier->hide();
    cl->addWidget(m_loadEarlier);
    connect(m_loadEarlier,&QPushButton::clicked,this,&MainWindow::loadEarlierHistory);
#ifdef LANLINK_FULL_TELEGRAM_UI
    ensureTelegramUiRuntime();
    m_messages = new Ui::ScrollArea(conversation);
#else
    m_messages = new QScrollArea;
#endif
    m_messages->setObjectName("messages");
    m_messages->setWidgetResizable(true);
    m_messages->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
#ifdef LANLINK_FULL_TELEGRAM_UI
    auto *messageCanvas = new TelegramLan::LanHistoryList;
    connect(m_messages->verticalScrollBar(),&QScrollBar::valueChanged,messageCanvas,[this,messageCanvas](int top){
        messageCanvas->setVisibleRange(top,top+m_messages->viewport()->height());
    });
    m_messageLayout = new TelegramLan::LanHistoryLayout(messageCanvas);
#else
    auto *messageCanvas = new QWidget;
    m_messageLayout = new QVBoxLayout(messageCanvas);
#endif
    messageCanvas->setObjectName("messageCanvas");
    m_messageLayout->setContentsMargins(24,20,24,24);
    m_messageLayout->setSpacing(20);
#ifdef LANLINK_FULL_TELEGRAM_UI
    m_messageLayout->setSpacing(0);
    m_messageLayout->setContentsMargins(24,20,24,8);
#endif
    m_messageLayout->addStretch();
#ifdef LANLINK_FULL_TELEGRAM_UI
    static_cast<Ui::ScrollArea*>(m_messages)->setOwnedWidget(object_ptr<QWidget>::fromRaw(messageCanvas));
#else
    m_messages->setWidget(messageCanvas);
#endif
    m_messages->viewport()->installEventFilter(this);
    connect(m_messages->verticalScrollBar(),&QScrollBar::sliderMoved,this,[this]{++m_scrollGeneration;m_scrollRestorePending=false;});
    connect(m_messages->verticalScrollBar(),&QScrollBar::actionTriggered,this,[this](int){++m_scrollGeneration;m_scrollRestorePending=false;});
    connect(m_messages->verticalScrollBar(),&QScrollBar::rangeChanged,this,[this](int,int) {
        if(!m_scrollRestorePending)return;
        const auto generation=m_scrollGeneration;
        QTimer::singleShot(0,this,[this,generation] {
            if(!m_scrollRestorePending || generation!=m_scrollGeneration)return;
            auto *bar=m_messages->verticalScrollBar();
            bar->setValue(m_scrollFollowBottom?bar->maximum():historyAnchorPosition(m_scrollPosition));
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
#ifndef LANLINK_FULL_TELEGRAM_UI
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
    moreMenu->addAction("文件传输记录", this, [this] {showTransfers();});
    moreMenu->addAction("选取聊天内容发邮件…",this,[this]{exportChatMail();});
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
#else
    cpl->setContentsMargins(8,6,8,4);
    cpl->setSpacing(1);
    auto *composeRow = new QHBoxLayout;
    composeRow->setSpacing(6);
    auto *attach=telegramAttachButton(compose);
    attach->setFixedSize(attach->st().width,attach->st().height);
    attach->setObjectName("attachMenu");
    attach->setAccessibleName("附件与聊天工具");
    attach->setToolTip("图片、文件、截图和更多工具");
    auto *attachMenu=new QMenu(attach);
    attachMenu->setObjectName("attachmentActions");
    attachMenu->addAction("图片…",this,&MainWindow::chooseImage);
    attachMenu->addAction("文件…",this,&MainWindow::sendFile);
    attachMenu->addAction("截图…",this,&MainWindow::captureScreenshot);
    attachMenu->addAction("表情…",this,&MainWindow::chooseEmoji);
    attachMenu->addAction("投屏…",this,&MainWindow::openScreen);
    attachMenu->addAction("聊天字体…",this,&MainWindow::chooseFont);
    attachMenu->addAction("聊天记录",this,[this]{m_historyPanel->setVisible(!m_historyPanel->isVisible());});
    attach->setClickedCallback([attach,attachMenu]{
        auto *popup=new Ui::PopupMenu(attach,popupCommandCopy(attachMenu));
        popup->deleteOnHide(true);
        popup->popup(attach->mapToGlobal(QPoint(0,0)));
    });
    composeRow->addWidget(attach,0,Qt::AlignBottom);
#endif
    auto *telegramEditor = new TelegramInputAdapter;
    m_editor = telegramEditor;
    telegramEditor->setSubmitSettings(m_enterSends ? TelegramInputLogic::SubmitSettings::Enter : TelegramInputLogic::SubmitSettings::CtrlEnter);
    connect(telegramEditor,&TelegramInputAdapter::submitted,this,&MainWindow::sendMessage);
    m_editor->setObjectName("editor");
    m_editor->setPlaceholderText("输入消息…");
    m_editor->setFont(m_chatFont);
    m_editor->setMinimumHeight(44);
    m_editor->setStyleSheet("QPlainTextEdit{background:white;border:0;padding:4px;}");
    m_editor->document()->setDocumentMargin(4);
    m_editor->installEventFilter(this);
    m_chat->installEventFilter(this);
#ifdef LANLINK_FULL_TELEGRAM_UI
    composeRow->addWidget(m_editor,1);
#else
    cpl->addWidget(m_editor);
    auto *bottom = new QHBoxLayout;
#endif
    m_sendHint = plainLabel(m_enterSends ? "Enter 发送 · Shift+Enter 换行" : "Ctrl+Enter 发送 · Enter 换行");
    m_sendHint->setStyleSheet("color:#929daa;border:0;");
    m_sendHint->setObjectName("composeHint");
    connect(m_editor, &TelegramInputAdapter::textChanged, this, &MainWindow::updateComposeHint);
#ifdef LANLINK_FULL_TELEGRAM_UI
    m_sendHint->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
#endif
#ifndef LANLINK_FULL_TELEGRAM_UI
    bottom->addWidget(m_sendHint);
    bottom->addStretch();
#endif
    auto *close = new QPushButton("关闭",compose);
#ifdef LANLINK_FULL_TELEGRAM_UI
    connect(close, &QPushButton::clicked, this, [this] {m_showConversation=false;updateColumns();});
    close->hide();
#else
    connect(close, &QPushButton::clicked, m_chat, &QDialog::hide);
#endif
    close->setFixedSize(78,32);
#ifndef LANLINK_FULL_TELEGRAM_UI
    bottom->addWidget(close);
#endif
#ifdef LANLINK_FULL_TELEGRAM_UI
    auto *send = telegramSendButton(compose);
    send->setFixedSize(send->st().width,send->st().height);
    send->setClickedCallback([this]{sendMessage();});
#else
    auto *send = new QPushButton("发送");
    send->setFixedSize(78,32);
    connect(send, &QPushButton::clicked, this, &MainWindow::sendMessage);
#endif
    send->setObjectName("primary");
    send->setAccessibleName("发送");
    send->setToolTip("发送消息");
#ifdef LANLINK_FULL_TELEGRAM_UI
    composeRow->addWidget(send,0,Qt::AlignBottom);
#else
    bottom->addWidget(send);
#endif
    auto *sendOptions = new QToolButton(compose);
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
            static_cast<TelegramInputAdapter*>(m_editor)->setSubmitSettings(mode ? TelegramInputLogic::SubmitSettings::Enter : TelegramInputLogic::SubmitSettings::CtrlEnter);
            updateComposeHint();
        });
    }
    sendOptions->setMenu(sendMenu);
#ifdef LANLINK_FULL_TELEGRAM_UI
    sendOptions->hide();
    attachMenu->addSeparator();
    sendMenu->setTitle("发送快捷键");
    attachMenu->addMenu(sendMenu);
    cpl->addLayout(composeRow);
    m_sendHint->setMaximumHeight(16);
    cpl->addWidget(m_sendHint);
#else
    bottom->addWidget(sendOptions);
#endif
#ifndef LANLINK_FULL_TELEGRAM_UI
    bottom->addWidget(new QSizeGrip(m_chat));
#endif
#ifndef LANLINK_FULL_TELEGRAM_UI
    cpl->addLayout(bottom);
#endif
    verticalSplit->addWidget(compose);
#ifdef LANLINK_FULL_TELEGRAM_UI
    verticalSplit->setSizes({640, 76});
#else
    verticalSplit->setSizes({590, 120});
#endif
    verticalSplit->setHandleWidth(1);
    verticalSplit->setStretchFactor(0, 3);
    verticalSplit->setStretchFactor(1, 0);
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
    m_historySearch = new TelegramSearchField; m_historySearch->setObjectName("historySearch");
    m_historySearch->setPlaceholderText("搜索已加载消息（可先加载更早消息）");
    telegramSearchChanged(m_historySearch,this,[this](const QString &){updateHistorySearch();});
    hl->addWidget(m_historySearch);
    m_history = new QTextBrowser;
    hl->addWidget(m_history);
    m_historyPanel->setMinimumWidth(220);
    m_historyPanel->setMaximumWidth(280);
    split->addWidget(m_historyPanel);
    m_historyPanel->hide();
    split->setSizes({720, 280});
#ifdef LANLINK_FULL_TELEGRAM_UI
    m_rosterPanel = takeCentralWidget();
    m_rosterPanel->setMinimumWidth(0);
    auto *shell=new Ui::RpWidget(this);
    auto *shellLayout=new QVBoxLayout(shell);
    shellLayout->setContentsMargins(0,0,0,0);shellLayout->setSpacing(0);
    shellLayout->addWidget(telegramWindowTitle(shell,QStringLiteral("LanLink 局域网通信")));
    m_columns=new QSplitter(Qt::Horizontal);
    m_columns->setObjectName("telegramColumns");
    m_columns->setChildrenCollapsible(false);m_columns->setHandleWidth(1);
    m_columns->addWidget(m_rosterPanel);m_columns->addWidget(m_chat);
    shellLayout->addWidget(m_columns,1);setCentralWidget(shell);
    connect(m_columns,&QSplitter::splitterMoved,this,[this] {
        if(m_rosterPanel->isVisible() && m_chat->isVisible()) {
            const auto sizes=m_columns->sizes();
            m_dialogsRatio=double(sizes[0])/qMax(1,sizes[0]+sizes[1]);
        }
    });
    installEventFilter(this);
    updateColumns();
#endif

}
void MainWindow::refreshContacts() {
    QString selected = m_current;
    QSignalBlocker block(m_contacts);
    m_contacts->clear();
    auto addRow = [this](QString id, QString name, QString subtitle, bool group) {
        auto *i=new RosterItem(name,m_contacts);i->setData(Qt::UserRole,id);i->setData(GroupRole,group);
        i->setData(DetailsRole,subtitle);
        const auto rows=m_node->history(id,1);
        if(!rows.isEmpty()) {
            const auto last=rows.last().toObject();
            subtitle=last.contains("image_path")&&!last["image_path"].toString().isEmpty()?"[图片]":last["text"].toString();
            subtitle=subtitle.simplified();
            // Keep a long message from becoming an enormous tooltip/accessibility label.
            if (subtitle.size()>200) {
                subtitle=subtitle.left(200);
                if (!subtitle.isEmpty() && subtitle.back().isHighSurrogate()) subtitle.chop(1);
                subtitle += QChar(0x2026);
            }
            if(last["direction"].toString()=="out") subtitle="我："+subtitle;
            const double ts=last["timestamp"].toDouble();i->setData(ActivityRole,ts);
            const auto date=QDateTime::fromMSecsSinceEpoch(qint64(ts*1000));
            i->setData(TimeRole,date.date()==QDate::currentDate()?date.toString("HH:mm"):date.toString("MM-dd"));
        }
        if(m_demo && rows.isEmpty()) {subtitle=group?"测试工作站：新的界面方案已更新":"收到，稍后一起核对。";i->setData(ActivityRole, QDateTime::currentSecsSinceEpoch());i->setData(TimeRole,"10:24");}
        i->setData(PreviewRole,subtitle);i->setData(UnreadRole,m_notifications?m_notifications->unread(id):0);
        i->setToolTip(name+"\n"+subtitle);i->setData(Qt::AccessibleTextRole,name+"，"+subtitle);
    };
    for(auto p:m_peers) addRow(p["id"].toString(),p["name"].toString(),"局域网设备 · "+p["host"].toString(),false);
    for(auto it=m_groups.begin();it!=m_groups.end();++it)
        addRow(it.key(),it.value()["name"].toString(),QString("%1 位成员 · 讨论组").arg(it.value()["members"].toArray().size()),true);
    for (int n = 0; n < m_contacts->count(); ++n)
        if (m_contacts->item(n)->data(Qt::UserRole).toString() == selected)
            m_contacts->setCurrentItem(m_contacts->item(n));
    filterContacts();
}
void MainWindow::filterContacts() {
    QSignalBlocker blocker(m_contacts);
    for(int n=0;n<m_contacts->count();++n) m_contacts->item(n)->setData(SortRole,m_tabs->currentIndex()==0);
    // Reuse Telegram's incrementally adjusted dialog ordering, with LAN IDs.
    TelegramLan::DialogsModel order(m_tabs->currentIndex()==0 ? TelegramLan::DialogsModel::SortMode::Date : TelegramLan::DialogsModel::SortMode::Name);
    QHash<QString,QListWidgetItem*> items;
    auto *selected=m_contacts->currentItem();
    while(m_contacts->count()) {
        auto *item=m_contacts->takeItem(0);
        const auto id=item->data(Qt::UserRole).toString();items[id]=item;
        TelegramLan::DialogRow row;row.id=id;row.name=item->text();
        row.sortKey=quint64(qMax(0.0,item->data(ActivityRole).toDouble())*1000);
        order.upsert(std::move(row));
    }
    for(auto *row:order.rows())m_contacts->addItem(items.value(row->id));
    if(selected)m_contacts->setCurrentItem(selected);
    m_contacts->setProperty("rosterMode",m_tabs->currentIndex());
    int count = 0;
    for (int n = 0; n < m_contacts->count(); ++n) {
        auto *i = m_contacts->item(n);
        bool group = i->data(Qt::UserRole).toString().startsWith("group:");
        bool show = ((m_tabs->currentIndex() == 0 && i->data(ActivityRole).toDouble()>0) || (m_tabs->currentIndex() == 1 && !group) ||
                     (m_tabs->currentIndex() == 2 && group)) &&
                    i->text().contains(m_search->text(), Qt::CaseInsensitive);
        i->setHidden(!show);
        count += show;
    }
#ifdef LANLINK_FULL_TELEGRAM_UI
    if(m_dialogs)m_dialogs->refresh();
#endif
    m_count->setText(
        QString("%1  ·  %2").arg(m_tabs->currentIndex() == 2 ? "我的讨论组" : m_tabs->currentIndex()==1 ? "联系人" : "全部会话").arg(count));
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
bool MainWindow::conversationVisible() const {
#ifdef LANLINK_FULL_TELEGRAM_UI
    if(QApplication::activeModalWidget())return false;
    for(auto *child:centralWidget()->findChildren<QWidget *>(QString(),Qt::FindDirectChildrenOnly))
        if(child->property("lanModalOverlay").toBool() && child->isVisible())return false;
    return m_chat->isVisible() && isActiveWindow() && !isMinimized();
#else
    return m_chat->isVisible() && m_chat->isActiveWindow() && !m_chat->isMinimized();
#endif
}
#ifdef LANLINK_FULL_TELEGRAM_UI
void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    updateColumns();
}
void MainWindow::updateColumns() {
    if(!m_columns) return;
    const auto layout=TelegramLan::ComputeColumnLayout(centralWidget()->width()-1,m_dialogsRatio);
    m_rosterPanel->setVisible(!layout.oneColumn || !m_showConversation);
    m_chat->setVisible(!layout.oneColumn || m_showConversation);
    m_columns->setSizes({layout.dialogsWidth,layout.chatWidth});
    if(auto *back=findChild<QWidget *>("backToConversations")) back->setVisible(layout.oneColumn);
}
#endif
void MainWindow::selectConversation(QListWidgetItem *item) {
    if (!item)
        return;
    if (!m_current.isEmpty())
        m_drafts[m_current] = m_editor->toPlainText();
    m_current = item->data(Qt::UserRole).toString();
    m_editor->setPlainText(m_drafts.value(m_current));
    m_chatTitle->setText(currentName());
    if(m_chatStatus) m_chatStatus->setText(m_current.startsWith("group:")
        ? QString("%1 位成员 · 可信局域网").arg(currentMembers().size())
        : QStringLiteral("局域网设备 · 文件接收需确认"));
    if(m_chatStatus) m_chatStatus->setToolTip(m_chatStatus->text());
    m_chatTitle->setToolTip(currentName()+(m_current.startsWith("group:")?
        QStringLiteral("\n群成员由局域网设备声明，未进行身份认证；仅限可信局域网使用"):QString()));
    m_chat->setWindowTitle(currentName());
    renderHistory(false);
#ifdef LANLINK_FULL_TELEGRAM_UI
    m_showConversation=true;updateColumns();
#endif
    if (m_notifications && conversationVisible()) m_notifications->markRead(m_current);
}
void MainWindow::openConversation() {
    if (m_current.isEmpty()) {
        showError("请先选择联系人或讨论组");
        return;
    }
#ifdef LANLINK_FULL_TELEGRAM_UI
    m_showConversation=true;updateColumns();
    if(isMinimized()) showNormal(); else show();
    raise();activateWindow();
#else
    m_chat->show();
    m_chat->raise();
    m_chat->activateWindow();
#endif
    m_editor->setFocus();
#ifdef LANLINK_FULL_TELEGRAM_UI
    if(m_notifications && conversationVisible()) m_notifications->markRead(m_current);
#else
    if(m_notifications) m_notifications->markRead(m_current);
#endif
}
void MainWindow::appendMessage(QJsonObject r) {
    auto *bar=m_messages->verticalScrollBar();
    const bool wasAtBottom=bar->maximum()-bar->value()<24;
    const int previousPosition=bar->value();
    const bool mine = r["direction"].toString() == "out";
    const QString who = mine ? "我" : m_peers.value(r["sender_id"].toString(), m_peers.value(m_current))["name"].toString("对方");
#ifdef LANLINK_FULL_TELEGRAM_UI
    TelegramLan::HistoryMessage snapshot;
    snapshot.id=r["id"].toString();snapshot.outgoing=mine;snapshot.threadId=m_current;
    snapshot.senderId=mine?m_node->id():r["sender_id"].toString();
    if(snapshot.senderId.isEmpty() && !m_current.startsWith("group:"))snapshot.senderId=m_current;
    if(r["timestamp"].isDouble())snapshot.timestamp=QDateTime::fromSecsSinceEpoch(qint64(r["timestamp"].toDouble()));
    const auto decoration=TelegramLan::DecorateHistoryMessage(snapshot,m_hasPreviousHistoryMessage?&m_previousHistoryMessage:nullptr);
    if(decoration.showDate) {
        auto *date=new TelegramLan::TelegramDateBadge(snapshot.timestamp.date().toString("yyyy年M月d日"));
        m_messageLayout->insertWidget(m_messageLayout->count()-1,date);
    }
    m_previousHistoryMessage=snapshot;m_hasPreviousHistoryMessage=true;
#endif
    auto *row = new QWidget;
    row->setProperty("historyMessageId",r["id"].toString());
    auto *horizontal = new QHBoxLayout(row); horizontal->setContentsMargins(0,0,0,0); horizontal->setSpacing(10);
#ifdef LANLINK_FULL_TELEGRAM_UI
    row->setProperty("telegramAttachedPrevious",decoration.attachPrevious);
    horizontal->setContentsMargins(0,decoration.attachPrevious?0:6,0,2);
#endif
    auto *avatar = plainLabel(mine ? "我" : who.left(1));
    avatar->setObjectName("messageAvatar");
#ifdef LANLINK_FULL_TELEGRAM_UI
    auto policy=avatar->sizePolicy();policy.setRetainSizeWhenHidden(true);avatar->setSizePolicy(policy);
    avatar->setVisible(!decoration.attachPrevious);
#endif
    avatar->setFixedSize(36,36);avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet(mine ? "background:#2688e8;color:white;border-radius:10px;font-size:15px;" : "background:#dfe9f5;color:#597b9c;border-radius:10px;font-size:15px;");
    auto *column = new QWidget; column->setMaximumWidth(570);
    auto *vertical = new QVBoxLayout(column);vertical->setContentsMargins(0,0,0,0);vertical->setSpacing(6);
    QString time=QDateTime::fromSecsSinceEpoch(qint64(r["timestamp"].toDouble())).toString("hh:mm");
    auto *caption = plainLabel(who+"  "+time); caption->setStyleSheet("color:#8997a6;font-size:12px;");caption->setAlignment(mine?Qt::AlignRight:Qt::AlignLeft);
#ifdef LANLINK_FULL_TELEGRAM_UI
    caption->setText(who);caption->setObjectName("messageAuthor");
#endif
#ifndef LANLINK_FULL_TELEGRAM_UI
    vertical->addWidget(caption);
#endif
    auto *bubble = new TelegramBubbleFrame(mine);
    bubble->setObjectName("messageBubble");

    auto *body = new QVBoxLayout(bubble);body->setContentsMargins(14,10,14,10);body->setSpacing(8);
#ifdef LANLINK_FULL_TELEGRAM_UI
    body->setContentsMargins(bubble->layoutPadding());
    body->addWidget(caption);
#endif
    const auto transferId=r["transfer_id"].toString(r["id"].toString().startsWith("file:")?r["id"].toString().mid(5):QString());
    const auto transferSnapshot=m_files && !transferId.isEmpty()?m_files->transferSnapshot(transferId):QJsonObject();
    if(!transferSnapshot.isEmpty()) {
        auto *card=new TransferCard;
        card->setProperty("transferId",transferId);
        card->setProperty("historySaved",true);
        card->setSnapshot(transferSnapshot);
        connect(card,&TransferCard::acceptRequested,this,[this](QString id){acceptTransferFromCard(id);});
        connect(card,&TransferCard::retryRequested,this,[this](QString id){
            if(m_files->transferSnapshot(id)["direction"]=="out")m_files->retryFile(id);
            else acceptTransferFromCard(id);
        });
        connect(card,&TransferCard::pauseRequested,this,[this](QString id){m_files->pauseFile(id);});
        connect(card,&TransferCard::rejectRequested,this,[this](QString id){m_files->rejectFile(id);});
        connect(card,&TransferCard::openRequested,this,[this](QString id,QString path){
            const auto state=m_files->transferSnapshot(id);
            const QFileInfo file(path);
            if(state["direction"]!="in" || state["state"]!="completed" || state["path"].toString()!=path || !file.isFile() ||
               file.isSymLink() || file.size()!=qint64(state["total"].toDouble()))return;
            if(confirm(m_chat,"打开文件","将使用系统默认程序打开已接收文件：\n"+file.fileName())) {
                const QFileInfo recheck(path);
                if(recheck.isFile() && !recheck.isSymLink() && recheck.size()==qint64(state["total"].toDouble()))
                    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            }
        });
        if(mine && m_current.startsWith("group:"))
            body->addWidget(plainLabel("接收设备："+m_peers.value(transferSnapshot["peer_id"].toString())["name"].toString(transferSnapshot["peer_id"].toString())));
        body->addWidget(card);
    } else if(!r["text"].toString().isEmpty()) {
        auto *text=new MessageText(r["text"].toString(),m_chatFont);
#ifdef LANLINK_FULL_TELEGRAM_UI
        text->setTextWidthLimit(qBound(80,m_messages->viewport()->width()-122,320));
        text->setCollapseLongMessages(true);
#endif
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
#ifdef LANLINK_FULL_TELEGRAM_UI
    if(r["timestamp"].isDouble()) {
        auto *stamp=plainLabel(time);stamp->setObjectName("messageTime");
        stamp->setStyleSheet("color:#7e91a3;font-size:11px;");stamp->setAlignment(Qt::AlignRight);
        body->addWidget(stamp);
    }
#endif
    if(mine && transferId.isEmpty() && !r["id"].toString().isEmpty()) {
        const auto id=r["id"].toString();
        auto *status=new QToolButton;status->setObjectName("deliveryStatus");status->setProperty("messageId",id);
        status->setText(DeliveryPresentation::summary(m_node->deliveryStatus(id)));
        status->setToolTip("查看每台接收设备的确认结果；已接收不代表已读");
        status->setToolButtonStyle(Qt::ToolButtonTextOnly);
        connect(status,&QToolButton::clicked,this,[this,id]{showDeliveryDetails(id);});
        body->addWidget(status,0,Qt::AlignRight);
    }
    horizontal->addWidget(column,0,Qt::AlignTop);
    if(mine) horizontal->addWidget(avatar,0,Qt::AlignTop); else horizontal->addStretch(1);
    m_messageLayout->insertWidget(m_messageLayout->count()-1,row);
#ifdef LANLINK_FULL_TELEGRAM_UI
    m_presentedHistory.push_back(snapshot);m_presentedHistoryRows.push_back(row);
    if(!m_rebuildingMessages)applyHistoryPresentation();
#endif
    if(!m_rebuildingMessages) restoreMessageScroll(previousPosition,wasAtBottom);
}
void MainWindow::applyHistoryPresentation() {
#ifdef LANLINK_FULL_TELEGRAM_UI
    const auto decorations=TelegramLan::DecorateHistory(m_presentedHistory);
    const bool group=m_current.startsWith("group:");
    for(int i=0;i<m_presentedHistoryRows.size();++i) {
        auto *row=m_presentedHistoryRows[i].data();if(!row)continue;
        const auto presentation=TelegramLan::PresentHistoryMessage(m_presentedHistory[i],decorations[i],group);
        row->setProperty("telegramAttachedPrevious",decorations[i].attachPrevious);
        row->setProperty("telegramAttachedNext",decorations[i].attachNext);
        row->layout()->setContentsMargins(0,decorations[i].attachPrevious?0:6,0,2);
        if(auto *avatar=row->findChild<QLabel *>("messageAvatar")) {
            auto policy=avatar->sizePolicy();policy.setRetainSizeWhenHidden(presentation.hasFromPhoto);avatar->setSizePolicy(policy);
            avatar->setVisible(presentation.displayFromPhoto);
            row->layout()->setAlignment(avatar,Qt::AlignBottom);
        }
        if(auto *author=row->findChild<QLabel *>("messageAuthor"))author->setVisible(presentation.displayFromName);
        if(auto *bubble=dynamic_cast<TelegramBubbleFrame *>(row->findChild<QWidget *>("messageBubble")))bubble->setRounding(presentation.rounding);
    }
#endif
}
void MainWindow::refreshDeliveryStatus(const QString &id) {
    const auto text=DeliveryPresentation::summary(m_node->deliveryStatus(id));
    for(auto *button:m_messages->findChildren<QToolButton *>("deliveryStatus"))
        if(button->property("messageId").toString()==id)button->setText(text);
}
void MainWindow::showDeliveryDetails(const QString &id) {
    QDialog dialog(m_chat);dialog.setWindowTitle("消息接收结果");dialog.resize(660,360);
    auto *layout=new QVBoxLayout(&dialog);
    auto *notice=plainLabel("结果基于对端保存后的协议确认，不表示对方已阅读。收件人按发送时的设备集合统计。");
    notice->setWordWrap(true);layout->addWidget(notice);
    auto *summary=plainLabel("");layout->addWidget(summary);
    auto *table=new QTableWidget(0,3);table->setHorizontalHeaderLabels({"接收设备","结果","原因"});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(table);
    auto *retry=new QPushButton("重试未确认／拒收设备");layout->addWidget(retry);
    const auto update=[this,id,summary,table,retry] {
        const auto state=m_node->deliveryStatus(id);summary->setText(DeliveryPresentation::summary(state));
        const auto targets=state["targets"].toArray();table->setRowCount(targets.size());
        for(int n=0;n<targets.size();++n) {
            const auto target=targets[n].toObject();
            auto name=target["name"].toString();if(name.isEmpty())name=target["recipient"].toString();
            table->setItem(n,0,new QTableWidgetItem(name));
            table->setItem(n,1,new QTableWidgetItem(DeliveryPresentation::targetState(target["state"].toString())));
            table->setItem(n,2,new QTableWidgetItem(target["error"].toString()));
        }
        bool pending=false;for(const auto &value:m_node->pendingMessages())if(value.toObject()["id"].toString()==id)pending=true;
        retry->setEnabled(pending);
    };
    connect(retry,&QPushButton::clicked,&dialog,[this,id,update]{m_node->retryMessage(id);update();});
    connect(m_node,&Node::event,&dialog,[id,update](QString kind,QJsonObject data){
        if((kind=="delivery_changed" || kind=="delivery_status_error") && data["id"].toString()==id)update();
    });
    auto *close=new QDialogButtonBox(QDialogButtonBox::Close);close->button(QDialogButtonBox::Close)->setText("关闭");layout->addWidget(close);
    connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);update();dialog.exec();
}
void MainWindow::restoreMessageScroll(int position, bool bottom) {
    m_scrollAnchorId.clear();
    m_scrollRestorePending=true;m_scrollPosition=position;m_scrollFollowBottom=bottom;
    const auto generation=++m_scrollGeneration;
    QTimer::singleShot(0,this,[this,position,bottom,generation] {
        if(generation!=m_scrollGeneration)return;
        m_messageLayout->activate();
        m_messages->widget()->adjustSize();
        QTimer::singleShot(0,this,[this,position,bottom,generation] {
            if(generation!=m_scrollGeneration)return;
            auto *bar=m_messages->verticalScrollBar();
            m_scrollPosition=bottom?bar->maximum():historyAnchorPosition(position);bar->setValue(m_scrollPosition);
        });
    });
}
QPair<QString,int> MainWindow::captureHistoryAnchor() const {
    const int top=m_messages->verticalScrollBar()->value();
    for(int i=0;i<m_messageLayout->count();++i) {
        auto *row=m_messageLayout->itemAt(i)->widget();if(!row)continue;
        const auto id=row->property("historyMessageId").toString();if(id.isEmpty())continue;
        const int y=row->mapTo(m_messages->widget(),QPoint()).y();
        if(y+row->height()>top) {
            const auto *body=row->findChild<QWidget *>("messageBubble");
            return {id,(body?body:row)->mapTo(m_messages->widget(),QPoint()).y()-top};
        }
    }
    return {};
}
int MainWindow::historyAnchorPosition(int fallback) const {
    if(m_scrollAnchorId.isEmpty())return fallback;
    for(int i=0;i<m_messageLayout->count();++i) {
        auto *row=m_messageLayout->itemAt(i)->widget();
        if(row && row->property("historyMessageId").toString()==m_scrollAnchorId) {
            const auto *body=row->findChild<QWidget *>("messageBubble");
            return (body?body:row)->mapTo(m_messages->widget(),QPoint()).y()-m_scrollAnchorOffset;
        }
    }
    return fallback;
}
void MainWindow::restoreHistoryAnchor(const QPair<QString,int> &anchor,int fallback) {
    restoreMessageScroll(fallback,false);m_scrollAnchorId=anchor.first;m_scrollAnchorOffset=anchor.second;
}
bool MainWindow::refreshHistoryWindow(bool reset,const QString &revealMessageId) {
    const bool initial=reset || m_loadedConversation!=m_current || m_loadedHistory.isEmpty();
    const bool retainBoundary=!initial && m_historyHasMore;
    auto oldest=retainBoundary?m_loadedHistory.first().toObject():QJsonObject();
    const auto originalOldest=oldest;
    if(initial)m_earlierHistoryMessageId.clear();
    if(retainBoundary && !revealMessageId.isEmpty()) {
        const auto arrived=m_node->historyRecord(m_current,revealMessageId);
        if(!arrived.isEmpty() && (arrived["timestamp"].toDouble()<oldest["timestamp"].toDouble() ||
           (arrived["timestamp"]==oldest["timestamp"] && arrived["id"].toString()<oldest["id"].toString())))oldest=arrived;
    }
    const auto atOrBefore=[&](const QJsonObject &r) {
        const double t=r["timestamp"].toDouble(),bound=oldest["timestamp"].toDouble();
        return t<bound || (t==bound && r["id"].toString()<=oldest["id"].toString());
    };
    QJsonArray records;HistoryCursor cursor;bool more=false;int extraOlderPages=-1;
    for(;;) {
        const auto page=m_node->historyPage(m_current,cursor,200);
        if(!page.error.isEmpty()){m_historyLoadError=page.error;return false;}
        auto combined=page.records;for(const auto &r:records)combined.append(r);records=combined;
        more=page.hasMore;
        if(retainBoundary && oldest!=originalOldest && !page.records.isEmpty()) {
            const auto first=page.records.first().toObject();
            const bool reachedOriginal=first["timestamp"].toDouble()<originalOldest["timestamp"].toDouble() ||
                (first["timestamp"]==originalOldest["timestamp"] && first["id"].toString()<=originalOldest["id"].toString());
            if(reachedOriginal)++extraOlderPages;
            if(extraOlderPages>=2 && page.hasMore && !atOrBefore(first)) {
                // At most two additional older pages are auto-fetched for a backdated arrival.
                oldest=originalOldest;m_earlierHistoryMessageId=revealMessageId;more=true;break;
            }
        }
        if(initial || !page.hasMore || page.records.isEmpty() || (retainBoundary && atOrBefore(page.records.first().toObject())))break;
        cursor=page.nextBefore;
    }
    // Keep the stable previously loaded boundary even when new records arrive.
    if(retainBoundary)while(!records.isEmpty() && atOrBefore(records.first().toObject()) &&
        records.first().toObject()["id"]!=oldest["id"]) {records.removeFirst();more=true;}
    m_loadedConversation=m_current;m_loadedHistory=records;m_historyHasMore=more;m_historyLoadError.clear();
    m_historyBefore={};
    if(!records.isEmpty()){const auto r=records.first().toObject();m_historyBefore={m_current,r["id"].toString(),r["timestamp"].toDouble(),true};}
    return true;
}
void MainWindow::loadEarlierHistory() {
    if(!m_historyLoadError.isEmpty()){renderHistory(m_loadedConversation==m_current);return;}
    if(!m_historyHasMore || !m_historyBefore.valid)return;
    const auto anchor=captureHistoryAnchor();const int position=m_messages->verticalScrollBar()->value();
    const auto page=m_node->historyPage(m_current,m_historyBefore,200);
    if(!page.error.isEmpty()){m_historyLoadError=page.error;m_loadEarlier->setText("重试加载历史");m_loadEarlier->setToolTip(page.error);return;}
    auto combined=page.records;for(const auto &r:m_loadedHistory)combined.append(r);m_loadedHistory=combined;
    m_historyHasMore=page.hasMore;if(page.nextBefore.valid)m_historyBefore=page.nextBefore;
    renderLoadedHistory(position,false,anchor);
}
void MainWindow::renderHistory(bool preservePosition,const QString &revealMessageId) {
    auto *bar=m_messages->verticalScrollBar();
    const bool layoutPending=m_scrollRestorePending;
    const int oldPosition=layoutPending?m_scrollPosition:bar->value();
    const bool bottom=!preservePosition || (layoutPending?m_scrollFollowBottom:bar->maximum()-oldPosition<24);
    const auto anchor=preservePosition&&!bottom?captureHistoryAnchor():QPair<QString,int>();
    if(!refreshHistoryWindow(!preservePosition,revealMessageId)) {
        if(m_loadedConversation!=m_current){m_loadedConversation=m_current;m_loadedHistory={};m_historyHasMore=false;renderLoadedHistory(0,false,{});}
        m_loadEarlier->setText("重试加载历史");m_loadEarlier->setToolTip(m_historyLoadError);m_loadEarlier->show();return;
    }
    renderLoadedHistory(oldPosition,bottom,anchor);
}
void MainWindow::renderLoadedHistory(int oldPosition,bool bottom,const QPair<QString,int> &anchor) {
#ifdef LANLINK_FULL_TELEGRAM_UI
    for(int i=0;i<m_messageLayout->count();++i)if(auto *row=m_messageLayout->itemAt(i)->widget()) {
        const auto id=row->property("historyMessageId").toString();if(id.isEmpty())continue;
        if(auto *text=row->findChild<TelegramMessageText *>("messageText")) {
            if(text->isExpanded())m_expandedHistoryMessages.insert(id);else m_expandedHistoryMessages.remove(id);
        }
    }
#endif
    m_rebuildingMessages=true;m_hasPreviousHistoryMessage=false;
    m_presentedHistory.clear();m_presentedHistoryRows.clear();
    while(m_messageLayout->count()>1){auto *item=m_messageLayout->takeAt(0);delete item->widget();delete item;}
    m_history->clear();
    for(const auto &value:m_loadedHistory)appendMessage(value.toObject());
    if(m_demo) {
        const auto now=double(QDateTime::currentSecsSinceEpoch());
        appendMessage({{"direction","in"},{"text","下午好，新的聊天界面可以看一下吗？"},{"timestamp",now-120}});
        appendMessage({{"direction","out"},{"text","可以。现在支持群聊、文件共享和截图发送。"},{"timestamp",now-90}});
        appendMessage({{"direction","in"},{"text","收到 👍\n这些是界面演示消息，不会发送到网络。"},{"timestamp",now-60}});
        appendMessage({{"direction","out"},{"text","聊天工具支持表情、投屏和文件发送。"},{"timestamp",now}});
    }
#ifdef LANLINK_FULL_TELEGRAM_UI
    for(int i=0;i<m_messageLayout->count();++i)if(auto *row=m_messageLayout->itemAt(i)->widget())
        if(m_expandedHistoryMessages.contains(row->property("historyMessageId").toString()))
            if(auto *text=row->findChild<TelegramMessageText *>("messageText"))text->setExpanded(true);
#endif
    if(!m_earlierHistoryMessageId.isEmpty())for(const auto &r:m_loadedHistory)
        if(r.toObject()["id"].toString()==m_earlierHistoryMessageId){m_earlierHistoryMessageId.clear();break;}
    applyHistoryPresentation();
    m_loadEarlier->setText(m_earlierHistoryMessageId.isEmpty()?"加载更早的消息":"较早位置收到新消息，加载更早消息");
    m_loadEarlier->setToolTip(m_earlierHistoryMessageId.isEmpty()?QString():QString("为避免一次载入过多历史，新消息尚未显示，请继续加载更早消息"));
    m_loadEarlier->setVisible(m_historyHasMore);
    updateHistorySearch();m_rebuildingMessages=false;
    if(!bottom&&!anchor.first.isEmpty())restoreHistoryAnchor(anchor,oldPosition);else restoreMessageScroll(oldPosition,bottom);
}
void MainWindow::watchModalOverlay(QWidget *overlay) {
    if(!overlay)return;
    overlay->setProperty("lanModalOverlay",true);overlay->installEventFilter(this);
}
bool MainWindow::eventFilter(QObject *object, QEvent *event) {
    if(object->property("lanModalOverlay").toBool() &&
       (event->type()==QEvent::Hide || event->type()==QEvent::Destroy)) {
        QTimer::singleShot(0,this,[this]{if(m_notifications && conversationVisible() && !m_current.isEmpty())m_notifications->markRead(m_current);});
    }
#ifdef LANLINK_FULL_TELEGRAM_UI
    if(object==m_messages->viewport() && event->type()==QEvent::Resize) {
        const int width=qBound(80,m_messages->viewport()->width()-122,320);
        for(auto *text:m_messages->findChildren<TelegramMessageText *>("messageText")) text->setTextWidthLimit(width);
    }
#endif
    if(object==m_messages->viewport() && (event->type()==QEvent::Wheel || event->type()==QEvent::KeyPress)) {++m_scrollGeneration;m_scrollRestorePending=false;}
    if(object==m_chat || object==this) {
        const auto type=event->type();
        if(type==QEvent::WindowActivate || type==QEvent::WindowStateChange || type==QEvent::Show) {
            QTimer::singleShot(0,this,[this] {
                if(m_notifications && conversationVisible() && !m_current.isEmpty()) m_notifications->markRead(m_current);
            });
        }
    }
    return QMainWindow::eventFilter(object, event);
}
void MainWindow::updateHistorySearch() {
    m_history->clear();
    const auto query = m_historySearch->text();
    for (auto value : m_loadedHistory) {
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
    const QString recipient=m_current,recipientName=currentName();
    if(recipient.isEmpty()){showError("请先选择联系人或讨论组");return;}
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
    if (confirm(m_chat,"发送图片",QString("向 %1 发送图片 %2？").arg(recipientName,QFileInfo(path).fileName()))) sendImageTo(recipient,bytes);
}
void MainWindow::updateComposeHint() {
    const auto bytes = m_editor->toPlainText().toUtf8().size();
    if (bytes >= Node::MaxTextBytes * 3 / 4) {
        m_sendHint->setText(QString("%1 / %2 字节（UTF-8）%3").arg(bytes).arg(Node::MaxTextBytes)
            .arg(bytes > Node::MaxTextBytes ? " · 已超限" : ""));
        m_sendHint->setStyleSheet(bytes > Node::MaxTextBytes ? "color:#c62828;border:0;" : "color:#9a6700;border:0;");
    } else {
        m_sendHint->setText(m_enterSends ? "Enter 发送 · Shift+Enter 换行" : "Ctrl+Enter 发送 · Enter 换行");
        m_sendHint->setStyleSheet("color:#929daa;border:0;");
    }
}
void MainWindow::sendMessage() {
    if (m_current.isEmpty())
        return;
    QString text = m_editor->toPlainText();
    if (text.trimmed().isEmpty())
        return;
    if (text.toUtf8().size() > Node::MaxTextBytes) {
        showError("消息超过 128 KiB（UTF-8）上限，请拆分发送或作为文件发送；草稿已保留");
        return;
    }
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
    #ifdef LANLINK_FULL_TELEGRAM_UI
    for(auto *text:m_messages->findChildren<TelegramMessageText *>("messageText")) text->applyFont(f);
#else
    for(auto *text:m_messages->findChildren<QTextBrowser *>("messageText")) static_cast<MessageText *>(text)->applyFont(f);
#endif
    restoreMessageScroll(oldPosition,bottom);
}
void MainWindow::captureScreenshot() {
    if (m_current.isEmpty()) {
        showError("请先选择要发送截图的会话");
        return;
    }
    const QString recipient=m_current;
    const QString recipientName=currentName();
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
#ifndef LANLINK_FULL_TELEGRAM_UI
    m_chat->hide();
#endif
    hide();
    QTimer::singleShot(250, this, [this, screen = QPointer<QScreen>(screens[screenIndex]), recipient, recipientName] {
        if(!screen) {show();return;}
        QPointer<MainWindow> guard(this);
        CaptureOverlay overlay(screen);
        bool captured = overlay.exec() == QDialog::Accepted;
        if(!guard)return;
        show();
#ifndef LANLINK_FULL_TELEGRAM_UI
        m_chat->show();
#endif
        m_editor->setFocus();
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
        preview.setWindowTitle("确认发送截图至：" + recipientName);
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
        const bool approved=preview.exec()==QDialog::Accepted;
        if(guard && approved)sendImageTo(recipient,bytes);
    });
}
void MainWindow::sendImage(QByteArray jpeg) { sendImageTo(m_current,std::move(jpeg)); }
void MainWindow::sendImageTo(const QString &recipient,QByteArray jpeg) {
    if (m_demo) {
        showError("演示模式不发送到网络");
        return;
    }
    if(recipient.isEmpty())return;
    m_node->sendImage(recipient, jpeg);
    if(recipient==m_current)renderHistory();
}
void MainWindow::editSettings() {
#ifdef LANLINK_FULL_TELEGRAM_UI
    TelegramSettingsActions actions;
    actions.notifications=m_settings.value("notifications",true).toBool();
    actions.chooseFont=[this]{chooseFont();};
    actions.setNotifications=[this](bool on){m_settings.setValue("notifications",on);if(m_notifications)m_notifications->setEnabled(on);};
    actions.versionNotice=QString("LanLink %1 · C++ / Qt 6\n开发预览，未加密、无设备身份认证。仅在可信局域网或明确配置的受管 VPN 测试，不要将端口映射到公网。\n离线待发保存在发送端，双方再次同时在线后送达。").arg(QCoreApplication::applicationVersion());
    watchModalOverlay(telegramShowProfileBox(centralWidget(),m_node->publicProfile(),[this](QJsonObject profile){
        if(!m_node->updateProfile(profile))return false;
        m_nickname->setText(m_node->name());m_settings.setValue("name",m_node->name());return true;
    },std::move(actions)));
#else
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
    sl->addWidget(plainLabel(QString("LanLink %1 · C++ / Qt 6 原生版\n开发预览，非 Python 便携版。").arg(QCoreApplication::applicationVersion())));
    auto *notice = plainLabel("开发预览，未加密、无设备身份认证。\n仅在可信局域网或明确配置的受管 VPN 测试，不要将端口映射到公网。"
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
    auto *visibility = plainLabel("昵称用于设备识别，以上资料可由当前获准网络中的联系人请求查看。不要填写敏感信息。");
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
#endif
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
    auto *host = new QLineEdit(m_node->networkPolicy().vpnOnly ? QString() : QString("192.168.1."));
    host->setPlaceholderText(m_node->networkPolicy().vpnOnly ? "已批准的 VPN IPv4 地址" : "对方的局域网 IPv4 地址");
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
void MainWindow::showNetworkDiagnostics() {
    QDialog dialog(this);
    dialog.setWindowTitle("网络连接诊断");
    dialog.resize(640,540);
    auto *layout = new QVBoxLayout(&dialog);
    auto *notice = plainLabel("VPN 模式需启动时指定接口、本机 IPv4 和精确对端列表。应用不安装 VPN、不修改路由或防火墙。\n"
        "诊断仅验证 LanLink 应答，不认证身份，不证明隧道加密。请勿将 TCP 端口映射到公网。");
    notice->setWordWrap(true); layout->addWidget(notice);
    auto *details = new QPlainTextEdit;
    details->setReadOnly(true);
    auto refresh = [this,details] { details->setPlainText(QString::fromUtf8(QJsonDocument(m_node->networkDiagnostics()).toJson(QJsonDocument::Indented))); };
    refresh(); layout->addWidget(details);
    auto *form = new QFormLayout;
    auto *host = new QLineEdit;
    host->setPlaceholderText("对方的数字 IPv4 地址");
    auto *port = new QSpinBox; port->setRange(1,65535); port->setValue(45870);
    form->addRow("测试设备 IP",host); form->addRow("对方端口",port); layout->addLayout(form);
    auto *result = plainLabel("测试会发送设备握手，对方可能保存本机设备名称与地址。");
    result->setWordWrap(true); layout->addWidget(result);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    auto *test = buttons->addButton("测试连接",QDialogButtonBox::ActionRole);
    auto *reload = buttons->addButton("刷新本机状态",QDialogButtonBox::ActionRole);
    test->setEnabled(!m_demo);
    connect(reload,&QPushButton::clicked,&dialog,refresh);
    connect(test,&QPushButton::clicked,&dialog,[this,host,port,result,test,&dialog] {
        test->setEnabled(false); result->setText("正在等待 LanLink 应答…");
        QPointer<QDialog> alive(&dialog);
        m_node->diagnoseAddress(host->text().trimmed(),quint16(port->value()),[alive,result,test](QJsonObject data,QString error) {
            if (!alive) return;
            test->setEnabled(true);
            result->setText(error.isEmpty() ? QString("LanLink 可达：%1。设备身份未验证。").arg(data["peer"].toObject()["name"].toString())
                : "连接未成功：" + error + "\n请检查两端 VPN、精确地址白名单、固定端口及管理员配置的路由/防火墙。");
        });
    });
    connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    layout->addWidget(buttons); dialog.exec();
}
void MainWindow::createGroup() {
#ifdef LANLINK_FULL_TELEGRAM_UI
    QJsonArray peers;
    for(auto peer:m_peers){peer["unread"]=m_notifications?m_notifications->unread(peer["id"].toString()):0;peers.append(peer);}
    watchModalOverlay(telegramShowCreateGroup(centralWidget(),peers,[this](QStringList ids,QString name){return !m_node->createGroup(ids,name).isEmpty();}));
#else
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
#endif
}
void MainWindow::event(QString kind, QJsonObject data) {
    if (kind == "peer") {
        m_peers[data["id"].toString()] = data;
        refreshContacts();
    } else if (kind == "group") {
        m_groups["group:" + data["id"].toString()] = data;
        refreshContacts();
    } else if(kind=="delivery_changed" || kind=="delivery_status_error") {
        refreshDeliveryStatus(data["id"].toString());
    } else if (kind == "message") {
        const QString conversation = data["peer_id"].toString();
        if(m_notifications && data["direction"].toString()=="in") {
            QString name = conversation.startsWith("group:") ? m_groups.value(conversation)["name"].toString("讨论组") : m_peers.value(conversation)["name"].toString("联系人");
            m_notifications->incoming(conversation, name, conversation==m_current && conversationVisible());
        }
        if (data["peer_id"].toString() == m_current)
            renderHistory(true,data["id"].toString());
        refreshContacts();
    } else if (kind == "error")
        showError(data["message"].toString());
    else if (kind == "profile_received") {
#ifdef LANLINK_FULL_TELEGRAM_UI
        const auto peerId=data["peer_id"].toString();
        auto *book=contactMailBook();
        watchModalOverlay(telegramShowContactProfile(centralWidget(),peerId,data["profile"].toObject(),book?book->email(peerId):QString(),
            [this,peerId](QString email){auto *book=contactMailBook();if(!book)return false;QString error;
                if(!book->setEmail(peerId,email,&error)){showError(error);return false;}return true;}));
#else
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
        const auto peerId=data["peer_id"].toString();auto *book=contactMailBook();
        auto *email=new QLineEdit(book?book->email(peerId):QString());email->setMaxLength(254);l->addRow("本机联系邮箱",email);
        auto *privacy=plainLabel("仅保存在本机，不向其他设备共享。发送邮件前仍需核对收件人。");privacy->setWordWrap(true);l->addRow(privacy);
        auto *save=new QPushButton("保存本机邮箱");l->addRow(save);
        connect(save,&QPushButton::clicked,&d,[this,peerId,email]{auto *book=contactMailBook();if(!book)return;QString error;
            if(!book->setEmail(peerId,email->text(),&error))showError(error);});
        auto *b = new QDialogButtonBox(QDialogButtonBox::Close);
        connect(b, &QDialogButtonBox::rejected, &d, &QDialog::reject);
        l->addRow(b);
        d.exec();
#endif
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
    if (view == "contacts" || view == "groups" || view == "sessions") {
        m_tabs->setCurrentIndex(view=="groups"?2:view=="sessions"?0:1);
        QApplication::processEvents();grab().save(path);
    } else if (view == "chat" || view == "history") {
        openConversation();
        if (view == "history")
            m_historyPanel->show();
        QApplication::processEvents();
#ifdef LANLINK_FULL_TELEGRAM_UI
        grab().save(path);
#else
        m_chat->grab().save(path);
#endif
    } else if (view == "transfers") {
        showTransfers();
#ifdef LANLINK_FULL_TELEGRAM_UI
        QApplication::processEvents();grab().save(path);
#else
        m_transfers->grab().save(path);
#endif
    } else
        grab().save(path);
}
void MainWindow::showTransfers() {
#ifdef LANLINK_FULL_TELEGRAM_UI
    if(!m_files)return;
    m_transferList=TelegramLan::ShowTransferBox(centralWidget(),m_files->transferSnapshots(),[this](QString action,QString id,QString path){
        if(action=="accept")acceptTransferFromCard(id);
        else if(action=="reject")m_files->rejectFile(id);
        else if(action=="pause")m_files->pauseFile(id);
        else if(action=="retry") {
            if(m_files->transferSnapshot(id)["direction"]=="out")m_files->retryFile(id);
            else acceptTransferFromCard(id);
        } else if(action=="open") {
            const auto state=m_files->transferSnapshot(id);const QFileInfo file(path);
            if(state["direction"]!="in" || state["state"]!="completed" || state["path"].toString()!=path || !file.isFile() ||
               file.isSymLink() || file.size()!=qint64(state["total"].toDouble()))return;
            if(confirm(this,"打开文件","将使用系统默认程序打开已接收文件：\n"+file.fileName())) {
                const QFileInfo recheck(path);
                if(recheck.isFile() && !recheck.isSymLink() && recheck.size()==qint64(state["total"].toDouble()))
                    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            }
        }
    });
    watchModalOverlay(centralWidget()->findChild<QWidget *>("telegramTransferOverlay",Qt::FindDirectChildrenOnly));
#else
    m_transfers->show();m_transfers->raise();
#endif
}
void MainWindow::buildFiles() {
#ifndef LANLINK_FULL_TELEGRAM_UI
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
#endif
}
void MainWindow::acceptTransferFromCard(const QString &id) {
    const auto snapshot=m_files->transferSnapshot(id);
    if(snapshot["direction"]!="in")return;
    QString directory=m_saveDirs.value(id,snapshot["save_dir"].toString());
    if(directory.isEmpty())directory=QFileDialog::getExistingDirectory(m_chat,"选择接收目录");
    if(directory.isEmpty())return;
    m_saveDirs[id]=directory;m_files->acceptFile(id,directory);
}
void MainWindow::transferSnapshotChanged(QJsonObject snapshot) {
    const auto id=snapshot["id"].toString();
    const bool saved=m_node->recordTransferEvent(snapshot);
    if(!saved && !m_transferHistoryWarnings.contains(id)) {
        m_transferHistoryWarnings.insert(id);
        statusBar()->showMessage("文件事件未能保存到聊天历史；传输状态仍可在传输窗口查看，请检查磁盘或数据库",15000);
    }
    // Only replace the hint belonging to this offer. Never overwrite a newer
    // offer, history/storage warning or unrelated error with a progress update.
    if(id==m_pendingOfferStatusId && snapshot["state"]!="waiting" &&
       statusBar()->currentMessage()==m_pendingOfferStatusText) {
        const auto message=QStringLiteral("文件：")+snapshot["name"].toString()+" · "+TransferCard::statusText(snapshot);
        if(message!=m_pendingOfferStatusText) {
            m_pendingOfferStatusText=message;statusBar()->showMessage(message,10000);
        }
    }
    const bool first=!m_timelineTransfers.contains(id);
    if(saved)m_timelineTransfers.insert(id);
    bool found=false;
    for(auto *card:m_messages->findChildren<TransferCard *>())
        if(card->property("transferId").toString()==id) {
            card->setSnapshot(snapshot);card->setProperty("historySaved",saved);
            card->setToolTip(saved?QString():QStringLiteral("此文件事件尚未保存到聊天历史"));
            found=true;
        }
    // Transfer progress remains real even when local history cannot be written.
    // Never pretend a missing database row is durable; only reveal after save.
    if(saved && snapshot["conversation_id"].toString(snapshot["peer_id"].toString())==m_current && !found && first)
        renderHistory(true,"file:"+id);
    if(saved && first)refreshContacts();
    transferProgress(id,qint64(snapshot["done"].toDouble()),qint64(snapshot["total"].toDouble()),TransferCard::statusText(snapshot));
#ifndef LANLINK_FULL_TELEGRAM_UI
    if(m_rows.contains(id)) {
        m_transferTable->item(m_rows.value(id),0)->setText(snapshot["name"].toString());
        if(auto *actions=m_transferTable->cellWidget(m_rows.value(id),3)) {
            const auto state=snapshot["state"].toString();
            if(auto *pause=actions->findChild<QPushButton *>("transferPauseAction"))
                pause->setEnabled(state=="running" || state=="verifying");
            if(auto *resume=actions->findChild<QPushButton *>("transferResumeAction")) {
                const bool outgoing=snapshot["direction"]=="out";
                const bool remoteRetry=outgoing && (state=="remote_failed" || state=="remote_paused");
                resume->setText(outgoing?QStringLiteral("重新发送"):QStringLiteral("接收／续传"));
                resume->setEnabled((state=="waiting" && !outgoing) ||
                    ((state=="paused" || state=="failed" || remoteRetry) && snapshot["can_retry"].toBool(!remoteRetry)));
            }
        }
    }
#endif
}
void MainWindow::transferProgress(QString id, qint64 done, qint64 total, QString status) {
    // The latest model may already contain a terminal receipt when a delayed
    // legacy progress signal arrives. Never repaint it as ordinary "sending".
    const auto authoritative=m_files?m_files->transferSnapshot(id):QJsonObject();
    if(!authoritative.isEmpty()) {
        done=qint64(authoritative["done"].toDouble());total=qint64(authoritative["total"].toDouble());
        status=TransferCard::statusText(authoritative);
    }
#ifdef LANLINK_FULL_TELEGRAM_UI
    if(m_transferList && !authoritative.isEmpty())m_transferList->upsert(authoritative);
#else
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
        pause->setObjectName("transferPauseAction");
        connect(pause, &QPushButton::clicked, this, [this, id] { m_files->pauseFile(id); });
        al->addWidget(pause);
        auto *resume = new QPushButton("接收／续传");
        resume->setObjectName("transferResumeAction");
        connect(resume, &QPushButton::clicked, this, [this, id] {
            if(m_files->transferSnapshot(id)["direction"]=="out")m_files->retryFile(id);
            else acceptTransferFromCard(id);
        });
        al->addWidget(resume);
        m_transferTable->setCellWidget(row, 3, actions);
    }
    int row = m_rows[id];
    m_transferTable->item(row, 1)->setText(
        QString("%1%  %2 / %3").arg(total ? done * 100 / total : 0).arg(done).arg(total));
    m_transferTable->item(row, 2)->setText(status);
#endif
}
void MainWindow::offerReceived(QJsonObject offer) {
    const auto id=offer["id"].toString();
    const auto senderId=offer["sender"].toObject()["id"].toString();
    m_offers[id]=offer;
    transferProgress(id,0,qint64(offer["size"].toDouble()),"等待确认");
    const auto sender=m_peers.value(senderId)["name"].toString("联系人");
    // Routing comes from FileTransfer's validated/persisted snapshot, never
    // from an untrusted bare group ID in the incoming offer.
    const auto snapshot=m_files->transferSnapshot(id);
    const auto conversation=snapshot["conversation_id"].toString(senderId);
    const auto contextName=conversation.startsWith("group:")?
        m_node->fileGroupContext(conversation)["name"].toString("讨论组")+" · "+sender:sender;
    // An offer is not permission to save a file. Keep it pending in the
    // conversation card; only the explicit receive action chooses a directory.
    if(m_notifications)m_notifications->incoming(conversation,contextName,
        conversation==m_current && conversationVisible());
    m_pendingOfferStatusId=id;
    m_pendingOfferStatusText=contextName+" 发来文件邀请，请在会话中选择接收或拒绝"+
        (conversation.startsWith("group:")?QStringLiteral("（群成员身份未验证）"):QString());
    statusBar()->showMessage(m_pendingOfferStatusText,10000);
}
void MainWindow::sendFile() {
    auto members = currentMembers();
    const auto conversation=m_current;
    const auto recipientName=currentName();
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
                 QString("向 %1 的 %2 名成员分别发送文件邀请？").arg(recipientName).arg(members.size())))
        return;
    for (auto id : members)
        m_files->offerFile(id, file, conversation);
    showTransfers();
}
void MainWindow::refreshSharingRoots() {
#ifdef LANLINK_FULL_TELEGRAM_UI
    if(!m_sharingPane || !m_files)return;
    QJsonArray roots;const auto published=m_files->publishedShares();
    for(auto it=published.begin();it!=published.end();++it)roots.append(QJsonObject{{"token",it.key()},{"path",it.value().toString()}});
    m_sharingPane->setRoots(roots);
#endif
}
void MainWindow::showSharing() {
#ifdef LANLINK_FULL_TELEGRAM_UI
    if(!m_files)return;
    TelegramLan::SharingActions actions;
    actions.inputsChanged=[this](QString token,QString directory){m_shareCode->setText(token);m_sharePath->setText(directory);};
    actions.publish=[this]{
        const auto path=QFileDialog::getExistingDirectory(this,"选择只读共享目录");
        if(path.isEmpty() || !confirm(this,"发布只读共享","持有访问码的局域网联系人将能读取此目录中的文件。确认发布？"))return;
        if(m_files->publishShare(path).isEmpty())return;
        refreshSharingRoots();
    };
    actions.copy=[this](QString token){if(m_files->publishedShares().contains(token))QApplication::clipboard()->setText(token);};
    actions.revoke=[this](QString token){m_files->revokeShare(token);refreshSharingRoots();};
    actions.browse=[this](QString token,QString directory){
        if(m_current.isEmpty() || m_current.startsWith("group:")){showError("请在联系人列表选择一台设备");return;}
        browseShareRequest(m_current,token.trimmed(),directory.trimmed());
    };
    actions.browseFrozen=[this](QString peer,QString token,QString directory){browseShareRequest(peer,token,directory);};
    actions.downloadFrozen=[this](QString peer,QString token,QString relative){downloadShareFile(peer,token,relative);};
    m_sharingPane=TelegramLan::ShowSharingBox(centralWidget(),{},std::move(actions));refreshSharingRoots();
    m_sharingPane->setInputs(m_shareCode->text(),m_sharePath->text());
    if(!m_sharePeer.isEmpty()) {
        m_sharingPane->setLoading(m_sharePeer,m_shareToken,m_shareDirectory);
        if(m_shareLocation->text().startsWith("已读取")) {
            QJsonArray entries;for(int i=0;i<m_shareList->count();++i)entries.append(m_shareList->item(i)->data(Qt::UserRole).toJsonObject());
            m_sharingPane->setRemote(m_sharePeer,m_shareToken,m_shareDirectory,entries,m_shareLocation->text());
        }
    }
    watchModalOverlay(centralWidget()->findChild<QWidget *>("telegramSharingOverlay",Qt::FindDirectChildrenOnly));
#else
    m_shares->show();m_shares->raise();
#endif
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
    m_shareLocation=plainLabel("尚未读取共享目录");
    m_shareLocation->setObjectName("shareLocation");
    m_shareLocation->setWordWrap(true); rl->addWidget(m_shareLocation);
    m_shareList = new QListWidget;
    connect(m_shareList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *i) {
        auto d = i->data(Qt::UserRole).toJsonObject();
        QString name = d["name"].toString();
        if (d["directory"].toBool() || d["is_dir"].toBool() || d["type"].toString() == "directory") {
            QString p = m_shareDirectory;
            p=p.isEmpty()?name:p+"/"+name;
            m_sharePath->setText(p);
            browseShareRequest(m_sharePeer,m_shareToken,p);
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
    browseShareRequest(m_current,m_shareCode->text().trimmed(),m_sharePath->text().trimmed());
}
void MainWindow::browseShareRequest(const QString &peer,const QString &token,const QString &directory) {
    m_sharePeer=peer; m_shareToken=token; m_shareDirectory=directory;
    m_shareList->clear();
    m_shareLocation->setText("正在读取共享目录…");
#ifdef LANLINK_FULL_TELEGRAM_UI
    if(m_sharingPane)m_sharingPane->setLoading(peer,token,directory);
#endif
    m_files->listShare(peer,token,directory);
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
    const QString peer=m_sharePeer,token=m_shareToken;
    QString rel=m_shareDirectory;
    if(!rel.isEmpty())rel+="/";
    rel+=d["name"].toString();
    downloadShareFile(peer,token,rel);
}
void MainWindow::downloadShareFile(QString peer,QString token,QString rel) {
    // Caller captures the accepted listing context before the chooser opens.
    QString dir = QFileDialog::getExistingDirectory(this, "下载文件保存目录");
    if (dir.isEmpty())
        return;
    m_files->downloadShare(peer,token,rel,dir);
    showTransfers();
}
void MainWindow::setupServices() {
    m_files = new FileTransfer(m_node, this);
    m_screen = new ScreenService(m_node, this);
    m_preview = new LinkPreview(this);
    m_node->serviceHandler = [this](QTcpSocket *s, QJsonObject r, QString h) {
        return m_files->handle(s, r, h) || m_screen->handle(s, r, h);
    };
    connect(m_files, &FileTransfer::transferChanged, this, &MainWindow::transferSnapshotChanged);
    connect(m_files, &FileTransfer::offerReceived, this, &MainWindow::offerReceived);
    connect(m_files, &FileTransfer::progress, this, &MainWindow::transferProgress);
    connect(m_files, &FileTransfer::error, this, &MainWindow::showError);
    connect(m_files, &FileTransfer::completed, this, [this](QString id, QString path) {
        if(id==m_pendingOfferStatusId && statusBar()->currentMessage()==m_pendingOfferStatusText) {
            m_pendingOfferStatusText="已接收并校验："+QFileInfo(path).fileName();
            statusBar()->showMessage(m_pendingOfferStatusText,10000);
        }
    });
    connect(m_files, &FileTransfer::shareListedFor, this, [this](QString peer,QString token,QString directory,QJsonArray entries) {
        if(peer!=m_sharePeer || token!=m_shareToken || directory!=m_shareDirectory)return;
        m_shareLocation->setText(QString("已读取 %1 / %2（修改访问码或路径后请重新浏览）").arg(m_peers.value(peer)["name"].toString(peer),directory.isEmpty()?"根目录":directory));
#ifdef LANLINK_FULL_TELEGRAM_UI
        if(m_sharingPane)m_sharingPane->setRemote(peer,token,directory,entries,m_shareLocation->text());
#endif
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
    for(auto value:m_files->transferSnapshots())transferSnapshotChanged(value.toObject());
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

void MainWindow::showMail() {
    // Mail polling is opt-in per account and survives hiding the mail pane.
    if(!m_mailWindow) m_mailWindow=new MailWindow({},this);
    m_mailWindow->show();m_mailWindow->raise();m_mailWindow->activateWindow();
}

ContactMailBook *MainWindow::contactMailBook() {
    if(!m_contactMailBook){
        auto book=std::make_unique<ContactMailBook>(m_node->root()+"/contact-mail.json");QString error;
        if(!book->load(&error)){showError(error);return nullptr;}
        m_contactMailBook=std::move(book);
    }
    return m_contactMailBook.get();
}
void MainWindow::exportChatMail() {
    const auto conversation=m_current;
    if(conversation.isEmpty() || m_loadedHistory.isEmpty()){showError("当前没有可选的已加载聊天记录");return;}
    // Freeze the source conversation and its loaded records before any modal interaction.
    const auto records=m_loadedHistory;const auto title=currentName();
    auto *book=contactMailBook();if(!book)return;
    QDialog dialog(this);dialog.setWindowTitle("选取聊天内容，生成邮件草稿");dialog.resize(650,560);
    auto *layout=new QVBoxLayout(&dialog);
    auto *notice=plainLabel(QString("仅列出当前会话已加载的 %1 条记录。需要更早内容时请先加载历史。所有项目默认不选；不会附加文件或其他会话内容。").arg(records.size()));notice->setWordWrap(true);layout->addWidget(notice);
    auto *list=new QListWidget;list->setObjectName("chatMailSelection");layout->addWidget(list,1);
    for(const auto &value:records){const auto record=value.toObject();const auto id=record["id"].toString();
        auto preview=record["text"].toString();if(preview.isEmpty())preview="[非文本消息，仅导出说明]";
        auto *item=new QListWidgetItem(QDateTime::fromSecsSinceEpoch(qint64(record["timestamp"].toDouble())).toString("MM-dd HH:mm")+"  "+preview.left(120),list);
        item->setData(Qt::UserRole,id);item->setCheckState(Qt::Unchecked);item->setToolTip("<pre>"+preview.toHtmlEscaped()+"</pre>");
    }
    auto *all=new QCheckBox("选择当前列表全部记录（不包含尚未加载的历史）");layout->addWidget(all);
    connect(all,&QCheckBox::toggled,&dialog,[list](bool checked){for(int i=0;i<list->count();++i)list->item(i)->setCheckState(checked?Qt::Checked:Qt::Unchecked);});
    auto *contact=new QComboBox;contact->addItem("手动填写收件邮箱",QString());
    for(auto it=m_peers.begin();it!=m_peers.end();++it){const auto email=book->email(it.key());if(!email.isEmpty())contact->addItem(it.value()["name"].toString()+" <"+email+">",email);}
    layout->addWidget(contact);
    auto *recipient=new QLineEdit(book->email(conversation));recipient->setObjectName("chatMailRecipient");recipient->setPlaceholderText("核对或编辑收件人邮箱");recipient->setMaxLength(254);layout->addWidget(recipient);
    connect(contact,&QComboBox::currentIndexChanged,&dialog,[contact,recipient](int){recipient->setText(contact->currentData().toString());});
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons->button(QDialogButtonBox::Ok)->setText("生成草稿并预览（不发送）");layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    QStringList selected;for(int i=0;i<list->count();++i)if(list->item(i)->checkState()==Qt::Checked)selected<<list->item(i)->data(Qt::UserRole).toString();
    const auto result=ChatMailExport::build(conversation,records,selected,title);
    if(!result.ok){showError(result.error);return;}
    showMail();
    if(!m_mailWindow->setExternalDraft(recipient->text().trimmed(),result.subject,result.body)){
        if(m_mailWindow->operationInProgress()){showError("邮箱正在执行操作，请完成或取消后再生成草稿");return;}
        if(!confirm(this,"替换邮件草稿","邮箱已有草稿。替换将丢弃原草稿正文和附件，但不会发送邮件。确认替换？"))return;
        m_mailWindow->setExternalDraft(recipient->text().trimmed(),result.subject,result.body,true);
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Let the mail editor confirm losing an unsaved draft or an active submission.
    if(m_mailWindow && !m_mailWindow->shutdown()){event->ignore();return;}
    QMainWindow::closeEvent(event);
}
