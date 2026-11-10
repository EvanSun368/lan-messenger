"""User-edited public LAN profile. Not authenticated; never treat as identity proof."""
import json
import os

FIELDS = {'nickname': 80, 'signature': 200, 'organization': 80, 'department': 80, 'location': 80}

def validate_profile(data):
    if not isinstance(data, dict):
        raise ValueError('资料格式错误')
    result = {}
    for key, limit in FIELDS.items():
        value = data.get(key, '')
        if not isinstance(value, str) or len(value) > limit:
            raise ValueError('资料字段过长或类型错误：' + key)
        if any(ord(c) < 32 or ord(c) == 127 for c in value):
            raise ValueError('资料不能包含控制字符')
        result[key] = value.strip()
    if not result['nickname']:
        raise ValueError('昵称不能为空')
    return result

class ProfileMixin:
    def init_profiles(self):
        path = self.root / 'profile.json'
        self.profile = dict.fromkeys(FIELDS, '')
        self.profile['nickname'] = self.name or '我的电脑'
        if path.exists():
            try:
                if path.stat().st_size > 8192:
                    raise ValueError('Profile too large')
                self.profile = validate_profile(json.loads(path.read_text(encoding='utf-8')))
            except (OSError, ValueError, TypeError):
                self.emit('error', message='个人资料读取失败，使用默认昵称')
        self.name = self.profile['nickname']

    def public_profile(self):
        with self.lock:
            return dict(self.profile)

    def update_profile(self, data):
        profile = validate_profile(data)
        with self.lock:
            path = self.root / 'profile.json'
            tmp = self.root / 'profile.json.tmp'
            tmp.write_text(json.dumps(profile, ensure_ascii=False), encoding='utf-8')
            os.replace(tmp, path)
            self.profile = profile
            self.name = profile['nickname']
        self.emit('profile_updated', profile=dict(profile))
        return dict(profile)

    def serve_profile(self, sock):
        from .core import send
        send(sock, {'id': self.id, 'profile': self.public_profile()})

    def fetch_profile(self, peer_id):
        from .core import send, receive
        with self.lock:
            peer = dict(self.peers[peer_id])
        with self.connect(peer) as sock:
            sock.settimeout(5)
            send(sock, {'type': 'profile_get', 'sender': self.hello()})
            reply = receive(sock)
        if not isinstance(reply, dict) or reply.get('id') != peer_id:
            raise ValueError('资料响应与联系人不匹配')
        profile = validate_profile(reply.get('profile'))
        self.emit('profile_received', peer_id=peer_id, profile=profile)
        return profile
