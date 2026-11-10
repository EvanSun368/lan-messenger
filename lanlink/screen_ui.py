"""Qt integration for explicit, visible one-to-one screen sharing."""
from PySide6.QtCore import QObject, QTimer, Qt, QByteArray, QBuffer, QIODevice
from PySide6.QtGui import QGuiApplication, QImageReader, QPixmap, QAction, QKeySequence
from PySide6.QtWidgets import QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QComboBox, QMessageBox, QDialog
from .screen_transport import ScreenService
from .ui_widgets import plain_question


class ScreenController(QObject):
    def __init__(self, window):
        super().__init__(window)
        self.window = window
        self.service = ScreenService(window.node)
        window.node.screen_service = self.service
        self.outgoing = {}
        self.incoming = None
        self.active_senders = set()
        self.capturing = False
        self.dialog = QDialog(window)
        self.dialog.setWindowTitle("会话投屏")
        self.dialog.resize(760, 520)
        self.dialog.finished.connect(self.stop)
        self.page = self.dialog
        self.target_ids = []
        self.target_name = ""
        layout = QVBoxLayout(self.page)
        info = QLabel('单聊 / 群聊屏幕预览 · 最多 5 帧/秒 · 不传声音、不支持远程控制\n仅在可信局域网使用，画面未加密。投屏前请关闭含敏感内容的窗口。')
        info.setWordWrap(True)
        layout.addWidget(info)
        row = QHBoxLayout()
        self.screens = QComboBox()
        for number, screen in enumerate(QGuiApplication.screens()):
            self.screens.addItem(f'屏幕 {number + 1} · {screen.name()}', screen.name())
        row.addWidget(self.screens)
        self.start_button = QPushButton('投屏给当前设备')
        self.start_button.clicked.connect(self.start)
        row.addWidget(self.start_button)
        self.stop_button = QPushButton('停止投屏（Ctrl+Shift+S）')
        self.stop_button.clicked.connect(self.stop)
        self.stop_button.setStyleSheet('QPushButton { background: #c62828; color: white; font-weight: bold; } QPushButton:disabled { background: #e2e8f0; color: #94a3b8; }')
        self.stop_button.setEnabled(False)
        row.addWidget(self.stop_button)
        layout.addLayout(row)
        self.status = QLabel('未投屏。接收方需要确认每次连接。')
        self.status.setTextFormat(Qt.PlainText)
        layout.addWidget(self.status)
        self.preview = QLabel('收到的画面将在这里显示')
        self.preview.setAlignment(Qt.AlignCenter)
        self.preview.setMinimumSize(320, 180)
        layout.addWidget(self.preview, 1)
        
        self.timer = QTimer(self)
        self.timer.setInterval(200)
        self.timer.timeout.connect(self.capture)
        window.events.event.connect(self.on_event)
        action = QAction(window)
        action.setShortcut(QKeySequence('Ctrl+Shift+S'))
        action.triggered.connect(self.stop)
        window.addAction(action)
        # Visible outside the screen tab while sharing.
        self.banner = QLabel('')
        self.banner.setStyleSheet('color: #b71c1c; font-weight: bold;')
        window.statusBar().addPermanentWidget(self.banner)

    def open_for_current(self):
        if not self.outgoing:
            current = self.window.current
            if not current:
                self.status.setText("请先选择单聊或讨论组")
                self.dialog.show()
                return
            if current.startswith('group:'):
                group = self.window.groups.get(current, {})
                self.target_ids = [p for p in group.get('members', []) if p != self.window.node.id]
                self.target_name = group.get('name', '讨论组')
            else:
                self.target_ids = [current] if current else []
                self.target_name = self.window.peers.get(current, {}).get('name', '当前会话')
            self.dialog.setWindowTitle('投屏 · ' + self.target_name)
            self.start_button.setText('向当前会话投屏')
        self.dialog.show()
        self.dialog.raise_()

    def start(self):
        if self.window.demo or self.outgoing:
            return
        if not self.target_ids:
            self.open_for_current()
        peers = [p for p in self.target_ids if p in self.window.node.peers]
        skipped = len(self.target_ids) - len(peers)
        if not peers:
            self.status.setText('没有可连接的会话成员')
            return
        result = plain_question(self.window, '确认分享屏幕',
            f'向 {self.target_name} 的 {len(peers)} 位成员分享所选屏幕？\n'
            f'{skipped} 位成员没有可用地址，将跳过。每位接收人单独确认。\n'
            '画面未加密，请先关闭敏感窗口。Ctrl+Shift+S 可停止全部投屏。')
        if result != QMessageBox.Yes:
            return
        try:
            self.outgoing = self.service.start_many(peers)
            self.start_button.setEnabled(False)
            self.screens.setEnabled(False)
            self.stop_button.setEnabled(True)
            self.status.setText('等待各接收人确认…')
            self.banner.setText('正在请求投屏 · Ctrl+Shift+S 取消')
        except Exception as exc:
            self.status.setText(str(exc))

    def capture(self):
        if not self.capturing or not self.outgoing:
            return
        screens = QGuiApplication.screens()
        selected = self.screens.currentIndex()
        if selected < 0 or selected >= len(screens):
            self.status.setText('所选屏幕已不可用')
            self.stop()
            return
        pixmap = screens[selected].grabWindow(0)
        if pixmap.isNull():
            self.status.setText('无法采集屏幕。请检查系统屏幕录制权限；部分 Wayland 环境不支持此方式。')
            self.stop()
            return
        pixmap = pixmap.scaled(1280, 720, Qt.KeepAspectRatio, Qt.SmoothTransformation)
        payload = QByteArray()
        buf = QBuffer(payload)
        buf.open(QIODevice.WriteOnly)
        if not pixmap.save(buf, 'JPEG', 65):
            self.stop()
            return
        self.service.publish(bytes(payload))

    def on_event(self, kind, data):
        if not kind.startswith('screen_'):
            return
        session = data.get('session')
        if kind == 'screen_offer':
            name = self.window.peers.get(data['peer_id'], {}).get('name', '未知设备')
            answer = plain_question(self.window, '接收投屏', f'{name}（{data["host"]}）希望向你投屏。\n身份未经验证，仅接受可信设备。是否接收？')
            self.service.decide(session, answer == QMessageBox.Yes)
        elif kind == 'screen_started':
            if data['direction'] == 'out':
                if session not in self.outgoing:
                    self.service.stop(session)
                    return
                self.active_senders.add(session)
                self.capturing = True
                self.timer.start()
                self.status.setText(f'正在分享给 {len(self.active_senders)} 位成员 · 低帧率预览')
                self.banner.setText('● 正在分享屏幕 · Ctrl+Shift+S 停止')
                self.capture()
            else:
                self.incoming = session
                self.status.setText('正在接收屏幕画面')
                self.stop_button.setEnabled(True)
                self.dialog.show()
                self.dialog.raise_()
        elif kind == 'screen_frame' and session == self.incoming:
            payload = QByteArray(data['jpeg'])
            buf = QBuffer(payload)
            buf.open(QIODevice.ReadOnly)
            reader = QImageReader(buf, b'JPEG')
            size = reader.size()
            if not size.isValid() or size.width() > 1920 or size.height() > 1080:
                self.status.setText('拒绝异常尺寸的投屏画面')
                self.service.stop(session)
                return
            image = reader.read()
            if image.isNull():
                self.service.stop(session)
                return
            self.preview.setPixmap(QPixmap.fromImage(image).scaled(self.preview.size(), Qt.KeepAspectRatio, Qt.SmoothTransformation))
        elif kind == 'screen_status':
            self.status.setText(data.get('message', '投屏中断'))
        elif kind == 'screen_stopped':
            if session in self.outgoing:
                self.outgoing.pop(session, None)
                self.active_senders.discard(session)
                if not self.active_senders:
                    self.timer.stop()
                    self.capturing = False
                if not self.outgoing:
                    self.start_button.setEnabled(True)
                    self.screens.setEnabled(True)
                    self.banner.setText('')
                else:
                    self.status.setText(f'正在分享给 {len(self.active_senders)} 位成员；其余等待确认')
            if session == self.incoming:
                self.incoming = None
                self.preview.clear()
                self.preview.setText('投屏已结束，画面已清除')
            self.stop_button.setEnabled(bool(self.outgoing or self.incoming))

    def stop(self):
        self.timer.stop()
        self.capturing = False
        self.service.stop_all()
        self.preview.clear()
        self.preview.setText('投屏已停止，画面已清除')
        self.status.setText('投屏已停止')
