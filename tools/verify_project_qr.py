"""Verify the real board QR against its configured project-compatible server."""
import serial,time,json,email.utils,urllib.request
from urllib.parse import urlsplit,parse_qs
from verify_qr import verify_qr
public=url=None
board=serial.Serial();board.port='COM5';board.baudrate=115200;board.timeout=.3;board.dtr=False;board.rts=False
with board:
    if not board.is_open:board.open()
    end=time.monotonic()+30;next_query=0
    while time.monotonic()<end and not (public and url):
        if time.monotonic()>=next_query:board.write(b'QR_INFO\n');next_query=time.monotonic()+2
        line=board.readline().decode(errors='replace').strip()
        if line.startswith('QR public key: '):public=line.split(': ',1)[1]
        if line.startswith('QR test URL: '):url=line.split(': ',1)[1]
assert public and url,'Board QR is not ready'
parts=urlsplit(url);base=parts.scheme+'://'+parts.netloc
assert parts.scheme=='https'
def request(address):
    return urllib.request.urlopen(urllib.request.Request(address,headers={'User-Agent':'Mozilla/5.0 (VNFARM hardware verification)'}),timeout=15)
with request(base+'/health') as r:now=int(email.utils.parsedate_to_datetime(r.headers['Date']).timestamp())
fields=parse_qs(parts.query)
verify_qr(url,public,device_id=fields['did'][0],base_url=base+'/checkin',now=now)
with request(url) as r:
    data=json.load(r);assert r.status==200 and data['valid'] is True and data['deviceId']==fields['did'][0]
print('PASS: actual board QR verified locally and by configured Worker; did='+fields['did'][0])
print('URL origin='+base+'; length='+str(len(url))+'; private key never exported.')
