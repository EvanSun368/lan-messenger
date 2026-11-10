import pathlib,tempfile,time,unittest,socket
from lanlink.core import Node,send,receive

def wait_for(predicate,seconds=3):
 end=time.time()+seconds
 while time.time()<end:
  if predicate():return
  time.sleep(.02)
 raise AssertionError('condition timed out')

class MessagingTest(unittest.TestCase):
 def test_group_roundtrip_and_dedup(self):
  with tempfile.TemporaryDirectory() as d:
   nodes=[Node(n,pathlib.Path(d)/n,lambda *x:None,discovery_port=0) for n in 'ABC']
   a,b,c=nodes
   for n in nodes:n.start()
   try:
    a.add_peer('127.0.0.1',b.port);a.add_peer('127.0.0.1',c.port)
    gid=a.send_group([b.id,c.id],'群聊一','测试群');conv='group:'+gid
    wait_for(lambda:len(b.history(conv))==1 and len(c.history(conv))==1)
    b.send_group_message(gid,'群聊二')
    wait_for(lambda:len(a.history(conv))==2 and len(c.history(conv))==2)
    # Retry same message ID after lost ACK must not duplicate it.
    mid=a.history(conv)[0]['id']
    for _ in range(2):
     with a.connect(a.peers[c.id]) as s:
      send(s,dict(type='group_message',sender=a.hello(),id=mid,text='群聊一',group=a.groups()[0]))
      self.assertEqual(receive(s)['ack'],mid)
    self.assertEqual(len(c.history(conv)),2)
   finally:
    for n in nodes:n.stop()
 def test_persistent_offline_outbox(self):
  with tempfile.TemporaryDirectory() as d:
   root=pathlib.Path(d);a=Node('A',root/'a',lambda *x:None,discovery_port=0);b=Node('B',root/'b',lambda *x:None,discovery_port=0)
   a.start();b.start();a.add_peer('127.0.0.1',b.port)
   # Force deterministic offline without waiting for the listener to exit.
   original=a.connect;a.connect=lambda p:(_ for _ in ()).throw(OSError('offline'))
   mid=a.send_message(b.id,'重启后补发');self.assertEqual(len(a.pending_messages()),1)
   a.stop()
   a2=Node('A',root/'a',lambda *x:None,discovery_port=0)
   try:
    self.assertEqual(a2.id,a.id);self.assertIn(b.id,a2.peers);self.assertEqual(a2.pending_messages()[0]['id'],mid)
    a2.start();wait_for(lambda:len(b.history(a2.id))==1 and not a2.pending_messages())
    self.assertEqual(b.history(a2.id)[0]['text'],'重启后补发')
   finally:a2.stop();b.stop()
 def test_bad_group_and_text(self):
  with tempfile.TemporaryDirectory() as d:
   n=Node('A',d,lambda *x:None)
   with self.assertRaises(ValueError):n.create_group(['not-an-id'])
   with self.assertRaises(ValueError):n.queue_message(['a'*32],' ')
   n.stop()
