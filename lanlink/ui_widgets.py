"""Original desktop widgets; no third-party branding or artwork."""
from PySide6.QtCore import Qt, QSize, QTimer, QRectF, Signal
from PySide6.QtGui import QIcon, QPixmap, QPainter, QColor, QPen, QFont
from PySide6.QtWidgets import QWidget, QScrollArea, QVBoxLayout, QHBoxLayout, QLabel, QPushButton, QMessageBox

def nav_icon(kind, color="#6d7890"):
    pix = QPixmap(28, 28)
    pix.fill(Qt.transparent)
    p = QPainter(pix)
    p.setRenderHint(QPainter.Antialiasing)
    p.setPen(QPen(QColor(color), 1.8))
    if kind == "chat":
        p.drawRoundedRect(QRectF(4, 4, 20, 16), 5, 5)
        p.drawLine(8, 20, 6, 24); p.drawLine(6, 24, 13, 20)
        p.drawPoint(10, 12); p.drawPoint(14, 12); p.drawPoint(18, 12)
    elif kind == "files":
        p.drawRoundedRect(QRectF(4, 8, 20, 15), 2, 2)
        p.drawLine(4, 8, 4, 5); p.drawLine(4, 5, 12, 5); p.drawLine(12, 5, 15, 8)
    elif kind == "screen":
        p.drawRoundedRect(QRectF(3, 4, 22, 15), 2, 2)
        p.drawLine(14, 19, 14, 24); p.drawLine(9, 24, 19, 24)
    elif kind == "share":
        for x,y in [(6,14),(21,6),(21,22)]:p.drawEllipse(QRectF(x-3,y-3,6,6))
        p.drawLine(9,12,18,7);p.drawLine(9,16,18,21)
    else:
        p.drawEllipse(QRectF(4,4,20,20));p.drawPoint(14,9);p.drawLine(14,13,14,20)
    p.end()
    return QIcon(pix)

class ChatView(QScrollArea):
    preview_requested = Signal(str)
    def __init__(self):
        super().__init__()
        self.setWidgetResizable(True)
        self.setObjectName("chatView")
        self.body = QWidget()
        self.body.setObjectName("chatBody")
        self.layout = QVBoxLayout(self.body)
        self.layout.setContentsMargins(10, 14, 10, 14)
        self.layout.setSpacing(14)
        self.layout.addStretch()
        self.setWidget(self.body)
        self.texts = []
        self.bubbles = []
        self.image_bubbles = []
        self.chat_family="Microsoft YaHei UI"
        self.chat_size=11
        self.placeholder = QLabel("选择左侧设备，开始聊天")
        self.placeholder.setAlignment(Qt.AlignCenter)
        self.placeholder.setStyleSheet("color:#9aa5b5; padding:30px;")
        self.layout.insertWidget(0, self.placeholder)

    def setPlaceholderText(self, text):
        self.placeholder.setText(text)

    def clear(self):
        self.texts.clear()
        self.bubbles.clear()
        self.image_bubbles.clear()
        while self.layout.count() > 1:
            item = self.layout.takeAt(0)
            if item.widget():
                item.widget().hide()
                item.widget().deleteLater()
        self.placeholder = QLabel("暂无消息")
        self.placeholder.setAlignment(Qt.AlignCenter)
        self.placeholder.setStyleSheet("color:#9aa5b5; padding:30px;")
        self.layout.insertWidget(0, self.placeholder)

    def toPlainText(self):
        return "\n".join(self.texts)

    def add_message(self, name, text, mine, image_path=None):
        self.placeholder.hide()
        self.texts.append(text)
        row = QWidget()
        line = QHBoxLayout(row)
        line.setContentsMargins(0,0,0,0)
        line.setSpacing(10)
        avatar = QLabel((name or "?")[:1])
        avatar.setTextFormat(Qt.PlainText)
        avatar.setAlignment(Qt.AlignCenter)
        avatar.setFixedSize(36,36)
        avatar.setStyleSheet("background:" + ("#d9ecff" if mine else "#e2e8ff") + ";color:#5272ac;border-radius:11px;font-weight:600;")
        content = QWidget()
        column = QVBoxLayout(content)
        column.setContentsMargins(0,0,0,0)
        column.setSpacing(5)
        who = QLabel(name)
        who.setTextFormat(Qt.PlainText)
        who.setStyleSheet("color:"+("#005ea8" if mine else "#72a411")+";font-size:12px;")
        who.setAlignment(Qt.AlignRight if mine else Qt.AlignLeft)
        bubble = QLabel(text)
        bubble.setTextFormat(Qt.PlainText)
        bubble.setTextInteractionFlags(Qt.TextSelectableByMouse)
        bubble.setWordWrap(True)
        bubble.setMaximumWidth(450)
        bubble.setStyleSheet("padding:4px 5px;border-radius:0;background:transparent;color:" + ("#005ea8;" if mine else "#151515;"))
        column.addWidget(who)
        column.addWidget(bubble)
        from .link_preview import extract_links
        from urllib.parse import urlsplit
        for url in extract_links(text)[:3]:
            try:
                host=urlsplit(url).hostname
            except ValueError:
                continue  # Keep the original plain text; omit an invalid preview action.
            if not host:
                continue
            link=QPushButton("预览链接 · " + host[:60])
            link.setToolTip("点击才访问该网站加载标题与简介："+url)
            link.setStyleSheet("text-align:left;color:#1684d8;background:transparent;border:0;padding:3px 0;font-size:11px;")
            link.clicked.connect(lambda checked=False,value=url:self.preview_requested.emit(value))
            column.addWidget(link)
        self.bubbles.append((bubble,text))
        if image_path:
            pix=QPixmap(str(image_path))
            if not pix.isNull():
                bubble.setText("")
                bubble.setWordWrap(False)
                self.image_bubbles.append((bubble,pix))
        self.apply_font(self.chat_family,self.chat_size)
        QTimer.singleShot(0,self.resize_bubbles)
        if mine:line.addStretch()
        avatar.hide()
        avatar.setParent(row)
        line.addWidget(content,0,Qt.AlignTop)

        if not mine:line.addStretch()
        self.layout.insertWidget(self.layout.count()-1,row)
        QTimer.singleShot(0, lambda:self.verticalScrollBar().setValue(self.verticalScrollBar().maximum()))

    def resize_bubbles(self):
        limit=min(450,max(120,self.viewport().width()-140))
        for bubble,text in self.bubbles:
            if any(image is bubble for image,pix in self.image_bubbles):continue
            width=max((bubble.fontMetrics().horizontalAdvance(line) for line in text.splitlines()),default=60)+58
            bubble.setFixedWidth(min(limit,max(88,width)))
        for bubble,pix in self.image_bubbles:
            shown=pix.scaled(min(350,limit-30),260,Qt.KeepAspectRatio,Qt.SmoothTransformation)
            bubble.setPixmap(shown)
            bubble.setFixedWidth(shown.width()+30)
            bubble.setFixedHeight(shown.height()+22)

    def apply_font(self,family,size):
        self.chat_family=family
        self.chat_size=size
        for bubble,text in self.bubbles:
            # Keep the original bubble colours and apply only a typography suffix.
            base=bubble.property("baseStyle")
            if base is None:
                base=bubble.styleSheet()
                bubble.setProperty("baseStyle",base)
            escaped=family.replace('"','').replace(';','')
            bubble.setFont(QFont(family,size))
            bubble.setStyleSheet(base+f'font-family:"{escaped}";font-size:{size}pt;')
        self.resize_bubbles()

    def resizeEvent(self,event):
        super().resizeEvent(event)
        if hasattr(self,"bubbles"):
            self.resize_bubbles()

from PySide6.QtWidgets import QStyledItemDelegate, QStyle
from PySide6.QtGui import QFont
class PeerDelegate(QStyledItemDelegate):
    def sizeHint(self, option, index):
        return QSize(220,54)
    def paint(self,p,option,index):
        p.save();p.setRenderHint(QPainter.Antialiasing)
        rect=option.rect.adjusted(0,0,0,0)
        selected=bool(option.state & QStyle.State_Selected)
        if selected:
            p.setPen(Qt.NoPen);p.setBrush(QColor('#e2f2ff'));p.drawRect(rect)
        text=index.data() or '';parts=text.split('\n',1);name=parts[0]
        avatar=QRectF(rect.left()+10,rect.top()+9,36,36)
        p.setPen(Qt.NoPen);p.setBrush(QColor('#dbe9fb'));p.drawRect(avatar)
        p.setPen(QColor('#5377ad'));font=QFont(option.font);font.setPixelSize(17);font.setBold(True);p.setFont(font)
        p.drawText(avatar,Qt.AlignCenter,name[:1])
        x=rect.left()+56;width=rect.width()-66
        font.setPixelSize(14);font.setBold(False);p.setFont(font);p.setPen(QColor('#27364a'))
        p.drawText(x,rect.top()+25,p.fontMetrics().elidedText(name,Qt.ElideRight,width))
        font.setPixelSize(11);p.setFont(font);p.setPen(QColor('#9aa4b4'))
        subtitle=parts[1] if len(parts)>1 else '讨论组 · 点击查看消息'
        p.drawText(x,rect.top()+43,p.fontMetrics().elidedText(subtitle,Qt.ElideRight,width));p.restore()

def plain_question(parent,title,text):
    dialog=QMessageBox(parent)
    dialog.setWindowTitle(title)
    dialog.setIcon(QMessageBox.Question)
    dialog.setTextFormat(Qt.PlainText)
    dialog.setText(text)
    dialog.setStandardButtons(QMessageBox.Yes|QMessageBox.No)
    dialog.setDefaultButton(QMessageBox.No)
    return dialog.exec()

class BlueTitleBar(QWidget):
    def __init__(self,window,title,close_callback=None):
        super().__init__(window)
        self.window=window
        self.drag=None
        self.setObjectName('blueTitleBar')
        self.setAttribute(Qt.WA_StyledBackground,True)
        self.setFixedHeight(32)
        layout=QHBoxLayout(self);layout.setContentsMargins(9,0,6,0)
        self.title=QLabel(title);self.title.setTextFormat(Qt.PlainText);self.title.setStyleSheet('color:white;font-size:13px;')
        layout.addWidget(self.title);layout.addStretch()
        small=QPushButton('−');small.setFixedSize(28,26);small.clicked.connect(window.showMinimized)
        close=QPushButton('×');close.setFixedSize(28,26);close.clicked.connect(close_callback or window.close)
        for b in (small,close):b.setStyleSheet('color:white;background:transparent;border:0;font-size:19px;padding:0;');layout.addWidget(b)
    def mousePressEvent(self,event):
        if event.button()==Qt.LeftButton:self.drag=event.globalPosition().toPoint()-self.window.frameGeometry().topLeft()
    def mouseMoveEvent(self,event):
        if self.drag is not None and event.buttons()&Qt.LeftButton:self.window.move(event.globalPosition().toPoint()-self.drag)
    def mouseReleaseEvent(self,event):self.drag=None
