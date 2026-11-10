import tempfile,time,pathlib,unittest
from lanlink.core import Node
class CoreTest(unittest.TestCase):
 def test_chat_and_file(self):
  with tempfile.TemporaryDirectory() as d:
   root=pathlib.Path(d);events=[]
   a=Node('A',root/'a',lambda *x:None,discovery_port=0);b=Node('B',root/'b',lambda *x:events.append(x),discovery_port=0)
   a.start();b.start()
   try:
    a.add_peer('127.0.0.1',b.port);a.send_message(b.id,'你好')
    from test_messaging import wait_for
    wait_for(lambda:len(b.history(a.id))==1)
    self.assertEqual(b.history(a.id)[0]['text'],'你好')
    p=root/'test.bin';p.write_bytes(b'abc'*100000);tid=a.offer_file(b.id,p)
    target=root/'received';target.mkdir();b.accept_file(tid,target);self.assertEqual((target/'test.bin').read_bytes(),p.read_bytes())
    self.assertTrue(any(k=='complete' for k,v in events))
   finally:a.stop();b.stop()
 def test_public_rejected(self):
  with tempfile.TemporaryDirectory() as d:
   a=Node('A',d,lambda *x:None)
   try:
    with self.assertRaises(ValueError):a.add_peer('8.8.8.8',1234)
   finally:a.stop()
 def test_history_latest_and_bad_id(self):
  with tempfile.TemporaryDirectory() as d:
   a=Node('A',d,lambda *x:None)
   try:
    for i in range(5):a.save(str(i),'p','in',str(i))
    self.assertEqual([v['text'] for v in a.history('p',2)],['3','4'])
    with self.assertRaises(ValueError):a.accept_file('../../bad',d)
   finally:a.stop()
 def test_stop_releases_sqlite_and_is_idempotent(self):
  import sqlite3
  with tempfile.TemporaryDirectory() as d:
   n=Node('A',d,lambda *x:None);n.stop();n.stop()
   with self.assertRaises(sqlite3.ProgrammingError):n.db.execute('SELECT 1')
   pathlib.Path(d,'history.sqlite').unlink()
if __name__=='__main__':unittest.main()
