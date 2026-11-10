"""Standard-library wire interoperability fixture; matches LanLink Python v1 framing."""
import socket,sys,json,hashlib,struct
port=int(sys.argv[1]);token=sys.argv[2]
def send(s,o):
 b=json.dumps(o).encode();s.sendall(struct.pack('!I',len(b))+b)
def exact(s,n):
 b=b''
 while len(b)<n:
  x=s.recv(n-len(b))
  if not x:raise EOFError()
  b+=x
 return b
def recv(s):return json.loads(exact(s,struct.unpack('!I',exact(s,4))[0]))
def req(o):
 with socket.create_connection(('127.0.0.1',port),5) as s:
  s.settimeout(10);send(s,o);return recv(s)
listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(10)
hello={'id':'a'*32,'name':'Python peer','port':listener.getsockname()[1]}
req({'type':'hello','sender':hello})
# C++ share -> Python: JSON header followed by raw payload and release acknowledgement.
o=req({'type':'share_get','token':token,'path':'interop.bin'})['offer']
with socket.create_connection(('127.0.0.1',port),5) as s:
 s.settimeout(10);send(s,{'type':'fetch','id':o['id'],'offset':0});assert recv(s)['ok'];data=exact(s,o['size']);assert hashlib.sha256(data).hexdigest()==o['sha256'];assert data==b'cpp-to-python'
assert req({'type':'file_release','id':o['id']})['ok']
# Python offer -> C++ accepts and fetches; C++ verifies SHA256 before publish.
data=b'python-to-cpp'*100
assert req({'type':'offer','sender':hello,'id':'b'*32,'name':'python.bin','size':len(data),'sha256':hashlib.sha256(data).hexdigest()})['ok']
s,_=listener.accept()
with s:
 s.settimeout(10);r=recv(s);assert r['type']=='fetch' and r['offset']==0;send(s,{'ok':True});s.sendall(data)
listener.close()
