import os
os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
import tempfile
import unittest
from PySide6.QtWidgets import QApplication
from lanlink.app import Window

class UiTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])

    def test_selection_message_progress_and_close(self):
        with tempfile.TemporaryDirectory() as root:
            window = Window(root, demo=True)
            window.show()
            self.app.processEvents()
            self.assertEqual(window.contacts.count(), 2)
            window.editor.setPlainText('<b>not markup</b>')
            window.send_message()
            self.assertIn('<b>not markup</b>', window.messages.toPlainText())
            self.assertEqual(window.editor.toPlainText(), '')
            window.on_event('progress', {'id':'test','name':'test.bin','done':42,'total':100,'status':'failed'})
            row = window.rows['test']
            self.assertEqual(window.transfers.item(row, 1).text(), '42%')
            self.assertEqual(window.transfers.item(row, 2).text(), '失败')
            self.assertFalse(window.transfers.cellWidget(row, 3).isEnabled())
            window.close()

    def test_group_and_share_navigation(self):
        with tempfile.TemporaryDirectory() as root:
            window = Window(root, demo=True)
            window.on_event('group', {'id': 'a'*32, 'name':'测试讨论组', 'members':['one','two']})
            window.contacts.setCurrentRow(2)
            self.assertEqual(window.current, 'group:'+'a'*32)
            self.assertIn('测试讨论组', window.conversation_title.text())
            window.send_file()
            self.assertIn('单台设备', window.statusBar().currentMessage())
            window.contacts.setCurrentRow(0)
            window.on_event('share_list', {'peer_id': 'demo1', 'token':'token','path':'', 'entries':[{'name':'test.bin','directory':False,'size':32}]})
            self.assertEqual(window.share_entries.count(), 1)
            window.contacts.setCurrentRow(1)
            self.assertEqual(window.share_entries.count(), 0)
            self.assertIsNone(window.share_peer)
            window.close()

    def test_tool_navigation_search_and_resizing(self):
        with tempfile.TemporaryDirectory() as root:
            window=Window(root,demo=True)
            window.show()
            self.app.processEvents()
            for index in [1,3,0,0]:
                window.nav_buttons[index].click()
                self.assertEqual(window.tabs.currentIndex(),index)
                self.assertTrue(window.nav_buttons[index].isChecked())
            window.search.setText('测试')
            self.assertTrue(window.contacts.item(0).isHidden())
            self.assertFalse(window.contacts.item(1).isHidden())
            window.search.clear()
            self.assertFalse(window.contacts.item(0).isHidden())
            window.resize(820,560)
            self.app.processEvents()
            window.messages.resize_bubbles()
            for bubble,text in window.messages.bubbles:
                self.assertLessEqual(bubble.width(),window.messages.viewport().width())
            window.close()

    def test_emoji_font_and_screenshot_preview_data(self):
        from PySide6.QtCore import QSettings,QRect
        from PySide6.QtGui import QPixmap,QColor
        from lanlink.chat_tools import ScreenshotSelection,encode_screenshot
        with tempfile.TemporaryDirectory() as root:
            window=Window(root,demo=True)
            window.settings=QSettings(root+'/settings.ini',QSettings.IniFormat)
            window.insert_emoji('😀')
            self.assertEqual(window.editor.toPlainText(),'😀')
            window.set_chat_font('Sans Serif',16)
            self.assertEqual(int(window.settings.value('chat_font_size')),16)
            self.assertEqual(window.messages.chat_size,16)
            image=QPixmap(320,200);image.fill(QColor('#4488cc'))
            selection=ScreenshotSelection(image,QRect(0,0,320,200))
            crop=selection.crop(QRect(20,20,80,60))
            self.assertEqual(crop.size().width(),80)
            payload=encode_screenshot(crop)
            window.send_screenshot_payload('demo1',payload)
            self.assertEqual(len(window.messages.image_bubbles),1)
            self.assertTrue(window.messages.image_bubbles[0][1].width()>0)
            self.assertGreater(window.messages.image_bubbles[0][0].height(),22)
            selection.finish(None)
            window.close()

    def test_screen_dialog_target_and_cancel_selection(self):
        from PySide6.QtCore import QRect
        from PySide6.QtGui import QPixmap
        from lanlink.chat_tools import ScreenshotSelection
        with tempfile.TemporaryDirectory() as root:
            window=Window(root,demo=True)
            self.assertEqual(window.tabs.count(),4)
            window.screen.open_for_current()
            self.assertIn('研发',window.screen.dialog.windowTitle())
            window.screen.dialog.close()
            window.current=None
            window.screen.open_for_current()
            self.assertIn('选择',window.screen.status.text())
            results=[]
            selection=ScreenshotSelection(QPixmap(100,100),QRect(0,0,100,100))
            selection.finished.connect(results.append)
            selection.finish(None)
            self.assertEqual(results,[None])
            window.screen.dialog.close()
            window.close()

    def test_link_preview_is_click_only_and_bounded(self):
        from PySide6.QtWidgets import QPushButton
        with tempfile.TemporaryDirectory() as root:
            window=Window(root,demo=True)
            calls=[]
            window.background=lambda fn,*args:calls.append(fn)
            window.append_message({'direction':'in','text':'https://example.com'})
            self.assertFalse(calls)
            buttons=[b for b in window.messages.findChildren(QPushButton) if b.text().startswith('预览链接')]
            self.assertEqual(len(buttons),1)
            buttons[0].click()
            self.assertEqual(len(calls),1)
            buttons[0].click()
            self.assertEqual(len(calls),1)
            window.load_link_preview('https://example.org')
            window.load_link_preview('https://example.net')
            self.assertEqual(len(calls),2)
            window.close()

    def test_malformed_link_keeps_text_without_preview(self):
        from PySide6.QtWidgets import QPushButton
        with tempfile.TemporaryDirectory() as root:
            window=Window(root,demo=True)
            window.append_message({'direction':'in','text':'损坏链接 https://[broken'})
            self.assertIn('https://[broken',window.messages.toPlainText())
            self.assertFalse([b for b in window.messages.findChildren(QPushButton) if b.text().startswith('预览链接')])
            window.close()

    def test_untrusted_names_are_plain_text(self):
        from PySide6.QtCore import Qt
        from PySide6.QtWidgets import QLabel
        with tempfile.TemporaryDirectory() as root:
            window=Window(root,demo=True)
            window.on_event('peer',{'id':'demo1','name':'<b>admin</b>','host':'127.0.0.1','port':1})
            window.contacts.setCurrentRow(1)
            window.contacts.setCurrentRow(0)
            window.append_message({'direction':'in','peer_id':'demo1','text':'hello'})
            names=[x for x in window.messages.findChildren(QLabel) if x.text()=='<b>admin</b>']
            self.assertTrue(names)
            self.assertTrue(all(x.textFormat()==Qt.PlainText for x in names))
            self.assertEqual(window.conversation_title.textFormat(),Qt.PlainText)
            self.assertEqual(window.share_target.textFormat(),Qt.PlainText)
            window.close()

    def test_feige_roster_and_separate_conversation(self):
        with tempfile.TemporaryDirectory() as root:
            window=Window(root,demo=True)
            window.show()
            self.assertLess(window.width(),500)
            self.assertFalse(window.chat_dialog.isVisible())
            window.open_conversation()
            self.assertTrue(window.chat_dialog.isVisible())
            self.assertEqual(window.roster_tabs.count(),3)
            window.on_event('group',{'id':'b'*32,'name':'测试组','members':['demo1','demo2']})
            window.roster_tabs.setCurrentIndex(2)
            self.assertTrue(window.contacts.item(0).isHidden())
            self.assertFalse(window.contacts.item(2).isHidden())
            window.toggle_history()
            self.assertTrue(window.history_panel.isVisible())
            window.chat_dialog.hide()
            self.assertTrue(window.isVisible())
            window.close()

    def test_switch_conversation_keeps_separate_drafts(self):
        with tempfile.TemporaryDirectory() as root:
            window=Window(root,demo=True)
            window.editor.setPlainText('给研发的草稿')
            window.contacts.setCurrentRow(1)
            self.assertEqual(window.editor.toPlainText(),'')
            window.editor.setPlainText('给测试的草稿')
            window.contacts.setCurrentRow(0)
            self.assertEqual(window.editor.toPlainText(),'给研发的草稿')
            window.close()

if __name__ == '__main__':
    unittest.main()
