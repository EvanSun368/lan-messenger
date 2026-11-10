"""Resumable files and explicitly published, token-protected read-only shares.
Trusted LAN only: capabilities and traffic are not encrypted.
"""
import pathlib, hashlib, json, uuid, re, socket, threading, os, time

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''): h.update(b)
    return h.hexdigest()

def safe_file_name(name):
    if not isinstance(name,str) or not name or len(name)>200 or name in ('.','..') or any(ord(c)<32 or c in '/\\:<>"|?*' for c in name): raise ValueError('Invalid filename')
    if name.endswith((' ','.')) or name.split('.')[0].upper() in {'CON','PRN','AUX','NUL',*[f'COM{i}' for i in range(1,10)],*[f'LPT{i}' for i in range(1,10)]}: raise ValueError('Reserved filename')
    return name

class FileServices:
    def init_files(self):
        self.file_lock=threading.RLock(); self.active_receives=set(); self.shares={};self.file_sockets={};self.migration_warnings=[]
        p=self.root/'transfers.json'
        if p.exists():
            try:
                data=json.loads(p.read_text()); self.incoming=data.get('incoming',{}); self.outgoing=data.get('outgoing',{})
            except (ValueError,OSError): pass
        self.prune_files()
    def prune_files(self):
        with self.file_lock:
            now=time.time()
            # Old previews did not distinguish ordinary offers from shared ones.
            # Never revive an untraceable shared-file capability after restart.
            legacy=[key for key,value in self.outgoing.items() if "share_token" not in value]
            if legacy:
                for key in legacy:self.outgoing.pop(key,None)
                self.migration_warnings.append(f"已停用 {len(legacy)} 个旧版发送任务：无法确认是否来自共享目录，请重新发起传输。")
            # Migrate pre-TTL previews once; persist the timestamp below.
            for entry in self.outgoing.values():entry.setdefault('created',now)
            self.outgoing={k:v for k,v in self.outgoing.items() if now-v.get('created',0)<7*86400 and (not v.get('share_token') or v['share_token'] in self.shares)}
            self.persist_files()
    def release_transfer(self,transfer_id):
        with self.file_lock:
            self.cancelled.add(transfer_id);self.outgoing.pop(transfer_id,None);self.persist_files()
            sock=self.file_sockets.get(transfer_id)
        if sock:
            try:sock.shutdown(socket.SHUT_RDWR)
            except OSError:pass
    def serve_release(self,s,r,host):
        from .core import send
        entry=self.outgoing.get(r.get('id'))
        if entry and entry['host']==host:self.release_transfer(r['id'])
        send(s,dict(ok=True))
    def notify_release(self,item):
        from .core import send,receive
        peer=self.peers.get(item['sender']['id'],item['peer'])
        try:
            with self.connect(peer) as s:send(s,dict(type='file_release',id=item['id']));receive(s)
        except (OSError,ValueError,EOFError):pass  # 7-day expiry covers lost acknowledgments.
    def persist_files(self):
        with self.file_lock:
            tmp=self.root/'transfers.tmp';tmp.write_text(json.dumps(dict(incoming=self.incoming,outgoing=self.outgoing)));tmp.replace(self.root/'transfers.json')
    def file_metadata(self,peer,path,share_token=None):
        self.prune_files()
        p=pathlib.Path(path).resolve(strict=True); size=p.stat().st_size
        if not p.is_file() or size>2**34:raise ValueError('File required; maximum 16 GiB')
        safe_file_name(p.name); tid=uuid.uuid4().hex
        r=dict(type='offer',sender=self.hello(),id=tid,name=p.name,size=size,sha256=digest(p))
        with self.file_lock:
            if share_token and share_token not in self.shares:raise ValueError('Share revoked')
            if len(self.outgoing)>=64:raise ValueError('Too many pending offers')
            self.outgoing[tid]=dict(path=str(p),host=socket.gethostbyname(peer['host']),size=size,sha256=r['sha256'],created=time.time(),share_token=share_token);self.persist_files()
        return r
    def offer_file(self,peer_id,path):
        from .core import send,receive
        r=self.file_metadata(self.peers[peer_id],path)
        try:
            with self.connect(self.peers[peer_id]) as s:send(s,r); reply=receive(s)
            if not reply.get('ok'):raise ValueError('Offer rejected')
        except Exception:
            self.release_transfer(r['id']);raise
        self.emit('offered',transfer_id=r['id'],name=r['name']);return r['id']
    def register_offer(self,r,notify=True):
        safe_file_name(r.get('name'))
        if not re.fullmatch(r'[0-9a-f]{32}',str(r.get('id',''))):raise ValueError('Invalid transfer id')
        if not re.fullmatch(r'[0-9a-f]{64}',str(r.get('sha256',''))):raise ValueError('Invalid digest')
        if type(r.get('size')) is not int or not 0<=r['size']<=2**34:raise ValueError('Invalid size')
        with self.file_lock:
            if len(self.incoming)>=64 or r['id'] in self.incoming:raise ValueError('Too many or duplicate offers')
            r['peer']=self.peers[r['sender']['id']].copy();self.incoming[r['id']]=r;self.persist_files()
        if notify:self.emit('file_offer',transfer_id=r['id'],name=r['name'],size=r['size'],peer_id=r['sender']['id'])
    def accept_file(self,transfer_id,save_dir):
        from .core import send,receive,exact
        if not re.fullmatch(r'[0-9a-f]{32}',transfer_id):raise ValueError('Invalid transfer id')
        with self.file_lock:
            if transfer_id in self.active_receives:raise ValueError('Transfer already active')
            item=self.incoming[transfer_id];self.active_receives.add(transfer_id)
        done=0
        try:
            directory=pathlib.Path(save_dir).resolve(strict=True)
            if not directory.is_dir():raise ValueError('Destination folder required')
            dest=directory/safe_file_name(item['name']);tmp=directory/('.lanlink-'+transfer_id+'.part')
            if dest.exists() or dest.is_symlink():raise FileExistsError('Destination exists')
            if tmp.is_symlink():raise ValueError('Unsafe partial file')
            self.cancelled.discard(transfer_id)
            # Refuse symlink race on platforms that expose O_NOFOLLOW.
            fd=os.open(str(tmp),os.O_RDWR|os.O_CREAT|getattr(os,'O_NOFOLLOW',0),0o600)
            with os.fdopen(fd,'r+b') as f:
                done=f.seek(0,2)
                if done>item['size']:raise ValueError('Partial file exceeds expected size')
                peer=self.peers.get(item['sender']['id'],item['peer'])
                with self.connect(peer) as s:
                    send(s,dict(type='fetch',id=transfer_id,offset=done));r=receive(s)
                    if not r.get('ok'):raise ValueError(r.get('error','Offer unavailable'))
                    while done<item['size']:
                        if transfer_id in self.cancelled:raise ValueError('Paused')
                        data=exact(s,min(65536,item['size']-done));f.write(data);done+=len(data);self.emit('progress',transfer_id=transfer_id,name=item['name'],done=done,total=item['size'])
                    f.flush();os.fsync(f.fileno())
            if digest(tmp)!=item['sha256']:
                tmp.unlink();raise ValueError('SHA256 mismatch; retry starts from zero')
            # Same-directory hard link publishes the verified file atomically and
            # fails if destination exists. Unsupported filesystems retain .part.
            os.link(tmp,dest,follow_symlinks=False)
            tmp.unlink()
            with self.file_lock:self.incoming.pop(transfer_id,None);self.persist_files()
            self.notify_release(item)
            self.emit('complete',transfer_id=transfer_id,name=item['name'],done=done,total=item['size'],path=str(dest))
        except Exception as e:
            self.emit('progress',transfer_id=transfer_id,name=item['name'],done=done,total=item['size'],status='paused',message=str(e));raise
        finally:
            with self.file_lock:self.active_receives.discard(transfer_id)
    def reject_file(self,transfer_id):
        with self.file_lock:item=self.incoming.pop(transfer_id,None);self.persist_files()
        if item:
            threading.Thread(target=self.notify_release,args=(item,),daemon=True).start()
    def serve_fetch(self,s,r,host):
        from .core import send
        entry=self.outgoing.get(r.get('id'))
        if not entry or entry['host']!=host or (entry.get('share_token') and entry['share_token'] not in self.shares):send(s,dict(ok=False,error='Offer unavailable'));return
        p=pathlib.Path(entry['path']);offset=r.get('offset',0)
        if type(offset)is not int or not 0<=offset<=entry['size']:raise ValueError('Invalid offset')
        if not p.is_file() or p.stat().st_size!=entry['size'] or digest(p)!=entry['sha256']:send(s,dict(ok=False,error='Source changed'));return
        done=offset
        with self.file_lock:self.file_sockets[r['id']]=s
        try:
            done=offset
            send(s,dict(ok=True))
            self.cancelled.discard(r['id'])
            with p.open('rb') as f:
                f.seek(offset)
                for chunk in iter(lambda:f.read(65536),b''):
                    if r['id'] in self.cancelled or r['id'] not in self.outgoing:raise ValueError('Paused or revoked')
                    s.sendall(chunk);done+=len(chunk);self.emit('progress',transfer_id=r['id'],name=p.name,done=done,total=entry['size'])
            self.emit('complete',transfer_id=r['id'],name=p.name,done=done,total=entry['size'],status='sent_unverified')
        except Exception as exc:
            self.emit('progress',transfer_id=r['id'],name=p.name,done=done,total=entry['size'],status='paused',message=str(exc))
            raise
        finally:
            with self.file_lock:self.file_sockets.pop(r['id'],None)
    def publish_share(self,path):
        p=pathlib.Path(path).resolve(strict=True)
        if not p.is_dir():raise ValueError('Folder required')
        token=uuid.uuid4().hex+uuid.uuid4().hex;self.shares[token]=p;return token
    def revoke_share(self,token):
        with self.file_lock:
            self.shares.pop(token,None)
            tids=[k for k,v in self.outgoing.items() if v.get('share_token')==token]
        for tid in tids:self.release_transfer(tid)
    def share_path(self,token,relative=''):
        if token not in self.shares:raise ValueError('Share unavailable')
        root=self.shares[token]
        if not isinstance(relative,str) or '\\' in relative or ':' in relative or '\0' in relative:raise ValueError('Invalid path')
        rel=pathlib.PurePosixPath(relative)
        if rel.is_absolute() or '..' in rel.parts:raise ValueError('Invalid path')
        p=root
        for part in rel.parts:
            p=p/part
            if p.is_symlink() or (hasattr(p,'is_junction') and p.is_junction()):raise ValueError('Links not shared')
        p=p.resolve(strict=True)
        if not p.is_relative_to(root):raise ValueError('Path escaped share')
        return p
    def serve_share(self,s,r,host):
        from .core import send
        try:
            p=self.share_path(r.get('token'),r.get('path',''))
            if r['type']=='share_list':
                if not p.is_dir():raise ValueError('Folder required')
                items=[]
                for f in sorted(p.iterdir(),key=lambda p:p.name.lower()):
                    if f.is_symlink() or (hasattr(f,'is_junction') and f.is_junction()):continue
                    if not(f.is_dir() or f.is_file()):continue
                    items.append(dict(name=f.name,directory=f.is_dir(),size=f.stat().st_size if f.is_file() else 0))
                    if len(items)>2000:raise ValueError('Folder has too many entries')
                send(s,dict(ok=True,items=items))
            else:
                if not p.is_file():raise ValueError('File required')
                offer=self.file_metadata(dict(host=host),p,share_token=r['token']);send(s,dict(ok=True,offer=offer))
        except (OSError,ValueError):send(s,dict(ok=False,error='Share unavailable or invalid path'))
    def list_share(self,peer_id,token,relative=''):
        from .core import send,receive
        with self.connect(self.peers[peer_id]) as s:send(s,dict(type='share_list',token=token,path=relative));r=receive(s)
        if not r.get('ok'):raise ValueError(r.get('error','Share unavailable'))
        return r['items']
    def download_share(self,peer_id,token,relative,save_dir):
        from .core import send,receive
        with self.connect(self.peers[peer_id]) as s:send(s,dict(type='share_get',token=token,path=relative));r=receive(s)
        if not r.get('ok'):raise ValueError(r.get('error','Share unavailable'))
        self.register_offer(r['offer'],notify=False);return self.accept_file(r['offer']['id'],save_dir)
