import json
import pathlib
import tempfile
import unittest
from lanlink.core import Node
from lanlink.profiles import validate_profile

class ProfileTests(unittest.TestCase):
    def test_edit_persist_and_fetch(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            a = Node('A', root/'a', lambda *args: None, discovery_port=0)
            b = Node('B', root/'b', lambda *args: None, discovery_port=0)
            a.start(); b.start()
            try:
                b.update_profile(dict(nickname='测试昵称', signature='<b>不是HTML</b>', organization='公司', department='研发', location='南京'))
                a.add_peer('127.0.0.1', b.port)
                self.assertEqual(a.peers[b.id]['name'], '测试昵称')
                self.assertNotIn('profile', b.hello())
                profile = a.fetch_profile(b.id)
                self.assertEqual(profile['department'], '研发')
                self.assertEqual(profile['signature'], '<b>不是HTML</b>')
            finally:
                a.stop(); b.stop()
            b2 = Node('忽略旧昵称', root/'b', lambda *args: None, discovery_port=0)
            self.assertEqual(b2.name, '测试昵称')
            self.assertEqual(b2.public_profile()['location'], '南京')
            b2.stop()

    def test_validation_and_corrupt_file(self):
        for data in ({'nickname': ''}, {'nickname': 'a'*81}, {'nickname': ['wrong']}, {'nickname':'x\n'}, {'nickname':'a','signature':'x'*201}):
            with self.assertRaises(ValueError): validate_profile(data)
        with tempfile.TemporaryDirectory() as d:
            pathlib.Path(d,'profile.json').write_text('{bad')
            n=Node('fallback',d,lambda *a: None)
            self.assertEqual(n.public_profile()['nickname'],'fallback')
            n.stop()

    def test_read_only_plain_text_ui(self):
        from PySide6.QtWidgets import QApplication
        from lanlink.profile_ui import ProfileDialog
        app=QApplication.instance() or QApplication([])
        dialog=ProfileDialog({'nickname':'<img src=x>','signature':'<b>text</b>'})
        self.assertTrue(dialog.fields['nickname'].isReadOnly())
        self.assertEqual(dialog.fields['signature'].text(),'<b>text</b>')
        dialog.close()
