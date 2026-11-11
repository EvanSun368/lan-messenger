#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_message_text.h"
using TestMessageText=TelegramMessageText;
#else
#include <QTextBrowser>
using TestMessageText=QTextBrowser;
#endif
#include "telegram_input_adapter.h"
#include "mailwindow.h"
#include "contactmailbook.h"
#include "telegram_search_field.h"
#ifdef LANLINK_FULL_TELEGRAM_UI
#include "telegram_original_title.h"
#include "telegram_profile_box.h"
#include "telegram_contact_boxes.h"
#include "telegram_transfer_list.h"
#include "telegram_sharing_box.h"
#include "telegram_dialogs_inner.h"
#include "telegram_lan_history_list.h"
#include "ui/widgets/scroll_area.h"
#endif
#include "telegram_row_painter.h"
#include "telegram_column_layout.h"
#include "mainwindow.h"
#include "node.h"
#include "filetransfer.h"
#include "chatnotifications.h"
#include <QtTest>
#include <QtWidgets>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
class UiTest : public QObject {
    Q_OBJECT
    QTemporaryDir settingsDir;
  private slots:
    void initTestCase() {
        QVERIFY(settingsDir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDir.path());
    }
    void historyPagingRetainsVisibleAnchor() {
        QTemporaryDir dir;MainWindow w(dir.path(),0,false);w.show();
        const auto connection=QUuid::createUuid().toString();
        {
            auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(w.node()->root()+"/history.sqlite");QVERIFY(db.open());QVERIFY(db.transaction());
            QSqlQuery q(db);q.prepare("INSERT INTO messages(id,peer,direction,text,time) VALUES(?,?,?,?,?)");
            for(int i=0;i<500;++i){q.bindValue(0,QString("paging%1").arg(i,5,10,QChar('0')));q.bindValue(1,"paging-peer");q.bindValue(2,"in");q.bindValue(3,QString("historical entry %1").arg(i));q.bindValue(4,double(i/30));QVERIFY(q.exec());}
            QVERIFY(db.commit());db.close();
        }QSqlDatabase::removeDatabase(connection);
        w.m_current="paging-peer";w.renderHistory(false);w.openConversation();QTest::qWait(80);
        QCOMPARE(w.m_loadedHistory.size(),200);QVERIFY(w.m_historyHasMore);
        auto *button=w.findChild<QPushButton *>("loadEarlierHistory");QVERIFY(button);QVERIFY(!button->isHidden());
        auto *bar=w.m_messages->verticalScrollBar();QTRY_VERIFY(bar->maximum()>1000);
        bar->triggerAction(QAbstractSlider::SliderToMinimum);bar->setValue(400);
        const auto anchor=w.captureHistoryAnchor();QVERIFY(!anchor.first.isEmpty());
        button->click();QTRY_COMPARE(w.m_loadedHistory.size(),400);
        QTRY_COMPARE(w.captureHistoryAnchor().first,anchor.first);
        QTRY_VERIFY(qAbs(w.captureHistoryAnchor().second-anchor.second)<=2);
        button->click();QTRY_COMPARE(w.m_loadedHistory.size(),500);QVERIFY(!w.m_historyHasMore);QVERIFY(button->isHidden());
        QTRY_COMPARE(w.captureHistoryAnchor().first,anchor.first);
        const auto name=QUuid::createUuid().toString();{
            auto db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(w.node()->root()+"/history.sqlite");QVERIFY(db.open());QSqlQuery q(db);
            QVERIFY(q.exec("INSERT INTO messages VALUES('paging-new','paging-peer','in','new arrival',9999)"));db.close();
        }QSqlDatabase::removeDatabase(name);
        w.renderHistory();QCOMPARE(w.m_loadedHistory.size(),501);
        QTRY_COMPARE(w.captureHistoryAnchor().first,anchor.first);
        w.m_historySearch->setText("historical entry 0");QVERIFY(w.m_history->toPlainText().contains("historical entry 0"));
        w.m_current="other";w.renderHistory(false);QVERIFY(w.m_loadedHistory.isEmpty());QVERIFY(button->isHidden());
    }
    void backdatedArrivalDoesNotAutoLoadWholeArchive() {
        QTemporaryDir dir;MainWindow w(dir.path(),0,false);
        const auto connection=QUuid::createUuid().toString();{
            auto db=QSqlDatabase::addDatabase("QSQLITE",connection);db.setDatabaseName(w.node()->root()+"/history.sqlite");QVERIFY(db.open());QVERIFY(db.transaction());
            QSqlQuery q(db);q.prepare("INSERT INTO messages VALUES(?,?,?,?,?)");
            for(int i=0;i<850;++i){q.bindValue(0,QString("archive%1").arg(i,5,10,QChar('0')));q.bindValue(1,"archive-peer");q.bindValue(2,"in");q.bindValue(3,"archived message");q.bindValue(4,double(i));QVERIFY(q.exec());}
            QVERIFY(db.commit());db.close();
        }QSqlDatabase::removeDatabase(connection);
        w.m_current="archive-peer";w.renderHistory(false);QCOMPARE(w.m_loadedHistory.size(),200);
        const auto name=QUuid::createUuid().toString();{
            auto db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(w.node()->root()+"/history.sqlite");QVERIFY(db.open());QSqlQuery q(db);
            QVERIFY(q.exec("INSERT INTO messages VALUES('backdated-live','archive-peer','in','new but old local clock',-1)"));db.close();
        }QSqlDatabase::removeDatabase(name);
        w.renderHistory(true,"backdated-live");QCOMPARE(w.m_loadedHistory.size(),200);
        QCOMPARE(w.m_earlierHistoryMessageId,QString("backdated-live"));QVERIFY(w.m_loadEarlier->text().contains("新消息"));
        int pages=0;while(w.m_historyHasMore){w.loadEarlierHistory();QVERIFY(++pages<8);}
        QCOMPARE(w.m_loadedHistory.size(),851);QVERIFY(w.m_earlierHistoryMessageId.isEmpty());
        QCOMPARE(w.m_loadedHistory.first().toObject()["id"].toString(),QString("backdated-live"));
    }
    void telegramColumnRules() {
        const auto small=TelegramLan::ComputeColumnLayout(639);
        QVERIFY(small.oneColumn);QCOMPARE(small.dialogsWidth,639);
        const auto edge=TelegramLan::ComputeColumnLayout(640);
        QVERIFY(!edge.oneColumn);QCOMPARE(edge.dialogsWidth,260);QCOMPARE(edge.chatWidth,380);
        const auto wide=TelegramLan::ComputeColumnLayout(1000,0.99);
        QCOMPARE(wide.dialogsWidth,620);QCOMPARE(wide.chatWidth,380);
    }
#ifdef LANLINK_FULL_TELEGRAM_UI
    void originalShellControlsAndRealEntryPoints() {
        QTemporaryDir dir; MainWindow w(dir.path(),0,true); w.show();
        QTest::qWait(30);
        auto *navigation=w.findChild<QWidget *>("navigationMenu"); QVERIFY(navigation);
        QVERIFY(dynamic_cast<Ui::IconButton *>(navigation));
        QVERIFY(dynamic_cast<Ui::ScrollArea *>(w.m_messages));
        QVERIFY(dynamic_cast<TelegramLan::LanHistoryList *>(w.m_messages->widget()));
        QVERIFY(dynamic_cast<TelegramLan::LanHistoryLayout *>(w.m_messageLayout));
        QVERIFY(w.m_dialogs);QVERIFY(w.m_dialogs->isVisible());QVERIFY(!w.m_contacts->isVisible());
        QVERIFY(dynamic_cast<Ui::InputField *>(dynamic_cast<TelegramSearchField *>(w.findChild<QWidget *>("search"))));
        QVERIFY(dynamic_cast<Ui::InputField *>(dynamic_cast<TelegramSearchField *>(w.findChild<QWidget *>("historySearch"))));
        QVERIFY(dynamic_cast<Ui::Platform::DefaultTitleWidget *>(w.findChild<QWidget *>("telegramOriginalTitle")));
        QPointer<QMenu> menu=w.findChild<QMenu *>("mainNavigationMenu"); QVERIFY(menu);
        auto action=[](QMenu *menu,const QString &text)->QAction* {
            for(auto *a:menu->actions()) if(a->text()==text)return a;
            return nullptr;
        };
        QTest::mouseClick(navigation,Qt::LeftButton); QTest::qWait(20);
        Ui::PopupMenu *popup=nullptr;
        for(auto *candidate:navigation->findChildren<QWidget *>())
            if(auto *original=dynamic_cast<Ui::PopupMenu *>(candidate);original && original->isVisible())popup=original;
        QVERIFY(popup);
        // These are actual Qt widget grabs with demo conversation data, not a
        // composited mockup or evidence of a complete original Telegram shell.
        QTest::qWait(250);
        QVERIFY(popup->grab().save("/tmp/original-shell-menu.png"));
        popup->hideMenu(); QTest::qWait(250);
        QVERIFY2(menu,"Closing the original popup destroyed its reusable action model");
        QTest::mouseClick(navigation,Qt::LeftButton);QTest::qWait(250);
        Ui::PopupMenu *reopened=nullptr;
        for(auto *candidate:navigation->findChildren<QWidget *>())
            if(auto *original=dynamic_cast<Ui::PopupMenu *>(candidate);original && original->isVisible())reopened=original;
        QVERIFY(reopened);reopened->hideMenu();QTest::qWait(250);
        QVERIFY(menu);
        auto *devices=action(menu,"联系人 / 局域网设备");QVERIFY(devices);devices->trigger();
        QCOMPARE(w.findChild<QTabBar *>("rosterTabs")->currentIndex(),1);
        auto *settingsAction=action(menu,"个人资料与设置…");QVERIFY(settingsAction);
        settingsAction->trigger();QTest::qWait(250);
        auto *overlay=dynamic_cast<TelegramProfileOverlay *>(w.findChild<QWidget *>("telegramSettingsOverlay"));
        QVERIFY(overlay);QVERIFY(overlay->isVisible());
        QVERIFY(dynamic_cast<Ui::GenericBox *>(overlay->findChild<QWidget *>("telegramSettingsBox")));
        QVERIFY(w.grab().save("/tmp/original-settings-box.png"));
        QPointer<QWidget> settingsGuard=overlay;
        auto *cancel=overlay->findChild<QWidget *>("telegramProfileCancel");QVERIFY(cancel);
        QTest::mouseClick(cancel,Qt::LeftButton);QTRY_VERIFY(!settingsGuard || !settingsGuard->isVisible());
        auto *groupAction=action(menu,"新建讨论组…");QVERIFY(groupAction);groupAction->trigger();QTest::qWait(100);
        auto *groupHost=dynamic_cast<TelegramContactBoxHost *>(w.findChild<QWidget *>("telegramCreateGroupOverlay"));
        QVERIFY(groupHost);QVERIFY(groupHost->isVisible());
        QVERIFY(w.grab().save("/tmp/original-group-box.png"));groupHost->closeBox();QTest::qWait(250);
        w.event("profile_received",{{"peer_id","frozen-peer"},{"profile",QJsonObject{{"nickname","测试联系人"},{"signature","仅为界面演示"}}}});
        auto *profileHost=dynamic_cast<TelegramContactBoxHost *>(w.findChild<QWidget *>("telegramContactProfileOverlay"));
        QVERIFY(profileHost);QCOMPARE(profileHost->property("peerId").toString(),QString("frozen-peer"));
        QVERIFY(w.grab().save("/tmp/original-contact-box.png"));profileHost->closeBox();QTest::qWait(250);
        auto *transferAction=action(menu,"文件传输记录");QVERIFY(transferAction);transferAction->trigger();QTest::qWait(100);
        auto *transferHost=dynamic_cast<TelegramContactBoxHost *>(w.findChild<QWidget *>("telegramTransferOverlay"));
        QVERIFY(transferHost);QVERIFY(w.m_transferList);QVERIFY(w.m_transferList->isVisible());
        QVERIFY(w.grab().save("/tmp/original-transfer-box.png"));transferHost->closeBox();QTest::qWait(250);
        // Trigger real callbacks; cancel modal forms without saving or sending.
        for(const auto &entry:QList<QPair<QString,QString>>{
                {"添加设备 IP…","添加设备 IP"}}) {
            auto *a=action(menu,entry.first);QVERIFY(a);bool seen=false;
            QTimer::singleShot(0,&w,[&]{
                if(auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
                    seen=dialog->windowTitle()==entry.second;dialog->reject();
                }
            });
            a->trigger();QVERIFY2(seen,qPrintable(entry.first));
        }
        auto *sharingAction=action(menu,"共享文件");QVERIFY(sharingAction);sharingAction->trigger();QTest::qWait(100);
        auto *sharingHost=dynamic_cast<TelegramContactBoxHost *>(w.findChild<QWidget *>("telegramSharingOverlay"));
        QVERIFY(sharingHost);QVERIFY(w.m_sharingPane);QVERIFY(w.m_sharingPane->isVisible());
        QVERIFY(w.grab().save("/tmp/original-sharing-box.png"));sharingHost->closeBox();QTest::qWait(250);
        auto *contacts=w.findChild<QListWidget *>("contacts");QVERIFY(contacts->count()>0);
        contacts->setCurrentRow(0);w.openConversation();QTest::qWait(250);
        QVERIFY(w.grab().save("/tmp/original-shell-wide.png"));
        // A real upstream type is insufficient: Qt layouts must allocate its
        // painted surface and keep both action buttons clear of the editor.
        auto *editor=w.findChild<TelegramInputAdapter *>("editor");QVERIFY(editor);
        auto widgetRect=[&](QWidget *widget) {return QRect(widget->mapTo(&w,QPoint()),widget->size());};
        for(const auto &name:{"attachMenu","primary","searchConversation"}) {
            auto *control=w.findChild<QWidget *>(name);QVERIFY(control);
            QVERIFY(dynamic_cast<Ui::IconButton *>(control));
            QVERIFY(control->isVisible());QVERIFY(control->width()>0);QVERIFY(control->height()>0);
            QVERIFY(!widgetRect(control).intersects(widgetRect(editor)));
        }
        auto *chatButton=w.findChild<QWidget *>("conversationMenu");QVERIFY(chatButton);
        QVERIFY(dynamic_cast<Ui::IconButton *>(chatButton));
        auto *chatMenu=chatButton->findChild<QMenu *>();QVERIFY(chatMenu);
        // All popup hosts must retain their reusable command models on close.
        for(const auto &name:{"conversationMenu","attachMenu"}) {
            auto *button=w.findChild<QWidget *>(name);QVERIFY(button);
            QPointer<QMenu> commands=button->findChild<QMenu *>();QVERIFY(commands);
            const int commandCount=commands->actions().size();
            for(int reopen=0;reopen<2;++reopen) {
                QTest::mouseClick(button,Qt::LeftButton);QTest::qWait(250);
                Ui::PopupMenu *visible=nullptr;
                for(auto *candidate:button->findChildren<QWidget *>())
                    if(auto *original=dynamic_cast<Ui::PopupMenu *>(candidate);original && original->isVisible())visible=original;
                QVERIFY(visible);visible->hideMenu();QTest::qWait(250);
                QVERIFY(commands);QCOMPARE(commands->actions().size(),commandCount);
            }
        }
        for(const auto &label:{"查看资料","发送文件…","投屏…","共享文件","聊天字体…"})QVERIFY(action(chatMenu,label));
        action(chatMenu,"投屏…")->trigger();QTest::qWait(20);
        QVERIFY(w.m_screenDialog);QVERIFY(w.m_screenDialog->isVisible());w.m_screenDialog->close();
        auto *searchButton=w.findChild<QWidget *>("searchConversation");QVERIFY(searchButton);
        QVERIFY(dynamic_cast<Ui::IconButton *>(searchButton));
        QTest::mouseClick(searchButton,Qt::LeftButton);QVERIFY(w.m_historyPanel->isVisible());
        w.m_historyPanel->hide();
        w.resize(400,700);QTest::qWait(250);
        QVERIFY(w.grab().save("/tmp/original-shell-narrow.png"));
        for(const auto &name:{"attachMenu","primary","searchConversation"}) {
            auto *control=w.findChild<QWidget *>(name);QVERIFY(control);
            QVERIFY(control->isVisible());QVERIFY(control->width()>0);QVERIFY(control->height()>0);
            QVERIFY(!widgetRect(control).intersects(widgetRect(editor)));
            QVERIFY(widgetRect(control).left()>=0);QVERIFY(widgetRect(control).right()<w.width());
        }
        for(auto *control:{chatButton,static_cast<QWidget *>(searchButton)}) {
            const auto position=control->mapTo(&w,QPoint());
            QVERIFY(position.x()>=0);QVERIFY(position.x()+control->width()<=w.width());
        }
    }
    void fullSingleWindowAndNarrowBack() {
        QTemporaryDir dir; MainWindow w(dir.path(),0,true);w.show();QTest::qWait(30);
        auto *columns=w.findChild<QSplitter *>("telegramColumns");QVERIFY(columns);
        QVERIFY(!w.m_chat->isWindow());
        w.showMinimized();QTest::qWait(10);QVERIFY(w.isMinimized());
        w.openConversation();QTest::qWait(20);QVERIFY(!w.isMinimized());
        auto *list=w.findChild<QListWidget *>("contacts");
        list->setCurrentRow(0);w.openConversation();QTest::qWait(20);
        QVERIFY(w.m_chat->isVisible());QVERIFY(w.m_rosterPanel->isVisible());
        w.resize(400,700);QTest::qWait(30);
        QVERIFY(w.m_chat->isVisible());QVERIFY(!w.m_rosterPanel->isVisible());
        for(auto *avatar:w.m_messages->findChildren<QLabel *>("messageAvatar")) {
            const int left=avatar->mapTo(w.m_messages->viewport(),QPoint()).x();
            QVERIFY(left>=0); QVERIFY(left+avatar->width()<=w.m_messages->viewport()->width());
        }
        w.grab().save("/tmp/telegram-columns-narrow-chat.png");
        auto *back=w.findChild<QWidget *>("backToConversations");QVERIFY(back);
        QTest::mouseClick(back,Qt::LeftButton);QTest::qWait(20);
        QVERIFY(w.m_rosterPanel->isVisible());QVERIFY(!w.m_chat->isVisible());
        w.grab().save("/tmp/telegram-columns-narrow-list.png");
        w.resize(1080,760);QTest::qWait(30);
        QVERIFY(w.m_chat->isVisible());QVERIFY(w.m_rosterPanel->isVisible());
    }
#endif
    void telegramRowPainterIsActuallyConnected() {
        QTemporaryDir dir; MainWindow w(dir.path(),0,true);
        auto *list=w.findChild<QListWidget *>("contacts");
        auto *tabs=w.findChild<QTabBar *>("rosterTabs");
        QVERIFY(list); QVERIFY(tabs);
        for (int mode : {0,1,2}) {
            tabs->setCurrentIndex(mode);
            QListWidgetItem *item=nullptr;
            for(int n=0;n<list->count();++n)
                if(!list->item(n)->isHidden()) {item=list->item(n);break;}
            QVERIFY(item);
            const auto index=list->indexFromItem(item);
#ifdef LANLINK_FULL_TELEGRAM_UI
            w.show();list->setCurrentItem(item);QTest::qWait(40);
            auto *view=w.m_dialogs;QVERIFY(view);QVERIFY(view->isVisible());
            const auto rect=view->rowRect(index);QVERIFY(rect.isValid());
            const auto image=view->viewport()->grab().toImage();
            // A grab is device pixels, while rowRect is logical coordinates.
            // Sample inside the viewport to avoid the fractional-DPI clip edge.
            const qreal dpr=image.devicePixelRatio();
            const int y=qRound(rect.center().y()*dpr);
            QCOMPARE(image.pixelColor(qRound(2*dpr),y),QColor("#419fd9"));
            QCOMPARE(image.pixelColor(image.width()-1-qRound(2*dpr),y),QColor("#419fd9"));
#else
            QStyleOptionViewItem option; option.rect=QRect(0,0,320,62);
            option.widget=list; option.state=QStyle::State_Selected;
            QCOMPARE(list->itemDelegate()->sizeHint(option,index).height(),
                     TelegramLan::TelegramDialogRowHeight);
            QImage image(320,62,QImage::Format_ARGB32); image.fill(Qt::magenta);
            QPainter painter(&image);
            list->itemDelegate()->paint(&painter,option,index); painter.end();
            // Source renderer uses full-row Telegram active palette, not the old
            // inset rounded card. This checks the production delegate, not a mock.
            QCOMPARE(image.pixelColor(0,0),QColor("#419fd9"));
            QCOMPARE(image.pixelColor(319,61),QColor("#419fd9"));
#endif
        }
    }
    void telegramInputIsActuallyConnected() {
        QTemporaryDir a,b; MainWindow w(a.path(),0,false); Node bob("Bob",b.path());
        QVERIFY(bob.start(0,0));w.node()->addPeer("127.0.0.1",bob.port());
        QTRY_COMPARE(w.node()->peers().size(),1);
        auto *editor=w.findChild<TelegramInputAdapter *>("editor");QVERIFY(editor);
        w.findChild<QTabBar *>("rosterTabs")->setCurrentIndex(1);
        auto *contacts=w.findChild<QListWidget *>("contacts");QVERIFY(contacts->count()>0);contacts->setCurrentRow(0);
        editor->setPlainText("telegram input ");
        QInputMethodEvent pre(QString::fromUtf8("中文"),{});QApplication::sendEvent(editor,&pre);
        QTest::keyClick(editor,Qt::Key_Return,Qt::ControlModifier);
        QTest::qWait(30);QCOMPARE(bob.history(w.node()->id()).size(),0);
        QInputMethodEvent commit;commit.setCommitString(QString::fromUtf8("中文"));QApplication::sendEvent(editor,&commit);
        QTest::keyClick(editor,Qt::Key_Return,Qt::ControlModifier);
        QTRY_COMPARE(bob.history(w.node()->id()).size(),1);
        QTest::qWait(50);QCOMPARE(bob.history(w.node()->id()).size(),1);
        QVERIFY(bob.history(w.node()->id()).first().toObject()["text"].toString().contains(QString::fromUtf8("中文")));
        QVERIFY(editor->toPlainText().isEmpty());
        // Exercise the actual visible send control, not just keyboard shortcuts.
        w.show();w.openConversation();QTest::qWait(20);
        auto *send=w.findChild<QWidget *>("primary");QVERIFY(send);
#ifdef LANLINK_FULL_TELEGRAM_UI
        QVERIFY(dynamic_cast<Ui::IconButton *>(send));
#endif
        editor->setPlainText("normal message via send button");
        QTest::mouseClick(send,Qt::LeftButton);
        QTRY_COMPARE(bob.history(w.node()->id()).size(),2);
        QCOMPARE(bob.history(w.node()->id()).last().toObject()["text"].toString(),QString("normal message via send button"));
        QVERIFY(editor->toPlainText().isEmpty());
    }
#ifdef LANLINK_FULL_TELEGRAM_UI
    void originalInputFontAndEmoji() {
        QTemporaryDir dir; MainWindow w(dir.path(),0,true);
        auto *editor=w.findChild<TelegramInputAdapter *>("editor"); QVERIFY(editor);
        QVERIFY(dynamic_cast<Ui::InputField *>(editor));
        QFont font("DejaVu Sans",17); editor->setFont(font);
        editor->setPlainText(QString::fromUtf8("中文 😀"));
        QCOMPARE(editor->rawEditor()->font().pointSize(),17);
        QTest::keyClicks(editor," typed");
        QCOMPARE(editor->rawEditor()->font().pointSize(),17);
        QVERIFY(editor->toPlainText().contains(QString::fromUtf8("😀")));
        auto cursor=editor->rawEditor()->textCursor(); cursor.setPosition(1);
        QCOMPARE(QFontInfo(cursor.charFormat().font()).pixelSize(),QFontInfo(font).pixelSize());
    }
#endif
    void actualLongSendAndDraftOnRejection() {
        QTemporaryDir a,b;
        MainWindow w(a.path(),0,false);
        auto *node=w.findChild<Node *>(); QVERIFY(node);
        Node bob("Bob",b.path()); QVERIFY(bob.start(0,0));
        node->addPeer("127.0.0.1",bob.port());
        QTRY_COMPARE(node->peers().size(),1);
        auto *contacts=w.findChild<QListWidget *>("contacts");
        auto *editor=w.findChild<TelegramInputAdapter *>("editor");
        QVERIFY(contacts); QVERIFY(editor);
        w.findChild<QTabBar *>("rosterTabs")->setCurrentIndex(1);
        QTRY_VERIFY(contacts->count()>0);
        contacts->setCurrentRow(0);
        auto *send=w.findChild<QWidget *>("primary");
        QVERIFY(send);
        const QString text=QString::fromUtf8("中").repeated(43690)+"ab";
        QCOMPARE(text.toUtf8().size(),Node::MaxTextBytes);
        editor->setPlainText(text);
        QTest::mouseClick(send,Qt::LeftButton);
        QTRY_COMPARE(bob.history(node->id()).size(),1);
        QCOMPARE(bob.history(node->id()).first().toObject()["text"].toString(),text);
        QCOMPARE(editor->toPlainText(),QString());
        QVERIFY(contacts->item(0)->toolTip().size()<300); // Full text stays in history, not the list tooltip.
        const QString over=text+QString::fromUtf8("😀"); editor->setPlainText(over);
        QString warning;
        QTimer::singleShot(0,[&]{for(auto widget:QApplication::topLevelWidgets()) {
            if(auto box=qobject_cast<QMessageBox *>(widget)) {warning=box->text();box->accept();}
        }});
        QTest::mouseClick(send,Qt::LeftButton);
        QVERIFY(warning.contains("128 KiB")); QCOMPARE(editor->toPlainText(),over);
        QCOMPARE(bob.history(node->id()).size(),1);
    }
    void capturedImageKeepsRecipient() {
        QTemporaryDir a,b,c; MainWindow w(a.path(),0,false);
        Node bob("Bob",b.path()),carol("Carol",c.path());
        QVERIFY(bob.start(0,0)); QVERIFY(carol.start(0,0));
        w.node()->addPeer("127.0.0.1",bob.port()); w.node()->addPeer("127.0.0.1",carol.port());
        QTRY_COMPARE(w.node()->peers().size(),2);
        // Recipient was approved before an asynchronous capture/preview. The
        // currently selected conversation can change without rerouting it.
        const QString approved=bob.id(); w.m_current=carol.id();
        QImage image(4,4,QImage::Format_RGB32); image.fill(Qt::red);
        QByteArray bytes; QBuffer buffer(&bytes); QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&buffer,"JPEG"));
        w.sendImageTo(approved,bytes);
        QTRY_COMPARE(bob.history(w.node()->id()).size(),1);
        QTest::qWait(50); QCOMPARE(carol.history(w.node()->id()).size(),0);
        QCOMPARE(w.m_current,carol.id());
    }
    void actualDeliveryBadgeFollowsAck() {
        QTemporaryDir a,b;MainWindow w(a.path(),0,false);Node bob("Bob",b.path());
        QVERIFY(bob.start(0,0));w.node()->addPeer("127.0.0.1",bob.port());QTRY_COMPARE(w.node()->peers().size(),1);
        w.m_current=bob.id();QSignalSpy delivered(w.node(),&Node::event);
        const auto id=w.node()->sendMessage(bob.id(),"actual ACK status");QVERIFY(!id.isEmpty());
        QToolButton *button=nullptr;for(auto *candidate:w.findChildren<QToolButton *>("deliveryStatus"))
            if(candidate->property("messageId").toString()==id)button=candidate;
        QVERIFY(button);QTRY_COMPARE(button->text(),QString("对方已接收"));
        QCOMPARE(bob.history(w.node()->id()).size(),1);
        QCOMPARE(w.node()->deliveryStatus(id)["received"].toInt(),1);
        QVERIFY(!button->text().contains("已读"));
        w.appendMessage({{"id","old-evidence-unavailable"},{"direction","out"},{"text","legacy"}});
        QToolButton *unknown=nullptr;for(auto *candidate:w.findChildren<QToolButton *>("deliveryStatus"))
            if(candidate->property("messageId").toString()=="old-evidence-unavailable")unknown=candidate;
        QVERIFY(unknown);QCOMPARE(unknown->text(),QString("送达状态未知"));
    }
#ifdef LANLINK_FULL_TELEGRAM_UI
    void actualGroupPartialReceiptAndDetails() {
        QTemporaryDir a,b,c;MainWindow w(a.path(),0,false);Node bob("在线测试设备",b.path()),carol("离线测试设备",c.path());
        QVERIFY(bob.start(0,0));QVERIFY(carol.start(0,0));
        w.node()->addPeer("127.0.0.1",bob.port());w.node()->addPeer("127.0.0.1",carol.port());QTRY_COMPARE(w.node()->peers().size(),2);
        const auto group="group:"+w.node()->createGroup({bob.id(),carol.id()},"本地双端确认测试");carol.stop();
        w.m_tabs->setCurrentIndex(2);
        for(int n=0;n<w.m_contacts->count();++n)if(w.m_contacts->item(n)->data(Qt::UserRole).toString()==group)w.m_contacts->setCurrentRow(n);
        w.show();w.openConversation();
        const auto id=w.node()->sendMessage(group,QString("真实局域网回环测试：在线设备已保存，离线设备尚未确认。\n")+QString("长消息全文仍然保留，可以展开、复制和保存。\n").repeated(20));
        QVERIFY(!id.isEmpty());QTRY_COMPARE(w.node()->deliveryStatus(id)["received"].toInt(),1);
        QToolButton *button=nullptr;for(auto *candidate:w.findChildren<QToolButton *>("deliveryStatus"))if(candidate->property("messageId").toString()==id)button=candidate;
        QVERIFY(button);QTRY_VERIFY(button->text().contains("1/2"));QVERIFY(!button->text().contains("已读"));
        QTest::qWait(30);QVERIFY(w.grab().save("/tmp/telegram-real-group-ack.png"));
        bool captured=false;
        QTimer::singleShot(30,[&]{for(auto *widget:QApplication::topLevelWidgets())if(auto *dialog=qobject_cast<QDialog *>(widget))
            if(dialog->windowTitle()=="消息接收结果") {captured=dialog->grab().save("/tmp/telegram-real-group-details.png");dialog->reject();}});
        button->click();QVERIFY(captured);
    }
#endif
    void sharingResultsKeepOriginalContext() {
        QTemporaryDir a,b,shared;
        QVERIFY(QDir(shared.path()).mkdir("nested"));
        QFile file(shared.path()+"/nested/data.txt");QVERIFY(file.open(QIODevice::WriteOnly));file.write("data");file.close();
        MainWindow w(a.path(),0,false);Node peer("Shared device",b.path());FileTransfer files(&peer);
        peer.serviceHandler=[&](auto socket,auto request,auto host){return files.handle(socket,request,host);};
        QVERIFY(peer.start(0,0));w.node()->addPeer("127.0.0.1",peer.port());
        QTRY_COMPARE(w.node()->peers().size(),1);
        const auto token=files.publishShare(shared.path());QVERIFY(!token.isEmpty());
        w.m_current=peer.id();
#ifdef LANLINK_FULL_TELEGRAM_UI
        w.show();w.showSharing();QVERIFY(w.m_sharingPane);
        w.m_sharingPane->setInputs(token,"nested");
        auto *browse=w.m_sharingPane->findChild<QWidget*>("originalShareBrowse");QVERIFY(browse);QTest::mouseClick(browse,Qt::LeftButton);
#else
        w.m_shareCode->setText(token);w.m_sharePath->setText("nested");w.browseShare();
#endif
        QTRY_COMPARE(w.m_shareList->count(),1);QVERIFY(w.m_shareList->item(0)->text().contains("data.txt"));
        w.m_shareCode->setText("edited-token");w.m_sharePath->setText("another-directory");
        QCOMPARE(w.m_shareToken,token);QCOMPARE(w.m_shareDirectory,QString("nested"));
        w.m_files->shareListedFor("stale-peer",token,"nested",QJsonArray{QJsonObject{{"name","wrong.txt"}}});
        w.m_files->shareListedFor(peer.id(),token,"old-path",QJsonArray{QJsonObject{{"name","wrong.txt"}}});
        QCOMPARE(w.m_shareList->count(),1);QVERIFY(w.m_shareList->item(0)->text().contains("data.txt"));
        QVERIFY(w.m_shareLocation->text().contains("nested"));
#ifdef LANLINK_FULL_TELEGRAM_UI
        auto *download=w.m_sharingPane->findChild<QWidget*>("originalShareDownload");QVERIFY(download);
        QCOMPARE(download->property("relativePath").toString(),QString("nested/data.txt"));
        w.m_sharingPane->setInputs("edited-token","another-directory");
        QVERIFY(w.grab().save("/tmp/original-sharing-live.png"));
        QCOMPARE(w.m_sharePeer,peer.id());QCOMPARE(w.m_shareToken,token);
        auto *host=dynamic_cast<TelegramContactBoxHost*>(w.findChild<QWidget*>("telegramSharingOverlay"));QVERIFY(host);
        host->closeBox();QTRY_VERIFY(w.m_sharingPane.isNull());w.showSharing();QVERIFY(w.m_sharingPane);
        QCOMPARE(w.m_sharingPane->tokenText(),QString("edited-token"));
        QCOMPARE(w.m_sharingPane->directoryText(),QString("another-directory"));
        download=w.m_sharingPane->findChild<QWidget*>("originalShareDownload");QVERIFY(download);
        QCOMPARE(download->property("relativePath").toString(),QString("nested/data.txt"));
#endif
    }
#ifdef LANLINK_FULL_TELEGRAM_UI
    void originalPrivateAndGroupMessageIdentity() {
        QTemporaryDir dir;MainWindow w(dir.path(),0,true);w.show();QTest::qWait(30);
        for(auto *avatar:w.m_messages->findChildren<QLabel *>("messageAvatar"))QVERIFY(avatar->isHidden());
        for(auto *author:w.m_messages->findChildren<QLabel *>("messageAuthor"))QVERIFY(author->isHidden());
        w.m_peers["group-author"]={{"name","研发同事"}};
        w.m_groups["group:identity-test"]={{"name","研发讨论组"},{"members",QJsonArray{w.node()->id(),"group-author"}}};
        w.refreshContacts();
        QListWidgetItem *groupItem=nullptr;
        for(int i=0;i<w.m_contacts->count();++i)if(w.m_contacts->item(i)->data(Qt::UserRole).toString()=="group:identity-test")groupItem=w.m_contacts->item(i);
        QVERIFY(groupItem);w.selectConversation(groupItem);w.openConversation();
        const double now=QDateTime::currentSecsSinceEpoch()+120;
        auto append=[&](QString id,double time){w.appendMessage({{"id",id},{"direction","in"},{"sender_id","group-author"},{"text",QString("群聊呈现测试：%1").arg(id.right(1))},{"timestamp",time}});};
        auto row=[&](QString id)->QWidget*{for(auto *child:w.m_messages->findChildren<QWidget*>())if(child->property("historyMessageId").toString()==id)return child;return nullptr;};
        append("identity-1",now);append("identity-2",now+10);append("identity-3",now+20);
        auto *first=row("identity-1"),*middle=row("identity-2"),*last=row("identity-3");QVERIFY(first);QVERIFY(middle);QVERIFY(last);
        QVERIFY(!first->findChild<QLabel*>("messageAuthor")->isHidden());
        QVERIFY(middle->findChild<QLabel*>("messageAuthor")->isHidden());
        QVERIFY(last->findChild<QLabel*>("messageAuthor")->isHidden());
        QVERIFY(first->findChild<QLabel*>("messageAvatar")->isHidden());
        QVERIFY(middle->findChild<QLabel*>("messageAvatar")->isHidden());
        QVERIFY(!last->findChild<QLabel*>("messageAvatar")->isHidden());
        append("identity-4",now+30);QVERIFY(last->findChild<QLabel*>("messageAvatar")->isHidden());
        QVERIFY(!row("identity-4")->findChild<QLabel*>("messageAvatar")->isHidden());
        w.appendMessage({{"id","identity-out"},{"direction","out"},{"text","收到，已经核对。"},{"timestamp",now+40}});
        auto *out=row("identity-out");QVERIFY(out);
        QVERIFY(out->findChild<QLabel*>("messageAvatar")->isHidden());QVERIFY(out->findChild<QLabel*>("messageAuthor")->isHidden());
        QTest::qWait(250);QVERIFY(w.grab().save("/tmp/original-group-identity.png"));
    }
    void actualHistoryDecorations() {
        QTemporaryDir dir;MainWindow w(dir.path(),0,true);
        w.m_hasPreviousHistoryMessage=false;
        const auto before=w.findChildren<QWidget *>("telegramDateBadge").size();
        const auto t=QDateTime(QDate(2026,11,12),QTime(9,0)).toSecsSinceEpoch();
        w.appendMessage({{"direction","in"},{"sender_id","same"},{"text","first"},{"timestamp",double(t)}});
        w.appendMessage({{"direction","in"},{"sender_id","same"},{"text","second"},{"timestamp",double(t+30)}});
        w.appendMessage({{"direction","in"},{"sender_id","same"},{"text","next day"},{"timestamp",double(t+86400)}});
        QCOMPARE(w.findChildren<QWidget *>("telegramDateBadge").size(),before+2);
        bool attached=false;for(auto *row:w.m_messages->findChildren<QWidget *>())attached|=row->property("telegramAttachedPrevious").toBool();
        QVERIFY(attached);QVERIFY(w.findChildren<QLabel *>("messageTime").size()>=3);
    }
#endif
    void selectedChatToMailIsOnlyADraft() {
        QTemporaryDir a,b,c;MainWindow w(a.path(),0,false);w.show();
        Node bob("Bob",b.path()),carol("Carol",c.path());QVERIFY(bob.start(0,0));QVERIFY(carol.start(0,0));
        w.node()->addPeer("127.0.0.1",bob.port());w.node()->addPeer("127.0.0.1",carol.port());
        QTRY_COMPARE(w.node()->peers().size(),2);
        const auto selected=w.node()->sendMessage(bob.id(),"这条明确选入邮件");QVERIFY(!selected.isEmpty());
        QVERIFY(!w.node()->sendMessage(bob.id(),"未选中的消息不能导出").isEmpty());
        QVERIFY(!w.node()->sendMessage(carol.id(),"另一个会话的秘密不导出").isEmpty());
        w.m_tabs->setCurrentIndex(1);w.refreshContacts();
        QListWidgetItem *target=nullptr;for(int i=0;i<w.m_contacts->count();++i)if(w.m_contacts->item(i)->data(Qt::UserRole).toString()==bob.id())target=w.m_contacts->item(i);
        QVERIFY(target);w.m_contacts->setCurrentItem(target);w.selectConversation(target);
        auto *book=w.contactMailBook();QVERIFY(book);QString error;QVERIFY(book->setEmail(bob.id(),"bob@example.invalid",&error));
        bool selectionSeen=false;
        QTimer::singleShot(0,[&]{for(auto *widget:QApplication::topLevelWidgets())if(auto *dialog=qobject_cast<QDialog*>(widget)){
            auto *list=dialog->findChild<QListWidget*>("chatMailSelection");if(!list)continue;
            auto *recipient=dialog->findChild<QLineEdit*>("chatMailRecipient");selectionSeen=recipient&&recipient->text()=="bob@example.invalid";
            for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toString()==selected)list->item(i)->setCheckState(Qt::Checked);
            dialog->accept();return;
        }});
        QAction *exportAction=nullptr;for(auto *action:w.findChildren<QAction*>())if(action->text()=="选取聊天内容发邮件…")exportAction=action;
        QVERIFY(exportAction);exportAction->trigger();QVERIFY(selectionSeen);QVERIFY(w.m_mailWindow);QVERIFY(!w.m_mailWindow->isBusy());
        auto *body=w.m_mailWindow->findChild<QPlainTextEdit*>("mailBody");QVERIFY(body);
        QVERIFY(body->toPlainText().contains("这条明确选入邮件"));
        QVERIFY(!body->toPlainText().contains("未选中的消息不能导出"));QVERIFY(!body->toPlainText().contains("另一个会话的秘密不导出"));
        QCOMPARE(w.m_mailWindow->findChild<QLineEdit*>("mailRecipients")->text(),QString("bob@example.invalid"));
        QVERIFY(!w.node()->publicProfile().contains("email"));
#ifdef LANLINK_FULL_TELEGRAM_UI
        QTest::qWait(30);QVERIFY(w.m_mailWindow->grab().save("/tmp/mail-chat-draft.png"));
#endif
    }
    void separateMailWindowEntry() {
        QTemporaryDir dir;MainWindow w(dir.path(),0,true);w.show();
        w.showMail();QTest::qWait(30);
        auto *mail=w.findChild<QDialog *>("mailWindow");QVERIFY(mail);QVERIFY(mail->isVisible());
        w.showMail();QCOMPARE(w.findChild<QDialog *>("mailWindow"),mail);
#ifdef LANLINK_FULL_TELEGRAM_UI
        QVERIFY(mail->grab().save("/tmp/mail-window.png"));
#endif
        mail->close();QTest::qWait(20);
        w.showMail();QVERIFY(w.m_mailWindow->setExternalDraft("recipient@example.invalid","draft","keep this draft"));
        QTimer::singleShot(0,[]{if(auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->done(QMessageBox::No);});
        QVERIFY(!w.close());QVERIFY(w.isVisible());QVERIFY(w.m_mailWindow->hasDraft());
        QTimer::singleShot(0,[]{if(auto *box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->done(QMessageBox::Yes);});
        QVERIFY(w.close());
    }
    void layoutAndDrafts() {
        QTemporaryDir dir;
        MainWindow w(dir.path(), 0, true);
        w.show();
        QTest::qWait(50);
        auto *contacts = w.findChild<QListWidget *>("contacts");
        auto *editor = w.findChild<TelegramInputAdapter *>("editor");
        auto *tabs = w.findChild<QTabBar *>("rosterTabs");
        auto *search = dynamic_cast<TelegramSearchField *>(w.findChild<QWidget *>("search"));
        QVERIFY(contacts);
        QVERIFY(editor);
        QVERIFY(contacts->count() >= 3);
        contacts->setCurrentRow(0);
        editor->setPlainText("draft one");
        contacts->setCurrentRow(1);
        QCOMPARE(editor->toPlainText(), QString());
        editor->setPlainText("draft two");
        contacts->setCurrentRow(0);
        QCOMPARE(editor->toPlainText(), QString("draft one"));
        search->setText("no match");
        for (int i = 0; i < contacts->count(); ++i)
            QVERIFY(contacts->item(i)->isHidden());
        search->clear();
        tabs->setCurrentIndex(2);
        int count = 0;
        for (int i = 0; i < contacts->count(); ++i)
            if (!contacts->item(i)->isHidden()) {
                ++count;
                QVERIFY(contacts->item(i)->data(Qt::UserRole).toString().startsWith("group:"));
            }
        QCOMPARE(count, 1);
        w.resize(340, 600);
        QTest::qWait(20);
        QVERIFY(contacts->width() > 0);
    }
    void composeAndUnread() {
        QTemporaryDir dir; MainWindow w(dir.path(),0,true); w.show();
        auto *contacts=w.findChild<QListWidget *>("contacts");
        auto *editor=w.findChild<TelegramInputAdapter *>("editor");
        auto *split=w.findChild<QSplitter *>("composeSplitter");
        QVERIFY(split); QVERIFY(!split->childrenCollapsible());
        QVERIFY(dynamic_cast<TelegramSearchField *>(w.findChild<QWidget *>("historySearch")));
        QVERIFY(w.findChild<QToolButton *>("sendOptions"));
        contacts->setCurrentRow(0);
        w.saveDemoScreenshot(dir.filePath("chat.png"),"chat");
        editor->setPlainText("send shortcut");
        auto *options=w.findChild<QToolButton *>("sendOptions");
        options->menu()->actions().at(0)->trigger();
        QTest::keyClick(editor,Qt::Key_Return,Qt::ControlModifier);
        QCOMPARE(editor->toPlainText(),QString());
        options->menu()->actions().at(1)->trigger();
        editor->setPlainText("next line");
        QTest::keyClick(editor,Qt::Key_Return,Qt::ShiftModifier);
        QVERIFY(editor->toPlainText().contains("\n"));
        QTest::keyClick(editor,Qt::Key_Return);
        QCOMPARE(editor->toPlainText(),QString());
        options->menu()->actions().at(0)->trigger();
    }
    void notificationIntegration() {
        QTemporaryDir dir; MainWindow w(dir.path(),0,true); w.show();
        auto *contacts=w.findChild<QListWidget *>("contacts");
        const QString id=contacts->item(0)->data(Qt::UserRole).toString();
        auto *notifications=w.findChild<ChatNotifications *>(); QVERIFY(notifications);
        w.node()->event("message", {{"peer_id",id},{"direction","in"},{"text","new message"}});
        QCOMPARE(notifications->unread(id),1);
        QVERIFY(w.windowTitle().contains("1"));
        contacts->setCurrentRow(0);
        w.saveDemoScreenshot(dir.filePath("read.png"),"chat");
#ifdef LANLINK_FULL_TELEGRAM_UI
        QTRY_VERIFY(w.isActiveWindow());
#else
        QTRY_VERIFY(w.m_chat->isActiveWindow());
#endif
        QTRY_COMPARE(notifications->unread(id),0);
        w.node()->event("message", {{"peer_id",id},{"direction","out"},{"text","outgoing"}});
        QCOMPARE(notifications->unread(id),0);
    }
    void imageScrollAndLongText() {
        QTemporaryDir dir;MainWindow w(dir.path(),0,true);w.show();
        w.saveDemoScreenshot(dir.filePath("chat.png"),"chat");
        auto *scroll=w.findChild<QScrollArea *>("messages");auto *bar=scroll->verticalScrollBar();
        for(int i=0;i<20;++i) w.appendMessage({{"direction","in"},{"text",QString("Message %1").arg(i)}});
        QTRY_VERIFY(bar->maximum()>64);
        QTRY_COMPARE(bar->value(),bar->maximum());
        bar->triggerAction(QAbstractSlider::SliderToMinimum);bar->setValue(40);QCOMPARE(bar->value(),40);
        w.appendMessage({{"direction","in"},{"text","new while reading"}});
        QTRY_COMPARE(bar->value(),40);
        // Persisted-history refresh must not jump to bottom while reading older messages.
        const QString peerId="11111111111111111111111111111111";
        QVERIFY(w.node()->remember({{"id",peerId},{"name","Test"},{"port",45870}},"127.0.0.1"));
        w.m_current=peerId;
        for(int i=0;i<25;++i) QVERIFY(!w.node()->sendMessage(peerId,QString("Stored %1").arg(i)).isEmpty());
        QTRY_VERIFY(bar->maximum()>84);
        bar->triggerAction(QAbstractSlider::SliderToMinimum);bar->setValue(60);QCOMPARE(bar->value(),60);
        w.renderHistory();QTRY_COMPARE(bar->value(),60);
        for(int i=0;i<8;++i) w.renderHistory();
        QTRY_COMPARE(bar->value(),60);
        bar->triggerAction(QAbstractSlider::SliderToMaximum);w.renderHistory();QTRY_COMPARE(bar->value(),bar->maximum());
        const QString longText=QString(32768,'x');
        w.appendMessage({{"direction","in"},{"text",longText}});
        w.appendMessage({{"direction","in"},{"text","https://example.com/"+QString(2000,'a')}});
        auto *chat=w.findChild<QDialog *>("chatWindow");chat->resize(760,540);
        w.m_historyPanel->show();QTest::qWait(60);
        for(auto *bubble:scroll->findChildren<QFrame *>("messageBubble")) {
            const auto left=bubble->mapTo(scroll->viewport(),QPoint(0,0)).x();
            QVERIFY(left>=0);QVERIFY(left+bubble->width()<=scroll->viewport()->width());
        }
        auto texts=w.findChildren<TestMessageText *>("messageText");
        TestMessageText *large=nullptr;
        for(auto *text:texts){QVERIFY(text->width()<=320);if(text->toPlainText()==longText)large=text;}
        QVERIFY(large);
#ifdef LANLINK_FULL_TELEGRAM_UI
        QVERIFY(large->isCollapsible());QVERIFY(!large->isExpanded());
        const auto foldedHeight=large->height();large->setExpanded(true);
        QVERIFY(large->height()>foldedHeight);
#endif
        QVERIFY(large->height()>1000);
#ifndef LANLINK_FULL_TELEGRAM_UI
        QCOMPARE(large->document()->defaultTextOption().wrapMode(),QTextOption::WrapAtWordBoundaryOrAnywhere);
#else
        QVERIFY(large->sourceLabels().size()>1);
#endif
        const int oldHeight=large->height();QFont font=w.m_chatFont;font.setPointSize(20);w.applyChatFont(font);
        QVERIFY(large->height()>oldHeight);QCOMPARE(large->toPlainText(),longText);
        QImage picture(900,700,QImage::Format_RGB32);picture.fill(Qt::blue);
        const auto imagePath=dir.filePath("image.png");QVERIFY(picture.save(imagePath));
        w.appendMessage({{"direction","in"},{"text","image"},{"image_path",imagePath}});
        auto *image=w.findChild<QToolButton *>("messageImage");QVERIFY(image);image->click();
        auto *viewer=w.findChild<QDialog *>("imageViewer");QVERIFY(viewer);QVERIFY(viewer->isVisible());
        auto *original=viewer->findChild<QLabel *>("originalImage");QVERIFY(original);
        QCOMPARE(original->pixmap().size(),QSize(900,700));viewer->close();QTest::qWait(10);
    }
    void rosterCards() {
        QTemporaryDir dir;MainWindow w(dir.path(),0,true);w.show();QTest::qWait(20);
        auto *list=w.findChild<QListWidget *>("contacts");auto *tabs=w.findChild<QTabBar *>("rosterTabs");
        auto *search=dynamic_cast<TelegramSearchField *>(w.findChild<QWidget *>("search"));QVERIFY(list);QVERIFY(tabs);
        const QString selected=list->currentItem()->data(Qt::UserRole).toString();
        for(int mode:{0,1,2,0,1}) {
            tabs->setCurrentIndex(mode);QTest::qWait(10);
            QCOMPARE(list->currentItem()->data(Qt::UserRole).toString(),selected);
            for(int i=0;i<list->count();++i) {
                auto *item=list->item(i);if(item->isHidden())continue;
                QVERIFY(!item->data(Qt::UserRole+20).toString().isEmpty());
                QCOMPARE(list->sizeHintForRow(i),TelegramLan::TelegramDialogRowHeight);
                const bool group=item->data(Qt::UserRole).toString().startsWith("group:");
                if(mode==1)QVERIFY(!group);if(mode==2)QVERIFY(group);
            }
        }
        search->setText("讨论组");tabs->setCurrentIndex(2);
        int shown=0;for(int i=0;i<list->count();++i) shown+=!list->item(i)->isHidden();QCOMPARE(shown,1);
        w.resize(340,600);QTest::qWait(20);QCOMPARE(list->horizontalScrollBar()->maximum(),0);
        search->clear();w.refreshContacts();QCOMPARE(list->currentItem()->data(Qt::UserRole).toString(),selected);
    }
    void composeLimitFeedback() {
        QTemporaryDir dir; MainWindow w(dir.path(),0,true);
        auto *contacts=w.findChild<QListWidget *>("contacts"); contacts->setCurrentRow(0);
        auto *editor=w.findChild<TelegramInputAdapter *>("editor");
        auto *hint=w.findChild<QLabel *>("composeHint"); QVERIFY(hint);
        const QString draft=QString(44000,QChar(0x4e2d));
        editor->setPlainText(draft); QVERIFY(hint->text().contains("已超限"));
        bool sawError=false;
        QTimer::singleShot(0,&w,[&] {
            for(auto *widget:QApplication::topLevelWidgets())
                if(auto *box=qobject_cast<QMessageBox *>(widget)) {
                    sawError=box->text().contains("128 KiB"); box->accept();
                }
        });
        w.sendMessage(); QVERIFY(sawError); QCOMPARE(editor->toPlainText(),draft);
        editor->setPlainText("short"); QVERIFY(!hint->text().contains("已超限"));
    }
    void screenshots() {
        QTemporaryDir dir;
        MainWindow w(dir.path(), 0, true);
        w.show();
        QTest::qWait(30);
        w.saveDemoScreenshot(dir.filePath("main.png"));
        QVERIFY(QFileInfo(dir.filePath("main.png")).size() > 0);
        w.saveDemoScreenshot(dir.filePath("chat.png"), "chat");
        auto *chat = w.findChild<QDialog *>("chatWindow");
        QVERIFY(chat);
        QVERIFY(chat->isVisible());
        auto texts=w.findChildren<TestMessageText *>("messageText");
        QVERIFY(texts.size()>=4);
        for(auto *label:texts) {
#ifndef LANLINK_FULL_TELEGRAM_UI
            QVERIFY(!label->openExternalLinks());
#else
            QVERIFY(!label->sourceLabels().isEmpty());
#endif
            QVERIFY(label->minimumWidth()>=60);
        }
        QVERIFY(QFileInfo(dir.filePath("chat.png")).size() > 0);
        chat->close();
        QVERIFY(!chat->isVisible());
        w.saveDemoScreenshot(dir.filePath("history.png"), "history");
        QVERIFY(chat->isVisible());
    }
};
QTEST_MAIN(UiTest)
#include "test_ui.moc"
