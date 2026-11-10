"""Bounded JPEG-only inline images. Never accepts remote file paths."""
import base64,hashlib,os,pathlib,re
MAX_BYTES=512*1024

def validate_jpeg(data):
    if not isinstance(data,bytes) or not 4<=len(data)<=MAX_BYTES or not data.startswith(b'\xff\xd8') or not data.endswith(b'\xff\xd9'):raise ValueError('Invalid JPEG')
    from PySide6.QtCore import QByteArray,QBuffer,QIODevice
    from PySide6.QtGui import QImageReader
    buf=QBuffer();buf.setData(QByteArray(data));buf.open(QIODevice.OpenModeFlag.ReadOnly)
    reader=QImageReader(buf,b'jpeg');size=reader.size()
    if not size.isValid() or size.width()>1600 or size.height()>1200:raise ValueError('Image dimensions exceed 1600x1200')
    image=reader.read()
    if image.isNull():raise ValueError('JPEG decode failed')
    return data

def encode_image(data):
    validate_jpeg(data)
    return dict(format='jpeg',sha256=hashlib.sha256(data).hexdigest(),data=base64.b64encode(data).decode('ascii'))

def decode_image(payload):
    if not isinstance(payload,dict) or payload.get('format')!='jpeg':raise ValueError('JPEG required')
    encoded=payload.get('data');digest=payload.get('sha256')
    if not isinstance(encoded,str) or len(encoded)>4*((MAX_BYTES+2)//3) or not isinstance(digest,str) or not re.fullmatch('[0-9a-f]{64}',digest):raise ValueError('Invalid image payload')
    try:data=base64.b64decode(encoded,validate=True)
    except Exception as e:raise ValueError('Invalid base64') from e
    if hashlib.sha256(data).hexdigest()!=digest:raise ValueError('Image digest mismatch')
    return validate_jpeg(data)

def save_image(root,payload):
    data=decode_image(payload);folder=pathlib.Path(root)/'images'
    if folder.is_symlink():raise ValueError('Images directory cannot be a symlink')
    folder.mkdir(exist_ok=True);dest=folder/(payload['sha256']+'.jpg')
    if dest.is_symlink():raise ValueError('Image cannot be a symlink')
    try:
        with dest.open('xb') as f:f.write(data)
    except FileExistsError:
        if not dest.is_file() or dest.read_bytes()!=data:raise ValueError('Existing image differs')
    return str(dest)
