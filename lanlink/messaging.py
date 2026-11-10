"""Durable sender-side outbox and fixed-membership groups for trusted LANs.
No server stores messages: the sender must run again to deliver pending items.
"""
import json
import re
import threading
import time
import uuid

ID = re.compile(r'[0-9a-f]{32}')

class MessagingMixin:
    def init_messaging(self):
        self.flush_lock = threading.Lock()
        self.messaging_socket = None
        self.messaging_stopped = False
        self.retry_wakeup = threading.Event()
        with self.lock:
            self.db.executescript('''
            CREATE TABLE IF NOT EXISTS message_images(id TEXT PRIMARY KEY, path TEXT NOT NULL);
            CREATE TABLE IF NOT EXISTS message_authors(id TEXT PRIMARY KEY, sender TEXT NOT NULL);
            CREATE TABLE IF NOT EXISTS known_peers(id TEXT PRIMARY KEY, data TEXT NOT NULL);
            CREATE TABLE IF NOT EXISTS groups(id TEXT PRIMARY KEY, name TEXT NOT NULL, members TEXT NOT NULL);
            CREATE TABLE IF NOT EXISTS outbox(id TEXT, recipient TEXT, payload TEXT NOT NULL, attempts INTEGER DEFAULT 0,
                error TEXT DEFAULT '', PRIMARY KEY(id,recipient));
            ''')
            self.peers.update({r[0]:json.loads(r[1]) for r in self.db.execute('SELECT id,data FROM known_peers')})
            self.db.commit()

    def persist_peer(self, peer):
        with self.lock:
            self.db.execute('INSERT OR REPLACE INTO known_peers VALUES(?,?)',(peer['id'],json.dumps(peer)))
            self.db.commit()
        self.retry_wakeup.set()

    def start_messaging(self):
        def retry():
            while self.running:
                self.flush_outbox()
                self.retry_wakeup.wait(5)
                self.retry_wakeup.clear()
        self.messaging_thread=threading.Thread(target=retry,daemon=True)
        self.messaging_thread.start()

    def stop_messaging(self):
        self.messaging_stopped=True
        self.retry_wakeup.set()
        sock=self.messaging_socket
        if sock:
            try:
                import socket
                sock.shutdown(socket.SHUT_RDWR)
                sock.close()
            except OSError:pass
        thread=getattr(self,"messaging_thread",None)
        if thread and thread is not threading.current_thread():thread.join(6)

    def groups(self):
        with self.lock:
            return [dict(id=r[0],name=r[1],members=json.loads(r[2])) for r in self.db.execute('SELECT id,name,members FROM groups')]

    def history_authors(self,rows):
        with self.lock:
            for row in rows:
                author=self.db.execute('SELECT sender FROM message_authors WHERE id=?',(row['id'],)).fetchone()
                if author:row['sender_id']=author[0]
                image=self.db.execute('SELECT path FROM message_images WHERE id=?',(row['id'],)).fetchone()
                if image:row['image_path']=image[0]
        return rows

    def pending_messages(self):
        with self.lock:
            return [dict(id=r[0],recipient=r[1],attempts=r[2],error=r[3]) for r in self.db.execute('SELECT id,recipient,attempts,error FROM outbox')]

    def create_group(self, peer_ids, name='群聊'):
        ids=sorted(set(peer_ids)|{self.id})
        if not 2<=len(ids)<=64 or any(not ID.fullmatch(x) for x in ids):
            raise ValueError('Group requires 2–64 valid members')
        if any(x!=self.id and x not in self.peers for x in ids):
            raise ValueError('Unknown group member')
        name=str(name).strip()[:80] or '群聊'
        gid=uuid.uuid4().hex
        group=dict(id=gid,name=name,members=ids)
        with self.lock:
            self.db.execute('INSERT INTO groups VALUES(?,?,?)',(gid,name,json.dumps(ids)));self.db.commit()
        self.emit('group',**group)
        return gid

    def send_group(self, peer_ids, text, name='群聊'):
        members=sorted(set(peer_ids)|{self.id})
        existing=next((g for g in self.groups() if g['members']==members),None)
        gid=existing['id'] if existing else self.create_group(peer_ids,name)
        self.send_group_message(gid,text)
        return gid

    def send_group_message(self,gid,text):
        group=next((g for g in self.groups() if g['id']==gid),None)
        if group is None:raise ValueError('Unknown group')
        return self.queue_message([x for x in group['members'] if x!=self.id],text,group)

    def send_message(self,peer_id,text):
        if peer_id not in self.peers:raise ValueError('Unknown peer')
        return self.queue_message([peer_id],text)

    def send_image(self,peer_id,jpeg_bytes):
        from .image_payload import encode_image
        if peer_id not in self.peers:raise ValueError('Unknown peer')
        return self.queue_message([peer_id],'[截图]',image=encode_image(jpeg_bytes))

    def send_group_image(self,gid,jpeg_bytes):
        from .image_payload import encode_image
        group=next((g for g in self.groups() if g['id']==gid),None)
        if group is None:raise ValueError('Unknown group')
        return self.queue_message([x for x in group['members'] if x!=self.id],'[截图]',group,image=encode_image(jpeg_bytes))

    def queue_message(self,recipients,text,group=None,image=None):
        if not isinstance(text,str) or not text.strip() or len(text)>10000:
            raise ValueError('Message length must be 1–10000')
        mid=uuid.uuid4().hex
        payload=dict(type='group_message' if group else 'message',id=mid,text=text)
        image_path=None
        if image is not None:
            from .image_payload import save_image
            image_path=save_image(self.root,image);payload['image']=image
        if group:
            payload['group']=group
            # Addresses let known group members reply. All connections still enforce private LAN addresses.
            payload['members_info']=[p for ident,p in self.peers.items() if ident in group['members']]
        conversation='group:'+group['id'] if group else recipients[0]
        with self.lock:
            count=self.db.execute('SELECT count(*) FROM outbox').fetchone()[0]
            if count+len(recipients)>10000:raise ValueError('Outbox full')
            for recipient in recipients:
                self.db.execute('INSERT INTO outbox(id,recipient,payload) VALUES(?,?,?)',(mid,recipient,json.dumps(payload)))
            if image_path:self.db.execute('INSERT INTO message_images VALUES(?,?)',(mid,image_path))
            self.db.execute('INSERT INTO messages VALUES(?,?,?,?,?)',(mid,conversation,'out',text,time.time()))
            self.db.commit()
        self.emit('message',peer_id=conversation,group_id=group['id'] if group else None,text=text,direction='out',id=mid,status='queued',image_path=image_path)
        self.emit('queued',id=mid,peer_id=conversation,recipients=len(recipients))
        self.retry_wakeup.set()
        # Also supports callers that use the core without start(). Network failures stay queued.
        self.flush_outbox()
        return mid

    def flush_outbox(self):
        from .core import send,receive
        if not self.flush_lock.acquire(False):return
        try:
            with self.lock: rows=list(self.db.execute('SELECT id,recipient,payload FROM outbox ORDER BY attempts,id LIMIT 100'))
            for mid,recipient,serialized in rows:
                if self.messaging_stopped:break
                try:
                    peer=self.peers.get(recipient)
                    if peer is None:raise ValueError('Peer address unknown')
                    payload=json.loads(serialized);payload['sender']=self.hello()
                    with self.connect(peer) as sock:
                        self.messaging_socket=sock
                        sock.settimeout(5)
                        send(sock,payload)
                        if receive(sock).get('ack')!=mid:raise ValueError('Missing acknowledgement')
                    with self.lock:
                        self.db.execute('DELETE FROM outbox WHERE id=? AND recipient=?',(mid,recipient));self.db.commit()
                    self.emit('delivered',id=mid,recipient=recipient)
                except Exception as exc:
                    if self.messaging_stopped:break
                    with self.lock:
                        self.db.execute('UPDATE outbox SET attempts=attempts+1,error=? WHERE id=? AND recipient=?',(str(exc)[:200],mid,recipient));self.db.commit()
        finally:self.flush_lock.release()

    def receive_chat(self,s,r):
        from .core import send
        text=r.get('text');mid=r.get('id');sender=r.get('sender',{}).get('id')
        if not isinstance(text,str) or not text.strip() or len(text)>10000 or not isinstance(mid,str) or not ID.fullmatch(mid):
            raise ValueError('Invalid message')
        image_path=None
        if 'image' in r:
            from .image_payload import save_image
            image_path=save_image(self.root,r['image'])
            text='[截图]'
        conversation=sender; group=None
        if r['type']=='group_message':
            group=r.get('group',{}); members=group.get('members',[]);gid=group.get('id');name=group.get('name')
            if (not isinstance(gid,str) or not ID.fullmatch(gid) or not isinstance(name,str) or len(name)>80
                or not isinstance(members,list) or not 2<=len(members)<=64 or any(not isinstance(x,str) or not ID.fullmatch(x) for x in members)
                or len(set(members))!=len(members) or self.id not in members or sender not in members):
                raise ValueError('Invalid group')
            members=sorted(members);group=dict(id=gid,name=name,members=members)
            with self.lock:
                existing=self.db.execute('SELECT members FROM groups WHERE id=?',(gid,)).fetchone()
                if existing and json.loads(existing[0])!=members:raise ValueError('Group membership mismatch')
                self.db.execute('INSERT OR IGNORE INTO groups VALUES(?,?,?)',(gid,name,json.dumps(members)));self.db.commit()
            for p in r.get('members_info',[])[:64]:
                if isinstance(p,dict) and p.get('id') in members and p.get('id')!=self.id:
                    import ipaddress
                    try:
                        if ipaddress.ip_address(p.get('host','')).is_private:self.remember(p,p['host'])
                    except (ValueError,KeyError):pass
            conversation='group:'+gid
            self.emit('group',**group)
        if self.save(mid,conversation,'in',text):
            with self.lock:
                if image_path:self.db.execute('INSERT OR REPLACE INTO message_images VALUES(?,?)',(mid,image_path))
                self.db.execute('INSERT OR REPLACE INTO message_authors VALUES(?,?)',(mid,sender));self.db.commit()
            self.emit('message',peer_id=conversation,sender_id=sender,group_id=group['id'] if group else None,text=text,direction='in',id=mid,image_path=image_path)
        send(s,dict(ack=mid))
