import socket
import threading
import time
import unittest
from lanlink.screen_transport import ScreenService, MAX_JPEG
from lanlink.core import send, receive


class FakeNode:
    def __init__(self):
        self.events = []
        self.callback = None
        self.peers = {'peer': {}}
    def emit(self, kind, **data):
        self.events.append((kind, data))
        if self.callback:
            self.callback(kind, data)
    def hello(self):
        return {'id': 'a' * 32, 'name': 'Sender', 'port': 45872}


class ScreenTests(unittest.TestCase):
    def setUp(self):
        self.node = FakeNode()
        self.service = ScreenService(self.node)
        self.a, self.b = socket.socketpair()
        self.a.settimeout(2)
        self.b.settimeout(2)
    def tearDown(self):
        self.service.stop_all()
        self.a.close()
        self.b.close()
    def start_receiver(self, accepted):
        self.node.callback = lambda kind, data: self.service.decide(data['session'], accepted) if kind == 'screen_offer' else None
        self.worker = threading.Thread(target=self.service.handle, args=(self.b, {'sender': {'id': 'b'*32}}, '127.0.0.1'))
        self.worker.start()
    def test_reject_no_frames(self):
        self.start_receiver(False)
        self.assertFalse(receive(self.a)['accepted'])
        self.worker.join(2)
        self.assertFalse(self.worker.is_alive())
        self.assertFalse(any(k == 'screen_frame' for k, d in self.node.events))
    def test_accept_frame_and_stop(self):
        self.start_receiver(True)
        self.assertTrue(receive(self.a)['accepted'])
        frame = b'\xff\xd8synthetic-test-frame'
        self.a.sendall(len(frame).to_bytes(4, 'big') + frame)
        self.assertTrue(receive(self.a)['ok'])
        frames = [d for k, d in self.node.events if k == 'screen_frame']
        self.assertEqual(frames[0]['jpeg'], frame)
        self.service.stop_all()
        self.worker.join(2)
        self.assertFalse(self.worker.is_alive())
        self.assertIsNone(self.service.receiving)
    def test_oversize_frame_rejected(self):
        self.start_receiver(True)
        self.assertTrue(receive(self.a)['accepted'])
        self.a.sendall((MAX_JPEG + 1).to_bytes(4, 'big'))
        self.worker.join(2)
        self.assertFalse(self.worker.is_alive())
        self.assertFalse(any(k == 'screen_frame' for k, d in self.node.events))
    def test_sender_waits_for_approval(self):
        self.node.connect = lambda peer: self.a
        sid = self.service.start('peer')
        self.assertEqual(receive(self.b)['type'], 'screen_offer')
        self.assertFalse(any(k == 'screen_started' for k, d in self.node.events))
        send(self.b, {'accepted': True})
        deadline = time.time() + 2
        while not any(k == 'screen_started' for k, d in self.node.events) and time.time() < deadline:
            time.sleep(.01)
        self.service.publish(b'\xff\xd8frame')
        length = int.from_bytes(self.b.recv(4), 'big')
        self.assertEqual(self.b.recv(length), b'\xff\xd8frame')
        send(self.b, {'ok': True})
        self.service.stop(sid)
        deadline = time.time() + 2
        while not any(k == 'screen_stopped' for k, d in self.node.events) and time.time() < deadline:
            time.sleep(.01)
        self.assertIsNone(self.service.sending)

if __name__ == '__main__':
    unittest.main()
