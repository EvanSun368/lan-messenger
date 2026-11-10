import tempfile,pathlib,unittest,hashlib,time,unittest.mock
from lanlink.core import Node
class Files(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.tmp.name)
  self.a=Node('a',self.root/'a',lambda *a:None,discovery_port=0);self.b=Node('b',self.root/'b',lambda *a:None,discovery_port=0)
  self.a.start();self.b.start();self.a.add_peer('127.0.0.1',self.b.port)
  self.dest=self.root/'dest';self.dest.mkdir();self.file=self.root/'sample.bin';self.file.write_bytes(bytes(range(256))*4096)
 def tearDown(self):self.a.stop();self.b.stop();self.tmp.cleanup()
 def test_resume_and_restart_state(self):
  tid=self.a.offer_file(self.b.id,self.file)
  partial=self.dest/('.lanlink-'+tid+'.part');partial.write_bytes(self.file.read_bytes()[:12345])
  self.b.init_files();self.a.init_files()
  self.b.accept_file(tid,self.dest)
  self.assertEqual((self.dest/self.file.name).read_bytes(),self.file.read_bytes());self.assertFalse(partial.exists())
 def test_changed_source_rejected(self):
  tid=self.a.offer_file(self.b.id,self.file);self.file.write_bytes(b'changed')
  with self.assertRaises(ValueError):self.b.accept_file(tid,self.dest)
  self.assertIn(tid,self.b.incoming);self.assertFalse((self.dest/self.file.name).exists())
 def test_partial_corruption(self):
  tid=self.a.offer_file(self.b.id,self.file);(self.dest/('.lanlink-'+tid+'.part')).write_bytes(b'wrong')
  with self.assertRaisesRegex(ValueError,'SHA256'):self.b.accept_file(tid,self.dest)
  self.b.accept_file(tid,self.dest);self.assertEqual((self.dest/self.file.name).read_bytes(),self.file.read_bytes())
 def test_readonly_share_token_traversal_links(self):
  share=self.root/'share';share.mkdir();(share/'visible.txt').write_text('yes');(share/'escape').symlink_to(self.file)
  token=self.a.publish_share(share)
  self.b.add_peer('127.0.0.1',self.a.port)
  self.assertEqual(self.b.list_share(self.a.id,token),[dict(name='visible.txt',directory=False,size=3)])
  for relative in ['../sample.bin','/etc/passwd','escape','C:\\evil','..\\sample.bin']:
   with self.assertRaises(ValueError):self.a.share_path(token,relative)
  with self.assertRaises(ValueError):self.b.list_share(self.a.id,'bad')
  self.b.download_share(self.a.id,token,'visible.txt',self.dest);self.assertEqual((self.dest/'visible.txt').read_text(),'yes')
  self.a.revoke_share(token)
  with self.assertRaises(ValueError):self.b.list_share(self.a.id,token)
 def test_existing_and_partial_symlink_rejected(self):
  tid=self.a.offer_file(self.b.id,self.file);(self.dest/self.file.name).write_text('keep')
  with self.assertRaises(FileExistsError):self.b.accept_file(tid,self.dest)
  self.assertEqual((self.dest/self.file.name).read_text(),'keep');(self.dest/self.file.name).unlink()
  (self.dest/('.lanlink-'+tid+'.part')).symlink_to(self.file)
  with self.assertRaises(ValueError):self.b.accept_file(tid,self.dest)
 def test_sender_real_restart_changed_port(self):
  tid=self.a.offer_file(self.b.id,self.file)
  partial=self.dest/('.lanlink-'+tid+'.part');partial.write_bytes(self.file.read_bytes()[:9876])
  oldport=self.a.port;oldid=self.a.id;self.a.stop()
  self.a=Node('a',self.root/'a',lambda *a:None,discovery_port=0);self.a.start()
  self.assertNotEqual(self.a.port,oldport);self.assertEqual(self.a.id,oldid)
  # Discovery (or manual add) refreshes the same identity's endpoint.
  self.b.add_peer('127.0.0.1',self.a.port)
  self.b.accept_file(tid,self.dest)
  self.assertEqual((self.dest/self.file.name).read_bytes(),self.file.read_bytes())
  self.assertNotIn(tid,self.a.outgoing)
 def test_revoke_invalidates_preissued_offer(self):
  from lanlink.core import send,receive
  token=self.a.publish_share(self.root);self.b.add_peer('127.0.0.1',self.a.port)
  with self.b.connect(self.b.peers[self.a.id]) as s:
   send(s,dict(type='share_get',token=token,path=self.file.name));offer=receive(s)['offer']
  self.b.register_offer(offer,notify=False);self.a.revoke_share(token)
  self.assertNotIn(offer['id'],self.a.outgoing)
  with self.assertRaisesRegex(ValueError,'unavailable'):self.b.accept_file(offer['id'],self.dest)
  self.assertFalse((self.dest/self.file.name).exists())
 def test_completed_rejected_expired_and_failed_offers_cleanup(self):
  self.file.write_bytes(b'x')
  for _ in range(70):
   tid=self.a.offer_file(self.b.id,self.file);self.b.accept_file(tid,self.dest);(self.dest/self.file.name).unlink()
  self.assertEqual(self.a.outgoing,{})
  tid=self.a.offer_file(self.b.id,self.file);self.b.reject_file(tid)
  deadline=time.monotonic()+2
  while tid in self.a.outgoing and time.monotonic()<deadline:time.sleep(.01)
  self.assertNotIn(tid,self.a.outgoing)
  offer=self.a.file_metadata(self.a.peers[self.b.id],self.file);self.a.outgoing[offer['id']]['created']=0
  self.a.prune_files();self.assertEqual(self.a.outgoing,{})
  self.b.stop()
  with self.assertRaises(OSError):self.a.offer_file(self.b.id,self.file)
  self.assertEqual(self.a.outgoing,{})

 def test_legacy_tasks_migrate_once_without_expiring(self):
  tid=self.a.offer_file(self.b.id,self.file)
  del self.a.outgoing[tid]['created'];self.a.persist_files()
  self.a.init_files();created=self.a.outgoing[tid]['created']
  self.assertGreater(created,time.time()-5)
  self.a.init_files();self.assertEqual(self.a.outgoing[tid]['created'],created)
  self.b.accept_file(tid,self.dest)
  self.assertEqual((self.dest/self.file.name).read_bytes(),self.file.read_bytes())
 def test_reject_does_not_wait_for_network(self):
  import threading
  tid=self.a.offer_file(self.b.id,self.file);started=threading.Event();release=threading.Event()
  def slow(item):started.set();release.wait(2)
  with unittest.mock.patch.object(self.b,'notify_release',slow):
   begin=time.monotonic();self.b.reject_file(tid);elapsed=time.monotonic()-begin
   try:
    self.assertLess(elapsed,.2);self.assertTrue(started.wait(1));self.assertNotIn(tid,self.b.incoming)
   finally:release.set()

 def test_legacy_unknown_share_capability_is_invalidated(self):
  tid=self.a.offer_file(self.b.id,self.file)
  del self.a.outgoing[tid]['share_token'];self.a.persist_files()
  self.a.init_files()
  self.assertNotIn(tid,self.a.outgoing)
  self.assertTrue(self.a.migration_warnings)
  with self.assertRaises(ValueError):self.b.accept_file(tid,self.dest)
  self.assertFalse((self.dest/self.file.name).exists())
  self.a.init_files()
  self.assertNotIn(tid,self.a.outgoing)
