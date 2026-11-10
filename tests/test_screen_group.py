import socket
import threading
import time
import unittest
from lanlink.screen_transport import ScreenService
from lanlink.core import send, receive, exact
from test_screen import FakeNode

class GroupScreenTests(unittest.TestCase):
    def test_independent_approval_and_stop(self):
        node = FakeNode()
        pairs = {p:socket.socketpair() for p in ('a','b','c')}
        node.peers = {p:{'id':p} for p in pairs}
        node.connect = lambda peer:pairs[peer['id']][0]
        service = ScreenService(node)
        for pair in pairs.values():
            for sock in pair:sock.settimeout(2)
        try:
            sessions = service.start_many(['a','b','c'])
            for pair in pairs.values():self.assertEqual(receive(pair[1])['type'],'screen_offer')
            self.assertFalse(any(k=='screen_started' for k,d in node.events))
            send(pairs['a'][1], {'accepted':True})
            send(pairs['b'][1], {'accepted':False})
            # C remains pending while A can receive frames.
            deadline=time.time()+2
            while not any(k=='screen_started' for k,d in node.events) and time.time()<deadline:time.sleep(.01)
            service.publish(b'\xff\xd8one')
            sock=pairs['a'][1]
            self.assertEqual(exact(sock,int.from_bytes(exact(sock,4),'big')),b'\xff\xd8one')
            send(sock,{'ok':True})
            send(pairs['c'][1], {'accepted':True})
            deadline=time.time()+2
            while len([1 for k,d in node.events if k=='screen_started'])<2 and time.time()<deadline:time.sleep(.01)
            sock=pairs['c'][1]
            self.assertEqual(exact(sock,int.from_bytes(exact(sock,4),'big')),b'\xff\xd8one')
            send(sock,{'ok':True})
            a=next(s for s,p in sessions.items() if p=='a')
            service.stop(a)
            service.publish(b'\xff\xd8two')
            self.assertEqual(exact(sock,int.from_bytes(exact(sock,4),'big')),b'\xff\xd8two')
            send(sock,{'ok':True})
            service.stop_all()
            self.assertFalse(service.senders)
        finally:
            service.stop_all()
            for pair in pairs.values():
                for sock in pair:sock.close()
