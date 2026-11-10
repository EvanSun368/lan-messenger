"""User-initiated, text-only public-web previews. Never execute or render HTML."""
import http.client
import ipaddress
import re
import socket
import ssl
import time
from html.parser import HTMLParser
from urllib.parse import urlsplit, urlunsplit, urljoin, quote

MAX_BYTES = 256 * 1024
TIMEOUT = 8


def extract_links(text):
    return list(dict.fromkeys(m.rstrip('.,;!?，。；！？）)]}') for m in
        re.findall(r'https?://[^\s<>\"\u200b]+', text, re.I)))[:3]


def validate_url(url):
    if not isinstance(url, str) or len(url) > 4096 or any(ord(c) < 32 for c in url):
        raise ValueError('链接无效或过长')
    p = urlsplit(url)
    if p.scheme.lower() not in ('http', 'https') or not p.hostname or p.username or p.password:
        raise ValueError('仅支持无账号信息的 HTTP/HTTPS 链接')
    host = p.hostname.encode('idna').decode('ascii')
    if '%' in host or host.endswith('.') or host.lower() == 'localhost':
        raise ValueError('不允许本机或特殊地址')
    port = p.port or (443 if p.scheme.lower() == 'https' else 80)
    if port not in (80, 443):
        raise ValueError('预览只支持标准网页端口')
    return p, host, port


def public_addresses(host, port):
    results = socket.getaddrinfo(host, port, type=socket.SOCK_STREAM)
    addresses = list(dict.fromkeys(r[4][0] for r in results))
    if not addresses:
        raise ValueError('无法解析网站地址')
    for address in addresses:
        ip = ipaddress.ip_address(address)
        if (not ip.is_global or ip.is_multicast or ip.is_reserved or
            (isinstance(ip, ipaddress.IPv6Address) and (ip.ipv4_mapped or ip.sixtofour or ip.teredo))):
            raise ValueError('不预览本机、内网或保留地址')
    return addresses


class _Metadata(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.title = ''; self.description = ''; self.in_title = False; self.ignored = 0
    def handle_starttag(self, tag, attrs):
        if tag in ('script', 'style'): self.ignored += 1
        if tag == 'title': self.in_title = True
        if tag == 'meta':
            a = dict(attrs); key = (a.get('property') or a.get('name') or '').lower()
            value = a.get('content') or ''
            if key == 'og:title': self.title = value[:300]
            if key in ('description', 'og:description'): self.description = value[:600]
    def handle_endtag(self, tag):
        if tag == 'title': self.in_title = False
        if tag in ('script', 'style'): self.ignored = max(0, self.ignored - 1)
    def handle_data(self, data):
        if self.in_title and not self.ignored: self.title = (self.title + data)[:300]


def _request(url, deadline):
    p, host, port = validate_url(url)
    addresses = public_addresses(host, port)
    remaining = deadline - time.monotonic()
    if remaining <= 0: raise TimeoutError('预览超时')
    # Pin the validated IP. Never resolve again at connection time (DNS rebinding).
    sock = socket.create_connection((addresses[0], port), timeout=min(TIMEOUT, remaining))
    conn = http.client.HTTPConnection(host, port, timeout=remaining)
    try:
        if p.scheme.lower() == 'https':
            sock = ssl.create_default_context().wrap_socket(sock, server_hostname=host)
        remaining = deadline - time.monotonic()
        if remaining <= 0: raise TimeoutError('预览超时')
        sock.settimeout(remaining)
        conn.sock = sock
        path = quote(p.path or '/', safe='/:%@!$&\'()*+,;=-._~')
        if p.query: path += '?' + quote(p.query, safe='/:?%&=+@!$\'()*+,;~-._')
        conn.request('GET', path, headers={'User-Agent':'LanLink-Preview/0.1',
                     'Accept':'text/html', 'Accept-Encoding':'identity', 'Connection':'close'})
        response = conn.getresponse()
        headers = dict((k.lower(), v) for k, v in response.getheaders())
        if response.status in (301, 302, 303, 307, 308): return response.status, headers, b''
        if response.status != 200: raise ValueError('网页返回 HTTP %s' % response.status)
        if headers.get('content-encoding', 'identity').lower() != 'identity':
            raise ValueError('不支持压缩预览响应')
        if headers.get('content-type', '').split(';')[0].strip().lower() not in ('text/html', 'application/xhtml+xml'):
            raise ValueError('仅支持网页预览')
        if int(headers.get('content-length', '0')) > MAX_BYTES: raise ValueError('网页超过预览大小限制')
        data = bytearray()
        while len(data) <= MAX_BYTES:
            remaining = deadline - time.monotonic()
            if remaining <= 0: raise TimeoutError('预览超时')
            sock.settimeout(min(2, remaining))
            chunk = response.read1(min(8192, MAX_BYTES + 1 - len(data)))
            if not chunk: break
            data.extend(chunk)
        if len(data) > MAX_BYTES: raise ValueError('网页超过预览大小限制')
        return response.status, headers, bytes(data)
    finally:
        conn.close(); sock.close()


def fetch_preview(url):
    """Call ONLY after user consent, on a worker thread. Returns plain strings."""
    deadline = time.monotonic() + TIMEOUT
    for count in range(4):
        validate_url(url)
        status, headers, data = _request(url, deadline)
        if status in (301, 302, 303, 307, 308):
            if count == 3: raise ValueError('网页重定向过多')
            location = headers.get('location')
            if not location: raise ValueError('无效重定向')
            url = urljoin(url, location)
            continue
        charset = re.search(r'charset\s*=\s*[\"\']?([\w-]+)', headers.get('content-type',''), re.I)
        encoding = charset.group(1) if charset else 'utf-8'
        try: text = data.decode(encoding, errors='replace')
        except LookupError: text = data.decode('utf-8', errors='replace')
        meta = _Metadata(); meta.feed(text)
        p = urlsplit(url)
        clean = lambda s: ' '.join(''.join(c for c in s if c.isprintable()).split())
        return {'url': url, 'site':p.hostname, 'title':clean(meta.title) or p.hostname,
                'description':clean(meta.description)}
    raise ValueError('无法加载预览')
