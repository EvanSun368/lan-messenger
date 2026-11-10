import socket
import unittest
from unittest.mock import patch
from lanlink.link_preview import extract_links, validate_url, public_addresses, fetch_preview, _Metadata

class LinkPreview(unittest.TestCase):
 def test_extract(self):
  self.assertEqual(extract_links('https://example.com。 https://example.com。 javascript:x'), ['https://example.com'])
 def test_invalid_urls(self):
  for u in ['file:///etc/passwd','http://u:p@example.com','http://localhost','http://127.0.0.1:22','https://example.com\nX:y']:
   with self.assertRaises(ValueError): validate_url(u)
 def test_nonpublic_and_mixed_dns(self):
  for ip in ['127.0.0.1','192.168.1.1','169.254.169.254','::1','::ffff:8.8.8.8','224.0.0.1','100.64.0.1']:
   with patch('socket.getaddrinfo', return_value=[(socket.AF_INET,1,6,'',(ip,80))]):
    with self.assertRaises(ValueError): public_addresses('evil.example',80)
  with patch('socket.getaddrinfo', return_value=[(2,1,6,'',('8.8.8.8',80)),(2,1,6,'',('10.0.0.1',80))]):
   with self.assertRaises(ValueError): public_addresses('mixed.example',80)
 def test_metadata_text_only(self):
  with patch('lanlink.link_preview._request', return_value=(200,{'content-type':'text/html'}, b'<title>Hello &amp; world</title><meta name="description" content="Test"><script>alert(1)</script>')):
   self.assertEqual(fetch_preview('https://example.com')['title'], 'Hello & world')
 def test_redirect_checked(self):
  with patch('lanlink.link_preview._request', return_value=(302,{'location':'file:///etc/passwd'},b'')):
   with self.assertRaises(ValueError): fetch_preview('https://example.com')
 def test_redirect_limit(self):
  with patch('lanlink.link_preview._request', return_value=(302,{'location':'https://example.com/again'},b'')) as req:
   with self.assertRaises(ValueError): fetch_preview('https://example.com')
   self.assertEqual(req.call_count,4)
 def test_request_pins_validated_address(self):
  from unittest.mock import MagicMock
  from lanlink.link_preview import _request
  import time
  sock = MagicMock(); response = MagicMock(); response.status=200
  response.getheaders.return_value=[('Content-Type','text/html')]
  response.read1.side_effect=[b'<title>x</title>',b'']
  conn = MagicMock(); conn.getresponse.return_value=response
  with patch('lanlink.link_preview.public_addresses',return_value=['8.8.8.8']) as resolve, patch('socket.create_connection',return_value=sock) as connect, patch('http.client.HTTPConnection',return_value=conn):
   _request('http://example.com/',time.monotonic()+8)
   self.assertEqual(connect.call_args.args[0],('8.8.8.8',80))
   resolve.assert_called_once_with('example.com',80)
 def test_size_and_content_limits(self):
  from unittest.mock import MagicMock
  from lanlink.link_preview import _request, MAX_BYTES
  import time
  for headers in [[('Content-Type','application/pdf')],[('Content-Type','text/html'),('Content-Length',str(MAX_BYTES+1))],[('Content-Type','text/html'),('Content-Encoding','gzip')]]:
   sock=MagicMock(); response=MagicMock(); response.status=200;response.getheaders.return_value=headers
   conn=MagicMock();conn.getresponse.return_value=response
   with patch('lanlink.link_preview.public_addresses',return_value=['8.8.8.8']),patch('socket.create_connection',return_value=sock),patch('http.client.HTTPConnection',return_value=conn):
    with self.assertRaises(ValueError):_request('http://example.com',time.monotonic()+8)
