"""Actual Python 0.4 <-> C++ message, group, image and profile interoperability."""
import argparse,json,pathlib,subprocess,sys,tempfile,time,threading
parser=argparse.ArgumentParser();parser.add_argument('--peer',required=True);parser.add_argument('--python-root',required=True);a=parser.parse_args();sys.path.insert(0,a.python_root)
from lanlink.core import Node
from PySide6.QtGui import QImage
from PySide6.QtCore import QBuffer,QIODevice

def until(test):
    end=time.monotonic()+10
    while time.monotonic()<end:
        if test():return
        time.sleep(.03)
    raise AssertionError('Timed out')
with tempfile.TemporaryDirectory() as root:
    proc=subprocess.Popen([a.peer,root+'/cpp'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    node=None
    try:
        info=json.loads(proc.stdout.readline());events=[]
        node=Node('Python peer',root+'/py',lambda k,d:events.append((k,d)),discovery_port=0);node.start();node.add_peer('127.0.0.1',info['port']);cpp=info['id']
        mid=node.send_message(cpp,'Python to C++');until(lambda:any(d.get('text')=='echo:Python to C++' for k,d in events if k=='message'))
        gid=node.create_group([cpp],'interop group');node.send_group_message(gid,'group');until(lambda:any(d.get('text')=='echo:group' and d.get('peer_id')=='group:'+gid for k,d in events if k=='message'))
        im=QImage(40,30,QImage.Format.Format_RGB32);im.fill(0xff223344);b=QBuffer();b.open(QIODevice.OpenModeFlag.WriteOnly);im.save(b,'JPEG');node.send_image(cpp,bytes(b.data()));until(lambda:any(d.get('image_path') and d.get('direction')=='in' for k,d in events if k=='message'))
        assert node.fetch_profile(cpp)['nickname']=='C++ peer'
        print('PASS Python/C++ direct message, group echo, JPEG echo, profile')
    finally:
        if node:node.stop()
        proc.terminate();proc.wait(timeout=5)
