"""LanLink: an independent LAN messenger. No proprietary assets or protocols."""
import argparse
import html
import os
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from PySide6.QtCore import QObject, Signal, Qt, QTimer, QSettings, QSize, QUrl
from PySide6.QtGui import QAction, QGuiApplication, QPixmap, QFont, QDesktopServices
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout,
    QHBoxLayout, QListWidget, QListWidgetItem, QTextBrowser, QPlainTextEdit,
    QPushButton, QLabel, QSplitter, QFileDialog, QMessageBox, QInputDialog,
    QTabWidget, QTableWidget, QTableWidgetItem, QHeaderView, QMenu, QDialog, QDialogButtonBox, QLineEdit, QToolButton, QGridLayout, QFontComboBox, QSpinBox, QTabBar, QSizeGrip, QFormLayout)
from .core import Node
from .screen_ui import ScreenController
from .ui_widgets import ChatView, PeerDelegate, nav_icon, plain_question, BlueTitleBar
from .chat_tools import ScreenshotSelection, encode_screenshot
from .profile_ui import ProfileController


class Events(QObject):
    event = Signal(str, dict)


class Window(QMainWindow):
    def __init__(self, storage=None, port=0, demo=False):
        super().__init__()
        self.setWindowTitle("LanLink · 局域网助手（开发预览）")
        self.resize(362, 800)
        self.setWindowFlag(Qt.FramelessWindowHint)
        self.setMinimumSize(340, 600)
        self.events = Events()
        self.events.event.connect(self.on_event)
        self.peers = {}
        self.current = None
        self.groups = {}
        self.drafts = {}
        self.rows = {}
        self.transfer_data = {}
        self.transfer_dirs = {}
        self.pending_previews=set()
        self.settings = QSettings("LanLink", "LanLink")
        self.demo = demo
        self.executor = ThreadPoolExecutor(max_workers=4)
        name = self.settings.value("name", os.environ.get("USERNAME", os.environ.get("USER", "我的电脑")))
        storage = Path(storage or Path.home() / ".lanlink")
        self.node = Node(name, storage, self.events.event.emit, port=port)
        self.profile_controller=ProfileController(self)
        self.build_ui(self.node.name)
        self.set_chat_font(str(self.settings.value("chat_font_family", "Microsoft YaHei UI")), int(self.settings.value("chat_font_size",11)), persist=False)
        self.screen = ScreenController(self)
        self.queue_status = QLabel("")
        self.statusBar().addPermanentWidget(self.queue_status)
        self.queue_timer = QTimer(self)
        self.queue_timer.timeout.connect(self.refresh_queue)
        self.queue_timer.start(2000)
        self.build_sharing_ui()
        if not demo:
            for peer in self.node.peers.values():
                self.on_event("peer", peer)
            for tid, item in self.node.incoming.items():
                self.on_event("progress", {"id": tid, "name": item["name"], "done": 0, "total": item["size"], "status": "paused"})
            if hasattr(self.node, "groups"):
                for group in self.node.groups():
                    self.on_event("group", group)
        if not demo:
            try:
                self.node.start()
                self.address.setText(f"局域网端口 {self.node.port} · 接收文件前需确认")
            except OSError as exc:
                self.address.setText("无法启动网络服务：" + str(exc))
                self.statusBar().showMessage("请检查端口占用，或用 --port 指定其他端口")
        else:
            self.populate_demo()
        if getattr(self.node, "migration_warnings", []):
            warning = " ".join(self.node.migration_warnings)
            self.address.setText(warning)
            self.address.setWordWrap(True)
            self.statusBar().showMessage(warning)

    def build_ui(self, name):
        root=QWidget();root.setObjectName("rosterRoot");self.setCentralWidget(root)
        outer=QVBoxLayout(root);outer.setContentsMargins(0,0,0,0);outer.setSpacing(0)
        self.main_titlebar=BlueTitleBar(self,"LanLink 局域网通信")
        outer.addWidget(self.main_titlebar)
        profile=QWidget();profile.setObjectName("profileHeader")
        pl=QVBoxLayout(profile);pl.setContentsMargins(12,14,12,10)
        identity=QHBoxLayout()
        avatar=QLabel("L");avatar.setFixedSize(58,58);avatar.setAlignment(Qt.AlignCenter);avatar.setStyleSheet("background:#d7eeff;color:#168fe0;font-size:30px;")
        identity.addWidget(avatar)
        names=QVBoxLayout();nickname=QLabel(name);nickname.setTextFormat(Qt.PlainText);nickname.setObjectName("nickname");self.nickname_label=nickname;names.addWidget(nickname)
        self.address=QLabel("局域网通信");self.address.setTextFormat(Qt.PlainText);self.address.setWordWrap(True);self.address.setObjectName("profileAddress");names.addWidget(self.address)
        header_actions=QHBoxLayout()
        for title,callback in [("截图",self.capture_screenshot),("资料",self.configure)]:
            button=QPushButton(title);button.setObjectName("profileAction");button.clicked.connect(callback);header_actions.addWidget(button)
        header_actions.addStretch();names.addLayout(header_actions)
        identity.addLayout(names,1);pl.addLayout(identity)
        searchrow=QHBoxLayout();self.search=QLineEdit();self.search.setPlaceholderText("搜索");self.search.textChanged.connect(self.filter_contacts);searchrow.addWidget(self.search,1)
        add=QPushButton("＋");add.setObjectName("headerButton");add.setFixedSize(28,28)
        menu=QMenu(add);menu.addAction("添加设备 IP",self.add_peer);menu.addAction("创建讨论组",self.create_group);add.setMenu(menu);searchrow.addWidget(add);pl.addLayout(searchrow)
        outer.addWidget(profile)
        self.roster_tabs=QTabBar();self.roster_tabs.setObjectName("rosterTabs")
        for label in ("消息","联系人","讨论组"):self.roster_tabs.addTab(label)
        self.roster_tabs.setExpanding(True);self.roster_tabs.setCurrentIndex(1)
        self.roster_tabs.currentChanged.connect(lambda _:self.filter_contacts(self.search.text()))
        outer.addWidget(self.roster_tabs)
        self.count=QLabel("联系人");self.count.setObjectName("rosterCount");outer.addWidget(self.count)
        self.contacts=QListWidget();self.contacts.setObjectName("contacts");self.contacts.setItemDelegate(PeerDelegate(self.contacts))
        self.contacts.currentItemChanged.connect(self.select_peer)
        self.contacts.itemDoubleClicked.connect(lambda _:self.open_conversation())
        outer.addWidget(self.contacts,1)
        footer=QWidget();footer.setObjectName("footerBar");fl=QHBoxLayout(footer);fl.setContentsMargins(7,6,7,6);fl.setSpacing(3)
        settings=QPushButton("设置");settings.clicked.connect(self.configure);fl.addWidget(settings)
        refresh=QPushButton("添加");refresh.clicked.connect(self.add_peer);fl.addWidget(refresh)
        fl.addStretch()
        self.nav_buttons={}
        for title,index in [("会话",0),("传输",1),("共享",3)]:
            button=QPushButton(title);button.setCheckable(True);button.clicked.connect(lambda checked=False,i=index:self.navigate(i));fl.addWidget(button);self.nav_buttons[index]=button
        more=QPushButton("⋯");more.clicked.connect(lambda:self.navigate(2));fl.addWidget(more)
        outer.addWidget(footer)

        self.chat_dialog=QDialog(self,Qt.Window|Qt.FramelessWindowHint)
        self.chat_dialog.resize(1020,754);self.chat_dialog.setMinimumSize(760,540);self.chat_dialog.setObjectName("chatWindow")
        chatouter=QVBoxLayout(self.chat_dialog);chatouter.setContentsMargins(1,1,1,1);chatouter.setSpacing(0)
        self.chat_titlebar=BlueTitleBar(self.chat_dialog,"会话");self.conversation_title=self.chat_titlebar.title;chatouter.addWidget(self.chat_titlebar)
        self.chat_split=QSplitter();self.chat_split.setHandleWidth(1)
        conversation=QWidget();cl=QVBoxLayout(conversation);cl.setContentsMargins(0,0,0,0);cl.setSpacing(0)
        self.messages=ChatView();self.messages.preview_requested.connect(self.load_link_preview);cl.addWidget(self.messages,1)
        compose=QWidget();compose.setObjectName("composer");compose_layout=QVBoxLayout(compose);compose_layout.setContentsMargins(7,5,7,6);compose_layout.setSpacing(2)
        toolbar=QHBoxLayout();toolbar.setSpacing(3)
        actions=[("☺",self.choose_emoji,"表情"),("截图",self.capture_screenshot,"选择区域截图"),("字体",self.configure_font,"聊天字体与字号"),("文件",self.send_file,"发送文件"),("投屏",lambda:self.screen.open_for_current(),"会话投屏"),("资料",self.profile_controller.view_current_profile,"联系人资料")]
        for title,callback,tip in actions:
            b=QPushButton(title);b.setObjectName("composeAction");b.setToolTip(tip);b.clicked.connect(callback);toolbar.addWidget(b)
            if title=="截图":self.capture_button=b
        toolbar.addStretch()
        history=QPushButton("聊天记录");history.setObjectName("historyButton");history.clicked.connect(self.toggle_history);toolbar.addWidget(history)
        compose_layout.addLayout(toolbar)
        self.editor=QPlainTextEdit();self.editor.setObjectName("editor");self.editor.setPlaceholderText("输入消息…");self.editor.setMinimumHeight(64);self.editor.setMaximumHeight(80);compose_layout.addWidget(self.editor)
        bottom=QHBoxLayout();hint=QLabel("Ctrl + Enter 发送");hint.setObjectName("muted");bottom.addWidget(hint);bottom.addStretch()
        close=QPushButton("关闭");close.clicked.connect(self.chat_dialog.hide);bottom.addWidget(close)
        send=QPushButton("发送");send.setObjectName("primary");send.clicked.connect(self.send_message);bottom.addWidget(send);bottom.addWidget(QSizeGrip(self.chat_dialog));compose_layout.addLayout(bottom)
        cl.addWidget(compose)
        self.chat_split.addWidget(conversation)
        self.history_panel=QWidget();self.history_panel.setObjectName("historyPanel");self.history_panel.setMinimumWidth(220);self.history_panel.setMaximumWidth(340)
        hl=QVBoxLayout(self.history_panel);hl.setContentsMargins(8,8,8,8)
        heading=QLabel("历史消息记录");heading.setObjectName("historyHeading");hl.addWidget(heading)
        self.history_text=QTextBrowser();hl.addWidget(self.history_text,1)
        self.chat_split.addWidget(self.history_panel);self.history_panel.hide();self.chat_split.setSizes([700,280]);chatouter.addWidget(self.chat_split,1)
        action=QAction(self.chat_dialog);action.setShortcut("Ctrl+Return");action.triggered.connect(self.send_message);self.chat_dialog.addAction(action)

        self.tools_dialog=QDialog(self);self.tools_dialog.resize(900,650);self.tools_dialog.setWindowTitle("工具")
        tl=QVBoxLayout(self.tools_dialog);tl.setContentsMargins(0,0,0,0)
        self.tabs=QTabWidget();self.tabs.tabBar().hide();tl.addWidget(self.tabs)
        self.tabs.addTab(QWidget(),"会话")
        self.transfers=QTableWidget(0,4);self.transfers.setHorizontalHeaderLabels(["文件","进度","状态","操作"])
        self.transfers.horizontalHeader().setSectionResizeMode(0,QHeaderView.Stretch);self.transfers.horizontalHeader().setSectionResizeMode(1,QHeaderView.ResizeToContents);self.transfers.setEditTriggers(QTableWidget.NoEditTriggers);self.tabs.addTab(self.transfers,"文件传输")
        about = QTextBrowser()
        about.setHtml("<h2>开发预览 · 独立实现</h2><p>已实现局域网发现、单聊、固定成员讨论组、消息历史、发送端离线待发、确认接收文件、续传和完整性校验、只读共享及低帧率投屏。</p>"
            "<p><b>仅在可信局域网使用。</b>当前协议无端到端加密或设备身份认证，不要传输敏感信息，不要将端口映射至公网。</p>"
            "<p>本版本不是飞鸽传书，也不能与飞鸽互通。文件夹整包传输、远控、剪贴板协同、自动更新尚未完成。支持表情、区域截图、聊天字体、公开资料和点击链接预览。投屏不含音频；离线待发需要发送端重新运行；共享口令持有者可读取共享文件。</p>"
            "<p>聊天记录保存在本机用户目录 .lanlink，未加密。文件必须由接收方选择目录并确认后才写入。</p>")
        self.tabs.addTab(about, "功能与说明")
        self.tabs.currentChanged.connect(self.update_navigation)
        self.statusBar().setStyleSheet("QStatusBar {background:#2196ed;color:white;font-size:10px;border:0;}")
        self.statusBar().show()
        self.update_navigation(0)

    def open_conversation(self):
        if not self.require_peer():return
        self.chat_dialog.show();self.chat_dialog.raise_();self.chat_dialog.activateWindow()

    def toggle_history(self):
        self.history_panel.setVisible(not self.history_panel.isVisible())
        self.refresh_history()

    def refresh_history(self):
        self.history_text.clear()
        if not self.current:return
        for row in ([] if self.demo else self.node.history(self.current)):
            who="我" if row.get("direction")=="out" else self.peers.get(row.get("sender_id",self.current),{}).get("name","对方")
            self.history_text.append(html.escape(who)+"<br>"+html.escape(row.get("text","")))
        if self.demo:self.history_text.setPlainText("演示记录\n\n"+self.messages.toPlainText())

    def navigate(self,index):
        self.tabs.setCurrentIndex(index);self.update_navigation(index)
        if index==0:self.open_conversation()

    def update_navigation(self,index):
        for target,button in self.nav_buttons.items():button.setChecked(target==index)
        if index!=0:
            self.tools_dialog.setWindowTitle(self.tabs.tabText(index));self.tools_dialog.show();self.tools_dialog.raise_()

    def filter_contacts(self,text):
        mode=self.roster_tabs.currentIndex() if hasattr(self,"roster_tabs") else 1
        for i in range(self.contacts.count()):
            item=self.contacts.item(i);group=str(item.data(Qt.UserRole)).startswith("group:")
            visible=(mode==0 or (mode==1 and not group) or (mode==2 and group)) and text.casefold() in item.text().casefold()
            item.setHidden(not visible)
        visible_count=sum(not self.contacts.item(i).isHidden() for i in range(self.contacts.count()))
        self.count.setText(("会话" if mode==0 else "局域网设备" if mode==1 else "讨论组")+f"[{visible_count}]")

    def select_peer(self, item, previous=None):
        if not item:
            return
        if self.current is not None:
            self.drafts[self.current]=self.editor.toPlainText()
        self.current = item.data(Qt.UserRole)
        self.editor.setPlainText(self.drafts.get(self.current,""))
        if hasattr(self, "share_entries"):
            self.share_entries.clear()
            self.share_peer = None
        if self.current.startswith("group:"):
            group = self.groups[self.current]
            self.conversation_title.setText(group.get("name", "讨论组") + " · " + str(len(group.get("members", []))) + " 位成员")
        else:
            peer = self.peers[self.current]
            self.conversation_title.setText(peer.get("name", self.current))
        self.chat_dialog.setWindowTitle(self.conversation_title.text())
        if hasattr(self,"share_target"):
            self.share_target.setText("共享目标：" + self.conversation_title.text())
        if hasattr(self,"screen"):
            self.screen.start_button.setText("投屏给：" + self.conversation_title.text())
        self.messages.clear()
        if not self.demo:
            for msg in self.node.history(self.current):
                self.append_message(msg)
        self.refresh_history()

    def append_message(self, data):
        mine = data.get("direction") in ("out", "sent", "outgoing")
        who = "我" if mine else self.peers.get(data.get("sender_id", data.get("peer_id", self.current)), {}).get("name", "对方")
        self.messages.add_message(who, data.get("text", ""), mine, data.get("image_path"))

    def background(self, fn, *args):
        def run():
            try:
                fn(*args)
            except Exception as exc:
                self.events.event.emit("error", {"message": str(exc)})
        self.executor.submit(run)

    def on_event(self, kind, data):
        if kind == "group":
            key = "group:" + data["id"]
            self.groups[key] = data
            for i in range(self.contacts.count()):
                if self.contacts.item(i).data(Qt.UserRole) == key:
                    self.contacts.item(i).setText("讨论组 · " + data["name"])
                    return
            item = QListWidgetItem("讨论组 · " + data["name"])
            item.setData(Qt.UserRole, key)
            self.contacts.addItem(item)
            self.count.setText(f"设备与讨论组 · {self.contacts.count()}")
            self.filter_contacts(self.search.text())
        elif kind in ("queued", "delivered"):
            self.statusBar().showMessage("消息已加入待发队列，对方可达后自动重试" if kind == "queued" else "消息已送达", 8000)
        elif kind == "peer":
            pid = data["id"]
            self.peers[pid] = data
            for i in range(self.contacts.count()):
                item = self.contacts.item(i)
                if item.data(Qt.UserRole) == pid:
                    item.setText(f'{data["name"]}\n{data["host"]}:{data["port"]}')
                    break
            else:
                item = QListWidgetItem(f'{data["name"]}\n{data["host"]}:{data["port"]}')
                item.setData(Qt.UserRole, pid)
                self.contacts.addItem(item)
            self.count.setText(f"设备与讨论组 · {self.contacts.count()}")
            self.filter_contacts(self.search.text())
        elif kind == "message":
            if data.get("peer_id") == self.current:
                self.append_message(data)
            self.statusBar().showMessage("消息已更新", 3000)
        elif kind == "offer":
            name = self.peers.get(data["peer_id"], {}).get("name", "未知设备")
            answer = plain_question(self, "接收文件", f'{name} 希望发送：\n{data["name"]}\n{data["size"]:,} 字节\n\n仅接收来自可信设备的文件。是否接收？')
            folder = QFileDialog.getExistingDirectory(self, "选择接收目录") if answer == QMessageBox.Yes else ""
            if folder:
                self.transfer_dirs[data["id"]] = folder
                self.background(self.node.accept_file, data["id"], folder)
            else:
                self.node.reject_file(data["id"])
        elif kind in ("progress", "complete", "offered"):
            tid = data["id"]
            self.transfer_data[tid] = data
            if tid not in self.rows:
                self.rows[tid] = self.transfers.rowCount()
                self.transfers.insertRow(self.rows[tid])
            row = self.rows[tid]
            total = data.get("total", 0)
            done = data.get("done", 0)
            percent = f"{done * 100 / total:.0f}%" if total else "0%"
            status = data.get("status", "")
            label = {"offered": "等待对方确认", "progress": "正在传输", "complete": "接收完成 · 校验通过", "sent_unverified": "已发送 · 接收校验未确认", "failed": "失败", "paused": "已暂停 · 可续传"}.get(status, status)
            if status in ("complete", "sent_unverified"):
                percent = "100%"
            for col, text in enumerate([data.get("name", "文件"), percent, label]):
                self.transfers.setItem(row, col, QTableWidgetItem(text))
            if hasattr(self.node, "cancel_transfer") and self.transfers.cellWidget(row, 3) is None:
                cancel = QPushButton("取消")
                cancel.clicked.connect(lambda checked=False, ident=tid: self.transfer_action(ident))
                self.transfers.setCellWidget(row, 3, cancel)
            if self.transfers.cellWidget(row, 3):
                self.transfers.cellWidget(row, 3).setEnabled(status not in ("complete", "sent_unverified", "failed"))
                self.transfers.cellWidget(row, 3).setText("续传" if status == "paused" and tid in self.node.incoming else "取消")
        elif kind == "preview_finished":
            self.pending_previews.discard(data["url"])
        elif kind == "link_preview":
            self.show_link_preview(data)
        elif kind == "share_list":
            self.share_entries.clear()
            if data["peer_id"] != self.current:
                return
            self.share_peer = data["peer_id"]
            self.share_context = (data["token"], data["path"])
            for entry in data["entries"]:
                item = QListWidgetItem(("[文件夹] " if entry["directory"] else "[文件] ") + entry["name"] + ("" if entry["directory"] else f" · {entry["size"]:,} 字节"))
                item.setData(Qt.UserRole, entry)
                self.share_entries.addItem(item)
        elif kind == "error":
            self.statusBar().showMessage(data.get("message", "操作失败"), 15000)
            # Network input must not trigger an unbounded stack of modal dialogs.
            self.address.setText("操作未完成：" + data.get("message", "操作失败")[:180])

    def require_peer(self):
        if not self.current:
            self.statusBar().showMessage("请先在左侧选择设备", 5000)
            return False
        return True

    def send_message(self):
        text = self.editor.toPlainText().strip()
        if not self.require_peer() or not text:
            return
        if self.demo:
            self.append_message({"text": text, "direction": "out"})
        else:
            if self.current.startswith("group:"):
                self.background(self.node.send_group_message, self.current[6:], text)
            else:
                self.background(self.node.send_message, self.current, text)
        self.editor.clear()

    def send_file(self):
        if self.current and self.current.startswith("group:"):
            self.statusBar().showMessage("文件和投屏请先选择单台设备", 5000)
            return
        if not self.require_peer() or self.demo:
            return
        file, _ = QFileDialog.getOpenFileName(self, "选择发送的文件")
        if file:
            self.background(self.node.offer_file, self.current, file)
            self.tabs.setCurrentIndex(1)

    def add_peer(self):
        value, ok = QInputDialog.getText(self, "添加设备", "输入对方局域网 IP:端口，例如 192.168.1.25:45872")
        if ok and value.strip():
            try:
                host, port = value.strip().rsplit(":", 1)
                self.background(self.node.add_peer, host, int(port))
            except (ValueError, OSError) as exc:
                QMessageBox.warning(self, "无法添加", str(exc))

    def load_link_preview(self,url):
        if url in self.pending_previews or len(self.pending_previews)>=2:
            self.statusBar().showMessage("已有链接预览正在加载，请稍候",3000)
            return
        self.pending_previews.add(url)
        self.statusBar().showMessage("正在加载链接预览…")
        def load():
            from .link_preview import fetch_preview
            try:
                result=fetch_preview(url)
                self.events.event.emit("link_preview",result)
            finally:
                self.events.event.emit("preview_finished",{"url":url})
        self.background(load)

    def show_link_preview(self,data):
        self.statusBar().showMessage("链接预览已加载",3000)
        dialog=QDialog(self)
        dialog.setWindowTitle("链接预览")
        layout=QVBoxLayout(dialog)
        for text in [data.get("title") or "无标题",data.get("description") or "该网页未提供简介",data.get("url","")]:
            label=QLabel(text)
            label.setTextFormat(Qt.PlainText)
            label.setTextInteractionFlags(Qt.TextSelectableByMouse)
            label.setWordWrap(True)
            label.setMaximumWidth(560)
            layout.addWidget(label)
        buttons=QDialogButtonBox(QDialogButtonBox.Close)
        open_button=buttons.addButton("在浏览器中打开",QDialogButtonBox.ActionRole)
        open_button.clicked.connect(lambda:QDesktopServices.openUrl(QUrl(data.get("url",""))))
        buttons.rejected.connect(dialog.reject)
        layout.addWidget(buttons)
        dialog.exec()

    def choose_emoji(self):
        dialog=QDialog(self)
        dialog.setWindowTitle("选择表情")
        layout=QGridLayout(dialog)
        emojis=["😀","😄","😊","🙂","😉","😍","😎","🤔","😅","😂","🥳","😢","😮","👍","👎","👏","🙏","🤝","💪","❤️","🎉","✅","❓","💡","🚀","🔥","☕","🌟"]
        for index,emoji in enumerate(emojis):
            button=QPushButton(emoji)
            button.setFixedSize(46,42)
            button.setStyleSheet("font-size:22px;padding:2px;")
            button.clicked.connect(lambda checked=False,value=emoji:self.insert_emoji(value,dialog))
            layout.addWidget(button,index//7,index%7)
        dialog.exec()

    def insert_emoji(self,value,dialog=None):
        self.editor.insertPlainText(value)
        self.editor.setFocus()
        if dialog:dialog.accept()

    def set_chat_font(self,family,size,persist=True):
        size=max(9,min(28,int(size)))
        family=str(family)[:100]
        self.messages.apply_font(family,size)
        escaped=family.replace('"','').replace(';','')
        self.editor.setFont(QFont(family,size))
        self.editor.setStyleSheet(f'font-family:"{escaped}";font-size:{size}pt;')
        if persist:
            self.settings.setValue("chat_font_family",family)
            self.settings.setValue("chat_font_size",size)
        self.chat_font_family=family
        self.chat_font_size=size

    def configure_font(self):
        dialog=QDialog(self)
        dialog.setWindowTitle("聊天字体")
        layout=QVBoxLayout(dialog)
        layout.addWidget(QLabel("仅影响本机消息显示和输入，不改变对方的字体。"))
        family=QFontComboBox()
        family.setCurrentFont(QFont(self.chat_font_family))
        layout.addWidget(family)
        size=QSpinBox()
        size.setRange(9,28)
        size.setSuffix(" pt")
        size.setValue(self.chat_font_size)
        layout.addWidget(size)
        sample=QLabel("你好，LanLink！ Hello 🙂")
        layout.addWidget(sample)
        def update_sample():
            sample.setFont(QFont(family.currentFont().family(),size.value()))
        family.currentFontChanged.connect(update_sample)
        size.valueChanged.connect(update_sample)
        update_sample()
        buttons=QDialogButtonBox(QDialogButtonBox.Ok|QDialogButtonBox.Cancel)
        buttons.accepted.connect(dialog.accept)
        buttons.rejected.connect(dialog.reject)
        layout.addWidget(buttons)
        if dialog.exec()==QDialog.Accepted:
            self.set_chat_font(family.currentFont().family(),size.value())

    def capture_screenshot(self):
        if not self.require_peer():return
        screens=QGuiApplication.screens()
        if not screens:return
        labels=[f"屏幕 {i+1} · {screen.name()}" for i,screen in enumerate(screens)]
        selected,ok=QInputDialog.getItem(self,"选择截图屏幕","选择后拖动截取范围；发送前可预览和取消。",labels,0,False)
        if not ok:return
        screen=screens[labels.index(selected)]
        target=self.current
        self.capture_button.setEnabled(False)
        self.capture_windows=[w for w in (self,self.chat_dialog,self.tools_dialog) if w.isVisible()]
        for visible in self.capture_windows:visible.hide()
        def grab():
            try:
                pix=screen.grabWindow(0)
            except RuntimeError:
                pix=QPixmap()
            if pix.isNull():
                for visible in getattr(self,"capture_windows",[self]):visible.show()
                self.capture_button.setEnabled(True)
                self.on_event("error",{"message":"无法截图，请检查系统屏幕录制权限。部分 Wayland 环境不支持此方式。"})
                return
            self.capture_overlay=ScreenshotSelection(pix,screen.geometry())
            self.capture_overlay.finished.connect(lambda result:self.preview_screenshot(result,target))
            self.capture_overlay.show()
            self.capture_overlay.activateWindow()
            self.capture_overlay.setFocus()
        QTimer.singleShot(250,grab)

    def preview_screenshot(self,pixmap,target):
        for visible in getattr(self,"capture_windows",[self]):visible.show()
        self.chat_dialog.activateWindow()
        self.capture_button.setEnabled(True)
        if pixmap is None:return
        try:payload=encode_screenshot(pixmap)
        except ValueError as exc:
            self.on_event("error",{"message":str(exc)})
            return
        dialog=QDialog(self)
        dialog.setWindowTitle("截图预览 · 确认后才发送")
        layout=QVBoxLayout(dialog)
        target_name=self.groups.get(target,{}).get("name") if target.startswith("group:") else self.peers.get(target,{}).get("name")
        label=QLabel(f"发送给：{target_name or target}\n请检查截图是否包含密码、客户资料等敏感信息。图片会保存在双方本机聊天记录中。")
        label.setWordWrap(True)
        label.setTextFormat(Qt.PlainText)
        layout.addWidget(label)
        preview=QLabel()
        rendered=QPixmap()
        rendered.loadFromData(payload,"JPEG")
        preview.setPixmap(rendered.scaled(720,420,Qt.KeepAspectRatio,Qt.SmoothTransformation))
        layout.addWidget(preview)
        buttons=QDialogButtonBox(QDialogButtonBox.Ok|QDialogButtonBox.Cancel)
        buttons.button(QDialogButtonBox.Ok).setText("发送截图")
        buttons.accepted.connect(dialog.accept)
        buttons.rejected.connect(dialog.reject)
        layout.addWidget(buttons)
        if dialog.exec()==QDialog.Accepted:
            self.send_screenshot_payload(target,payload)

    def send_screenshot_payload(self,target,payload):
        if self.demo:
            from .image_payload import save_image,encode_image
            path=save_image(self.node.root,encode_image(payload))
            self.append_message({"text":"[截图]","direction":"out","image_path":path})
        elif target.startswith("group:"):
            self.background(self.node.send_group_image,target[6:],payload)
        else:
            self.background(self.node.send_image,target,payload)

    def refresh_queue(self):
        count = len(self.node.pending_messages()) if hasattr(self.node, "pending_messages") else 0
        self.queue_status.setText(f"待投递：{count} 条（发送端需保持运行）" if count else "")

    def transfer_action(self, tid):
        data = self.transfer_data.get(tid, {})
        if data.get("status") == "paused" and tid in self.node.incoming:
            folder = self.transfer_dirs.get(tid) or QFileDialog.getExistingDirectory(self, "选择原接收目录以续传")
            if folder:
                self.transfer_dirs[tid] = folder
                self.background(self.node.accept_file, tid, folder)
        else:
            self.node.cancel_transfer(tid)

    def build_sharing_ui(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        note = QLabel("共享目录只读。访问口令持有者可以浏览和下载目录内文件；请只共享专用目录，不要共享整个磁盘。\n共享口令仅本次运行有效，关闭程序或撤销后失效。当前连接未加密，仅限可信局域网。")
        note.setWordWrap(True)
        layout.addWidget(note)
        local = QHBoxLayout()
        publish = QPushButton("选择目录并开启共享")
        publish.clicked.connect(self.publish_share)
        local.addWidget(publish)
        self.local_share = QLineEdit()
        self.local_share.setReadOnly(True)
        self.local_share.setPlaceholderText("本机共享口令（请自行复制给可信设备）")
        local.addWidget(self.local_share, 1)
        revoke = QPushButton("撤销共享")
        revoke.clicked.connect(self.revoke_share)
        local.addWidget(revoke)
        layout.addLayout(local)
        self.share_target=QLabel("共享目标：请先在消息页选择设备")
        self.share_target.setTextFormat(Qt.PlainText)
        layout.addWidget(self.share_target)
        self.share_token = QLineEdit()
        self.share_token.setPlaceholderText("输入对方提供的共享口令")
        layout.addWidget(self.share_token)
        row = QHBoxLayout()
        self.share_path = QLineEdit()
        self.share_path.setPlaceholderText("相对目录，留空为根目录")
        row.addWidget(self.share_path, 1)
        refresh = QPushButton("浏览 / 刷新")
        refresh.clicked.connect(self.browse_share)
        row.addWidget(refresh)
        layout.addLayout(row)
        self.share_entries = QListWidget()
        self.share_entries.itemDoubleClicked.connect(self.open_shared_entry)
        layout.addWidget(self.share_entries, 1)
        download = QPushButton("打开目录 / 下载选中文件")
        download.clicked.connect(lambda: self.open_shared_entry(self.share_entries.currentItem()))
        layout.addWidget(download)
        self.share_peer = None
        self.tabs.addTab(page, "文件共享")

    def publish_share(self):
        if self.demo:
            return
        folder = QFileDialog.getExistingDirectory(self, "选择只读共享目录")
        if folder and plain_question(self, "确认共享", "允许持有口令的设备读取该目录内文件？\n" + folder) == QMessageBox.Yes:
            self.revoke_share()
            try:
                self.local_share.setText(self.node.publish_share(folder))
            except Exception as exc:
                self.on_event("error", {"message": str(exc)})

    def revoke_share(self):
        if self.local_share.text():
            self.node.revoke_share(self.local_share.text())
            self.local_share.clear()

    def browse_share(self):
        if self.demo or not self.require_peer():
            return
        if self.current.startswith("group:"):
            self.statusBar().showMessage("请选择单台设备", 5000)
            return
        peer, token, path = self.current, self.share_token.text().strip(), self.share_path.text().strip()
        def fetch():
            entries = self.node.list_share(peer, token, path)
            self.events.event.emit("share_list", {"peer_id": peer, "entries": entries, "token": token, "path": path})
        self.background(fetch)

    def open_shared_entry(self, item):
        if not item or not self.share_peer:
            return
        entry = item.data(Qt.UserRole)
        token, root = self.share_context
        path = "/".join(filter(None, [root.rstrip("/"), entry["name"]]))
        if entry["directory"]:
            self.share_path.setText(path)
            self.share_token.setText(token)
            self.browse_share()
            return
        folder = QFileDialog.getExistingDirectory(self, "确认下载并选择保存目录")
        if folder:
            self.background(self.node.download_share, self.share_peer, token, path, folder)

    def create_group(self):
        if self.demo:
            return
        dialog = QDialog(self)
        dialog.setWindowTitle("创建讨论组")
        layout = QVBoxLayout(dialog)
        name = QLineEdit("讨论组")
        layout.addWidget(QLabel("名称"))
        layout.addWidget(name)
        choices = QListWidget()
        for pid, peer in self.peers.items():
            item = QListWidgetItem(peer["name"])
            item.setData(Qt.UserRole, pid)
            item.setFlags(item.flags() | Qt.ItemIsUserCheckable)
            item.setCheckState(Qt.Unchecked)
            choices.addItem(item)
        layout.addWidget(choices)
        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(dialog.accept)
        buttons.rejected.connect(dialog.reject)
        layout.addWidget(buttons)
        if dialog.exec() == QDialog.Accepted:
            ids = [choices.item(i).data(Qt.UserRole) for i in range(choices.count()) if choices.item(i).checkState() == Qt.Checked]
            if ids and name.text().strip():
                self.background(self.node.create_group, ids, name.text().strip())
            else:
                self.statusBar().showMessage("请选择至少一个成员并填写名称", 5000)

    def configure(self):
        from .profiles import FIELDS,validate_profile
        from .profile_ui import LABELS
        dialog=QDialog(self,Qt.Window|Qt.FramelessWindowHint)
        dialog.resize(700,560)
        layout=QVBoxLayout(dialog);layout.setContentsMargins(1,1,1,1);layout.setSpacing(0)
        layout.addWidget(BlueTitleBar(dialog,"设置"))
        tabs=QTabWidget();layout.addWidget(tabs,1)
        system=QWidget();system.setStyleSheet("background:#f1f1f1;")
        form=QFormLayout(system);form.setContentsMargins(26,24,26,24);form.setSpacing(20)
        for title,value in [("本机端口",str(self.node.port)),("发现方式","UDP 广播 + 手动添加 IP"),("文件接收","每次确认接收；不覆盖同名文件"),("离线待发","由发送端保存，双方运行且可达时重试"),("安全范围","仅可信局域网；连接未加密")]:
            field=QLineEdit(value);field.setReadOnly(True);form.addRow(title,field)
        font=QPushButton("聊天字体与字号…");font.clicked.connect(self.configure_font);form.addRow("显示",font)
        note=QLabel("以上为当前已实现的设置。原版的网卡绑定、自动接收、开机自启等尚未提供，不显示为可用选项。")
        note.setWordWrap(True);form.addRow(note)
        tabs.addTab(system,"系统设置")
        personal=QWidget();personal.setStyleSheet("background:#f1f1f1;")
        fields=QFormLayout(personal);fields.setContentsMargins(26,24,26,24);fields.setSpacing(20)
        notice=QLabel("保存后昵称会广播；其他资料可由可达的局域网客户端查看。请勿填写敏感信息。")
        notice.setWordWrap(True);fields.addRow(notice)
        edits={};current=self.node.public_profile()
        for key,limit in FIELDS.items():
            entry=QLineEdit(current.get(key,""));entry.setMaxLength(limit);edits[key]=entry;fields.addRow(LABELS[key],entry)
        tabs.addTab(personal,"个人设置")
        buttons=QDialogButtonBox(QDialogButtonBox.Ok|QDialogButtonBox.Cancel);buttons.accepted.connect(dialog.accept);buttons.rejected.connect(dialog.reject);layout.addWidget(buttons)
        if dialog.exec()==QDialog.Accepted:
            try:
                result=self.node.update_profile(validate_profile({key:field.text() for key,field in edits.items()}))
                self.settings.setValue("name",result["nickname"])
                self.nickname_label.setText(result["nickname"])
            except (OSError,ValueError) as exc:self.on_event("error",{"message":str(exc)})

    def populate_demo(self):
        self.address.setText("界面演示 · 示例数据，不代表真实设备或传输")
        self.on_event("peer", {"id": "demo1", "name": "研发工作站", "host": "192.168.1.21", "port": 45872})
        self.on_event("peer", {"id": "demo2", "name": "测试电脑", "host": "192.168.1.35", "port": 45872})
        self.contacts.setCurrentRow(0)
        self.append_message({"direction": "in", "text": "新版本测试包准备好了吗？"})
        self.append_message({"direction": "out", "text": "准备好了，我发给你。收到后请核对版本。"})
        self.on_event("progress", {"id": "demo", "name": "firmware_test.zip", "done": 100, "total": 100, "status": "已完成（演示）"})

    def closeEvent(self, event):
        self.chat_dialog.close()
        self.tools_dialog.close()
        self.screen.stop()
        self.node.stop()
        self.executor.shutdown(wait=False, cancel_futures=True)
        super().closeEvent(event)


STYLE = """
QWidget {font-family:'Microsoft YaHei UI','Noto Sans CJK SC',sans-serif;font-size:13px;color:#151515;}
QMainWindow,QDialog {background:white;}
QWidget#rosterRoot {border:1px solid #2196ed;background:white;}
QWidget#blueTitleBar,QWidget#profileHeader,QWidget#footerBar {background:#2196ed;}
QLabel#nickname {font-size:21px;font-weight:600;color:white;}
QLabel#profileAddress {font-size:12px;color:white;}
QWidget#profileHeader QLineEdit {background:#f4f4f4;border:0;border-radius:3px;padding:7px;color:#333;}
QPushButton#profileAction {background:transparent;color:white;border:0;padding:2px 5px;}
QPushButton#headerButton {background:transparent;border:0;color:white;font-size:23px;padding:0;}
QPushButton#headerButton::menu-indicator {image:none;}
QTabBar#rosterTabs::tab {background:white;border:0;border-bottom:2px solid transparent;padding:17px 8px;color:#666;}
QTabBar#rosterTabs::tab:selected {color:#006ac5;border-bottom:2px solid #41a5ff;}
QLabel#rosterCount {background:#fafafa;color:#5d6b78;font-size:11px;padding:7px 3px;}
QListWidget#contacts {border:0;background:white;outline:0;}
QWidget#footerBar QPushButton {background:transparent;color:white;border:0;padding:5px 6px;}
QWidget#footerBar QPushButton:hover {background:#1481d1;}
QWidget#chatWindow {border:1px solid #2196ed;}
QScrollArea#chatView,QWidget#chatBody {background:white;border:0;}
QWidget#composer {background:white;border-top:1px solid #ddd;}
QPushButton#composeAction {border:0;background:transparent;color:#91999e;padding:3px 5px;}
QPlainTextEdit#editor {background:white;border:0;}
QPushButton {background:#fff;border:1px solid #ccc;border-radius:2px;padding:5px 13px;}
QPushButton:hover {background:#eef8ff;border-color:#80b9e8;}
QPushButton#primary {background:#eaf7ff;color:#111;min-width:62px;border:1px solid #888;}
QPushButton#historyButton {background:#e9f6ff;color:#005d9f;border:1px solid #2497ec;padding:4px 7px;}
QLabel#muted {color:#aaa;font-size:11px;}
QWidget#historyPanel {background:#f1f1f1;border-left:1px solid #d0d0d0;}
QLabel#historyHeading {background:#ddd;padding:4px;}
QTextBrowser {border:0;background:transparent;padding:4px;}
QLineEdit,QSpinBox,QFontComboBox {background:white;border:1px solid #bbb;padding:4px;}
QTableWidget {border:0;background:white;gridline-color:#ddd;}
QHeaderView::section {background:#eee;padding:7px;border:1px solid #ddd;}
QTabWidget::pane {border:0;}
QScrollBar:vertical {width:9px;background:#f6f6f6;}
QScrollBar::handle:vertical {background:#d4d4d4;border-radius:4px;min-height:24px;}
QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical {height:0;}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-dir")
    parser.add_argument("--port", type=int, default=45872)
    parser.add_argument("--demo", action="store_true")
    parser.add_argument("--screenshot")
    args = parser.parse_args()
    app = QApplication(sys.argv[:1])
    app.setStyle("Fusion")
    app.setStyleSheet(STYLE)
    window = Window(args.data_dir, args.port, args.demo)
    window.show()
    if args.screenshot:
        def capture():
            window.grab().save(args.screenshot)
            app.quit()
        QTimer.singleShot(700, capture)
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
