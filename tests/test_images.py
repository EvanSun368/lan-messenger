import unittest,tempfile,pathlib,time
from PySide6.QtGui import QImage,QColor
from PySide6.QtCore import QBuffer,QIODevice
from lanlink.core import Node
from lanlink.image_payload import encode_image,decode_image,save_image

def jpeg(w=20,h=20):
 im=QImage(w,h,QImage.Format.Format_RGB32);im.fill(QColor('blue'));b=QBuffer();b.open(QIODevice.OpenModeFlag.WriteOnly);im.save(b,'JPEG');return bytes(b.data())

class ImageTests(unittest.TestCase):
 def test_validation(self):
  p=encode_image(jpeg());self.assertTrue(decode_image(p));p['sha256']='0'*64
  with self.assertRaises(ValueError):decode_image(p)
  with self.assertRaises(ValueError):encode_image(jpeg(1601,20))
  with self.assertRaises(ValueError):encode_image(b'not jpeg')
 def test_symlink(self):
  with tempfile.TemporaryDirectory() as d:
   r=pathlib.Path(d);(r/'elsewhere').mkdir();(r/'images').symlink_to(r/'elsewhere',target_is_directory=True)
   with self.assertRaises(ValueError):save_image(r,encode_image(jpeg()))
 def test_online_group_offline(self):
  with tempfile.TemporaryDirectory() as d:
   r=pathlib.Path(d);a=Node('A',r/'a',lambda *x:None,discovery_port=0);b=Node('B',r/'b',lambda *x:None,discovery_port=0)
   a.start();b.start()
   try:
    a.add_peer('127.0.0.1',b.port);a.send_image(b.id,jpeg());a.flush_outbox()
    for _ in range(40):
     if b.history(a.id):break
     time.sleep(.05)
    self.assertTrue(pathlib.Path(b.history(a.id)[0]['image_path']).exists())
    gid=a.create_group([b.id]);a.send_group_image(gid,jpeg());a.flush_outbox()
    for _ in range(40):
     if b.history('group:'+gid):break
     time.sleep(.05)
    self.assertTrue(b.history('group:'+gid)[0]['image_path'])
    a.peers[b.id]['port']=1;a.send_image(b.id,jpeg());self.assertTrue(a.pending_messages())
    a.peers[b.id]['port']=b.port;a.flush_outbox();self.assertFalse(a.pending_messages())
   finally:a.stop();b.stop()
if __name__=='__main__':unittest.main()
