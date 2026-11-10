"""Independent LAN prototype. Trusted networks only: transport is not encrypted."""
import socket, threading, json, uuid, time, sqlite3, pathlib, hashlib, os, re, ipaddress
MAX_FRAME=1048576

def send(s,obj):
    data=json.dumps(obj).encode(); s.sendall(len(data).to_bytes(4,'big')+data)
def exact(s,n):
    b=bytearray()
    while len(b)<n:
        v=s.recv(min(65536,n-len(b)))
        if not v: raise EOFError('Connection closed')
        b.extend(v)
    return bytes(b)
def receive(s):
    n=int.from_bytes(exact(s,4),'big')
    if n>MAX_FRAME: raise ValueError('Frame too large')
    return json.loads(exact(s,n))

from .file_services import FileServices
from .messaging import MessagingMixin
from .profiles import ProfileMixin

class Node(ProfileMixin,MessagingMixin,FileServices):
    def __init__(self,name,storage_dir,callback,port=0,discovery_port=45871):
        self.name=name[:80]; self.root=pathlib.Path(storage_dir); self.root.mkdir(parents=True,exist_ok=True)
        p=self.root/'identity'; self.id=p.read_text() if p.exists() else uuid.uuid4().hex; p.write_text(self.id)
        self.callback=callback; self.port=port; self.discovery_port=discovery_port
        self.peers={}; self.incoming={}; self.outgoing={}; self.cancelled=set(); self.running=False
        self.slots=threading.BoundedSemaphore(16); self.lock=threading.RLock(); self.db=sqlite3.connect(self.root/'history.sqlite',check_same_thread=False)
        self.db.execute('CREATE TABLE IF NOT EXISTS messages(id TEXT PRIMARY KEY, peer TEXT, direction TEXT, text TEXT, time REAL)'); self.db.commit()
        self.init_profiles()
        self.init_messaging()
        self.init_files()
        self.active_sockets=set()
    def emit(self,kind,**data):
        try: 
            if kind=='file_offer': kind='offer'; data['id']=data['transfer_id']
            if kind in ('progress','complete','offered'): data['id']=data.get('transfer_id'); data.setdefault('status',kind)
            if kind=='message':data['timestamp']=time.time()
            self.callback(kind,data)
        except Exception: pass
    def history(self,peer_id,limit=200):
        with self.lock: return self.history_authors([dict(zip(('id','peer','direction','text','timestamp'),r)) for r in self.db.execute('SELECT * FROM messages WHERE peer=? ORDER BY time DESC LIMIT ?',(peer_id,limit))][::-1])
    def save(self,mid,peer,direction,text):
        with self.lock:
            c=self.db.execute('INSERT OR IGNORE INTO messages VALUES(?,?,?,?,?)',(mid,peer,direction,text,time.time())); self.db.commit(); return c.rowcount>0
    def hello(self): return dict(id=self.id,name=self.name,port=self.port)
    def remember(self,p,host):
        if not isinstance(p.get('id'),str) or not re.fullmatch(r'[0-9a-f]{32}',p['id']):raise ValueError('Invalid peer id')
        if p['id']==self.id:return
        if p['id'] not in self.peers and len(self.peers)>=256:raise ValueError('Peer limit reached')
        if not isinstance(p['port'],int) or not 0<p['port']<65536:raise ValueError('Bad port')
        self.peers[p['id']]=dict(id=p['id'],name=str(p['name'])[:80],host=host,port=p['port']); self.persist_peer(self.peers[p['id']]); self.emit('peer',**self.peers[p['id']])
    def start(self):
        self.server=socket.socket(); self.server.bind(('',self.port)); self.port=self.server.getsockname()[1]; self.server.listen(16); self.server.settimeout(1); self.running=True
        threading.Thread(target=self.listen,daemon=True).start()
        self.start_messaging()
        try:
            self.udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); self.udp.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1); self.udp.setsockopt(socket.SOL_SOCKET,socket.SO_BROADCAST,1); self.udp.bind(('',self.discovery_port)); self.udp.settimeout(1)
            threading.Thread(target=self.discover,daemon=True).start()
        except OSError as e:self.emit('error',message=str(e))
    def discover(self):
        last=0
        while self.running:
            try:
                if time.time()-last>5:self.udp.sendto(json.dumps(dict(protocol='lanlink1',**self.hello())).encode(),('255.255.255.255',self.discovery_port));last=time.time()
                data,addr=self.udp.recvfrom(4096); p=json.loads(data)
                if p.get('protocol')=='lanlink1' and ipaddress.ip_address(addr[0]).is_private:self.remember(p,addr[0])
            except socket.timeout:pass
            except Exception:pass
    def listen(self):
        while self.running:
            try:
                s,a=self.server.accept()
                if not ipaddress.ip_address(a[0]).is_private or not self.slots.acquire(False):s.close();continue
                with self.lock:self.active_sockets.add(s)
                threading.Thread(target=self.handle,args=(s,a[0]),daemon=True).start()
            except socket.timeout:pass
            except OSError:break
    def connect(self,p):
        import ipaddress
        address=socket.gethostbyname(p['host'])
        if not ipaddress.ip_address(address).is_private:raise ValueError('Trusted LAN addresses only')
        s=socket.create_connection((address,p['port']),5);s.settimeout(30)
        with self.lock:
            self.active_sockets={v for v in self.active_sockets if v.fileno()!=-1};self.active_sockets.add(s)
        return s
    def add_peer(self,host,port):
        with self.connect(dict(host=host,port=port)) as s:
            send(s,dict(type='hello',sender=self.hello())); p=receive(s);self.remember(p,host)
    def cancel_transfer(self,transfer_id):self.cancelled.add(transfer_id)
    def handle(self,s,host):
        try:
            s.settimeout(30);r=receive(s);t=r['type']
            if 'sender'in r:self.remember(r['sender'],host)
            if t=='hello':send(s,self.hello())
            elif t=='profile_get':self.serve_profile(s)
            elif t=='screen_offer' and hasattr(self,'screen_service'):
                self.screen_service.handle(s,r,host)
            elif t in ('message','group_message'):
                self.receive_chat(s,r)
            elif t=='offer':
                self.register_offer(r);send(s,dict(ok=True))
            elif t=='file_release':self.serve_release(s,r,host)
            elif t=='fetch':self.serve_fetch(s,r,host)
            elif t in ('share_list','share_get'):self.serve_share(s,r,host)
        except Exception as e:
            if 'r' in locals() and r.get('type')=='fetch' and 'p' in locals() and p is not None:self.emit('progress',transfer_id=r['id'],name=p.name,done=locals().get('done',0),total=locals().get('total',0),status='failed',message=str(e))
            self.emit('error',message=str(e))
        finally:
            s.close()
            with self.lock:self.active_sockets.discard(s)
            self.slots.release()
    def stop(self):
        self.running=False
        with self.lock:
            sockets=tuple(self.active_sockets);self.active_sockets.clear()
        for s in sockets:
            try:s.shutdown(socket.SHUT_RDWR)
            except OSError:pass
            s.close()
        self.stop_messaging()
        if hasattr(self,'server'):self.server.close()
        if hasattr(self,'udp'):self.udp.close()
