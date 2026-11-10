"""Explicit, local screenshot selection. No background capture or clipboard reads."""
from PySide6.QtCore import Qt, Signal, QRect, QPoint, QByteArray, QBuffer, QIODevice
from PySide6.QtGui import QPainter, QColor, QPen, QPixmap
from PySide6.QtWidgets import QWidget

MAX_IMAGE_BYTES = 512 * 1024

def encode_screenshot(pixmap):
    image = pixmap.toImage()
    image.setDevicePixelRatio(1)
    image = image.scaled(1280, 960, Qt.KeepAspectRatio, Qt.SmoothTransformation)
    for quality in (85, 70, 55, 40):
        payload=QByteArray()
        stream=QBuffer(payload)
        stream.open(QIODevice.WriteOnly)
        if image.save(stream, 'JPEG', quality) and len(payload)<=MAX_IMAGE_BYTES:
            return bytes(payload)
    raise ValueError('截图内容过大，请缩小选择范围后重试')

class ScreenshotSelection(QWidget):
    finished = Signal(object)
    def __init__(self,pixmap,geometry):
        super().__init__(None,Qt.FramelessWindowHint|Qt.WindowStaysOnTopHint)
        self.source=pixmap
        self.setGeometry(geometry)
        self.setCursor(Qt.CrossCursor)
        self.start=None
        self.selection=QRect()
        self.completed=False
        self.setMouseTracking(True)

    def crop(self,rect):
        rect=rect.normalized().intersected(self.rect())
        if rect.width()<5 or rect.height()<5:return None
        sx=self.source.width()/self.width()
        sy=self.source.height()/self.height()
        return self.source.copy(QRect(int(rect.x()*sx),int(rect.y()*sy),int(rect.width()*sx),int(rect.height()*sy)))

    def paintEvent(self,event):
        p=QPainter(self)
        p.drawPixmap(self.rect(),self.source)
        p.fillRect(self.rect(),QColor(0,0,0,95))
        if not self.selection.isNull():
            p.save();p.setClipRect(self.selection);p.drawPixmap(self.rect(),self.source);p.restore()
            p.setPen(QPen(QColor('#008cff'),2));p.drawRect(self.selection)
        p.fillRect(16,16,min(560,self.width()-32),40,QColor(20,30,45,220))
        p.setPen(Qt.white)
        p.drawText(QRect(28,16,self.width()-56,40),Qt.AlignVCenter,'拖动选择截图范围 · Enter 选择整屏 · Esc 取消')

    def mousePressEvent(self,event):
        if event.button()==Qt.LeftButton:
            self.start=event.position().toPoint()
            self.selection=QRect(self.start,self.start)
            self.update()

    def mouseMoveEvent(self,event):
        if self.start is not None:
            self.selection=QRect(self.start,event.position().toPoint()).normalized().intersected(self.rect())
            self.update()

    def mouseReleaseEvent(self,event):
        if event.button()==Qt.LeftButton and self.start is not None:
            result=self.crop(QRect(self.start,event.position().toPoint()))
            self.start=None
            if result is not None:self.finish(result)

    def keyPressEvent(self,event):
        if event.key()==Qt.Key_Escape:self.finish(None)
        elif event.key() in (Qt.Key_Enter,Qt.Key_Return):self.finish(self.source)

    def finish(self,result):
        if self.completed:return
        self.completed=True
        self.hide()
        self.finished.emit(result)
        self.deleteLater()

    def closeEvent(self,event):
        if not self.completed:self.finish(None)
        event.accept()
