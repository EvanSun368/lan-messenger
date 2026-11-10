"""Capture the real Qt UI with explicitly labelled demo data; no desktop capture."""
import tempfile
from pathlib import Path
from PySide6.QtWidgets import QApplication
from PySide6.QtCore import QTimer,QEventLoop
from lanlink.app import Window,STYLE
app=QApplication([]);app.setStyle('Fusion');app.setStyleSheet(STYLE)
Path('screenshots').mkdir(exist_ok=True)
with tempfile.TemporaryDirectory() as directory:
    window=Window(directory,port=45872,demo=True)
    window.show();window.open_conversation()
    loop=QEventLoop();QTimer.singleShot(200,loop.quit);loop.exec()
    window.grab().save('preview.png')
    window.chat_dialog.grab().save('screenshots/feige-chat.png')
    window.toggle_history();app.processEvents()
    window.chat_dialog.grab().save('screenshots/feige-history.png')
    window.toggle_history()
    for index,name in [(1,'preview-transfers.png'),(3,'preview-sharing.png')]:
        window.navigate(index);app.processEvents();window.tools_dialog.grab().save(name);window.tools_dialog.hide()
    window.screen.open_for_current();app.processEvents();window.screen.dialog.grab().save('preview-screen.png');window.screen.dialog.close()
    def capture_modal(name):
        dialog=app.activeModalWidget()
        if dialog:dialog.grab().save(name);dialog.reject()
    for fn,name in [(window.configure,'screenshots/feige-settings.png'),(window.choose_emoji,'screenshots/emoji-panel.png'),(window.configure_font,'screenshots/font-settings.png')]:
        QTimer.singleShot(150,lambda name=name:capture_modal(name));fn()
    window.profile_controller.view_current_profile();app.processEvents()
    dialog=window.profile_controller.open_dialogs[-1];dialog.grab().save('screenshots/contact-profile.png');dialog.close()
    window.close()
