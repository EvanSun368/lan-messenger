"""Opt-in, one-to-one JPEG screen preview over the existing LAN connection.

No recording, input injection, unattended access, encryption or authentication.
The receiver must approve every session. Both sides can close the connection.
"""
import socket
import threading
import time
import uuid
from .core import send, receive, exact

MAX_JPEG = 2 * 1024 * 1024


class ScreenService:
    def __init__(self, node):
        self.node = node
        self.lock = threading.RLock()
        self.pending = {}
        self.sockets = {}
        self.senders = {}
        self.receiving = None
        self.frame = None
        self.sequence = 0
        self.changed = threading.Condition(self.lock)

    def emit(self, kind, **data):
        self.node.emit(kind, **data)

    def decide(self, session, accepted):
        with self.lock:
            offer = self.pending.get(session)
            if offer:
                offer['accepted'] = bool(accepted)
                offer['event'].set()

    def publish(self, jpeg):
        if not isinstance(jpeg, bytes) or not 0 < len(jpeg) <= MAX_JPEG:
            raise ValueError('Screen frame is too large')
        with self.changed:
            if self.sending:
                self.frame = jpeg
                self.sequence += 1
                self.changed.notify_all()

    @property
    def sending(self):
        # Backwards-compatible single-session indicator.
        return next(iter(self.senders), None)

    def start(self, peer_id):
        return next(iter(self.start_many([peer_id])))

    def start_many(self, peer_ids):
        peers = list(dict.fromkeys(peer_ids))
        if not peers or len(peers) > 63:
            raise ValueError('Select 1–63 recipients')
        if any(peer not in self.node.peers for peer in peers):
            raise ValueError('Recipient address unavailable')
        with self.lock:
            if self.senders:
                raise ValueError('Already sharing a screen')
            sessions = {uuid.uuid4().hex: peer for peer in peers}
            self.senders.update(sessions)
            self.frame = None
            self.sequence = 0
        for session, peer in sessions.items():
            threading.Thread(target=self._send, args=(session, peer), daemon=True).start()
        return sessions

    def _send(self, session, peer_id):
        try:
            with self.node.connect(self.node.peers[peer_id]) as sock:
                with self.lock:
                    if session not in self.senders:
                        return
                    self.sockets[session] = sock
                sock.settimeout(35)
                send(sock, {'type': 'screen_offer', 'sender': self.node.hello(), 'session': session})
                if not receive(sock).get('accepted'):
                    self.emit('screen_status', session=session, message='对方拒绝或未确认投屏')
                    return
                sock.settimeout(5)
                self.emit('screen_started', session=session, direction='out', peer_id=peer_id)
                last = -1
                while True:
                    with self.changed:
                        self.changed.wait_for(lambda: session not in self.senders or (self.frame is not None and self.sequence != last), timeout=5)
                        if session not in self.senders:
                            break
                        jpeg = self.frame
                        last = self.sequence
                    if jpeg is None:
                        raise TimeoutError('Screen capture did not produce a frame')
                    sock.sendall(len(jpeg).to_bytes(4, 'big') + jpeg)
                    # Backpressure and stop detection, no unbounded frame queue.
                    if not receive(sock).get('ok'):
                        break
        except Exception as exc:
            self.emit('screen_status', session=session, message=str(exc))
        finally:
            with self.lock:
                self.senders.pop(session, None)
                if not self.senders:
                    self.frame = None
                self.sockets.pop(session, None)
            self.emit('screen_stopped', session=session, direction='out')

    def handle(self, sock, request, host):
        # A fresh local id prevents a remote sender selecting/colliding session ids.
        session = uuid.uuid4().hex
        offer = {'event': threading.Event(), 'accepted': False}
        with self.lock:
            if self.receiving or self.pending:
                send(sock, {'accepted': False})
                return
            self.pending[session] = offer
            self.sockets[session] = sock
        try:
            self.emit('screen_offer', session=session, peer_id=request['sender']['id'], host=host)
            offer['event'].wait(30)
            with self.lock:
                approved = offer['accepted'] and session in self.sockets
                self.pending.pop(session, None)
                if approved:
                    self.receiving = session
            send(sock, {'accepted': approved})
            if not approved:
                return
            sock.settimeout(10)
            self.emit('screen_started', session=session, direction='in', peer_id=request['sender']['id'])
            while True:
                with self.lock:
                    if self.receiving != session:
                        break
                size = int.from_bytes(exact(sock, 4), 'big')
                if not 0 < size <= MAX_JPEG:
                    raise ValueError('Invalid screen frame size')
                frame = exact(sock, size)
                if not frame.startswith(b'\xff\xd8'):
                    raise ValueError('Expected JPEG screen frame')
                self.emit('screen_frame', session=session, jpeg=frame)
                time.sleep(0.2)  # Bound even untrusted senders to at most 5 fps.
                send(sock, {'ok': True})
        except (EOFError, OSError):
            pass
        except Exception as exc:
            self.emit('screen_status', session=session, message=str(exc))
        finally:
            with self.lock:
                self.pending.pop(session, None)
                self.sockets.pop(session, None)
                if self.receiving == session:
                    self.receiving = None
            self.emit('screen_stopped', session=session, direction='in')

    def stop(self, session):
        with self.changed:
            offer = self.pending.get(session)
            if offer:
                offer['accepted'] = False
                offer['event'].set()
            self.senders.pop(session, None)
            if not self.senders:
                self.frame = None
            if self.receiving == session:
                self.receiving = None
            sock = self.sockets.pop(session, None)
            self.changed.notify_all()
        if sock:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            sock.close()

    def stop_all(self):
        with self.lock:
            sessions = set(self.sockets) | set(self.pending)
            sessions.update(self.senders)
        for session in sessions:
            self.stop(session)
