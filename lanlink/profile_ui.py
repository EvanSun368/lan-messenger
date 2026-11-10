"""Plain-text profile dialogs. All listed fields are public to reachable LAN clients."""
from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QDialog, QVBoxLayout, QFormLayout, QLineEdit,
    QLabel, QDialogButtonBox, QMessageBox)
from .profiles import FIELDS, validate_profile

LABELS = {'nickname': '昵称', 'signature': '个性签名', 'organization': '组织 / 公司',
          'department': '部门', 'location': '所在地'}

class ProfileDialog(QDialog):
    def __init__(self, profile, parent=None, editable=False, peer_id=''):
        super().__init__(parent)
        self.setWindowTitle('编辑个人资料' if editable else '联系人资料')
        self.setMinimumWidth(410)
        self.editable = editable
        layout = QVBoxLayout(self)
        notice = QLabel('保存后，昵称会广播给局域网联系人；其他资料可由可达的局域网客户端查看。请勿填写敏感信息。' if editable else '对方自行填写的资料，未经身份认证。')
        notice.setWordWrap(True)
        notice.setTextFormat(Qt.PlainText)
        layout.addWidget(notice)
        form = QFormLayout()
        self.fields = {}
        for key, limit in FIELDS.items():
            field = QLineEdit(str(profile.get(key, '')))
            field.setMaxLength(limit)
            field.setReadOnly(not editable)
            field.setPlaceholderText('必填' if key == 'nickname' else '选填')
            self.fields[key] = field
            form.addRow(LABELS[key], field)
        layout.addLayout(form)
        if peer_id:
            identity = QLabel('设备 ID：' + peer_id)
            identity.setTextFormat(Qt.PlainText)
            identity.setTextInteractionFlags(Qt.TextSelectableByMouse)
            layout.addWidget(identity)
        buttons = QDialogButtonBox(QDialogButtonBox.Save | QDialogButtonBox.Cancel if editable else QDialogButtonBox.Close)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def value(self):
        return validate_profile({key: field.text() for key, field in self.fields.items()})

    def accept(self):
        if self.editable:
            try:
                self.value()
            except ValueError as exc:
                QMessageBox.warning(self, '资料未保存', str(exc))
                return
        super().accept()

class ProfileController:
    def __init__(self, window):
        self.window = window
        self.open_dialogs = []
        window.events.event.connect(self.on_event)

    def edit_my_profile(self):
        window = self.window
        dialog = ProfileDialog(window.node.public_profile(), window, editable=True)
        if dialog.exec() == QDialog.Accepted:
            try:
                profile = window.node.update_profile(dialog.value())
                window.settings.setValue('name', profile['nickname'])
                window.statusBar().showMessage('个人资料已保存。昵称将在下一次局域网发现时更新。', 8000)
            except (OSError, ValueError) as exc:
                QMessageBox.warning(window, '保存失败', str(exc))

    def view_current_profile(self):
        window = self.window
        peer_id = window.current
        if not peer_id or peer_id.startswith('group:'):
            QMessageBox.information(window, '联系人资料', '请先选择一位联系人。')
            return
        if window.demo:
            profile = dict.fromkeys(FIELDS, '')
            profile.update(nickname=window.peers[peer_id]['name'], signature='演示资料，并非真实联系人')
            self.on_event('profile_received', dict(peer_id=peer_id, profile=profile))
            return
        window.statusBar().showMessage('正在读取联系人公开资料…', 5000)
        window.background(window.node.fetch_profile, peer_id)

    def on_event(self, kind, data):
        if kind != 'profile_received':
            return
        dialog = ProfileDialog(data['profile'], self.window, peer_id=data['peer_id'])
        dialog.setAttribute(Qt.WA_DeleteOnClose)
        self.open_dialogs.append(dialog)
        dialog.finished.connect(lambda _: self.open_dialogs.remove(dialog) if dialog in self.open_dialogs else None)
        dialog.show()
