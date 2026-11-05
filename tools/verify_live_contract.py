"""Verify the QR currently displayed by real firmware, without exporting its key."""
import argparse
import json
import time
import email.utils
import urllib.error
import urllib.request
from urllib.parse import urlsplit,parse_qs
import serial
from verify_qr import verify_qr

BASE='https://vnfarm-qrcode-device-link.dong141220047.workers.dev'
class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*args): return None
opener=urllib.request.build_opener(NoRedirect())
def call(url,data=None,cookie=''):
    headers={'User-Agent':'Mozilla/5.0 (VNFARM protocol test)','Accept':'application/json'}
    if cookie: headers['Cookie']=cookie
    if data is not None: headers['Content-Type']='application/json'
    request=urllib.request.Request(url,data=data,headers=headers)
    try: response=opener.open(request,timeout=15)
    except urllib.error.HTTPError as error: response=error
    with response:
        raw=response.read()
        body=json.loads(raw) if raw and 'application/json' in response.headers.get('Content-Type','') else {}
        return response.status,body,dict(response.headers)

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port',default='COM5')
args=parser.parse_args()
public=url=None
with serial.Serial(args.port,115200,timeout=.3) as board:
    board.reset_input_buffer();board.write(b'QR_INFO\n')
    until=time.monotonic()+8
    while time.monotonic()<until and (not public or not url):
        line=board.readline().decode(errors='replace').strip()
        if line.startswith('QR public key: '):public=line.split(': ',1)[1]
        if line.startswith('QR test URL: '):url=line.split(': ',1)[1]
        if 'QR not ready' in line: raise RuntimeError('Complete Wi-Fi/NTP before testing the displayed QR')
assert public and url,'No QR diagnostic received'
fields=parse_qs(urlsplit(url).query)
assert int(fields['t'][0])%30==0
_,_,clock_headers=call(BASE+'/health')
cloud_now=int(email.utils.parsedate_to_datetime(clock_headers['Date']).timestamp())
verify_qr(url,public,device_id=fields['did'][0],base_url=BASE+'/checkin',now=cloud_now)
code,_,headers=call(url)
assert code==302 and headers['Location'].startswith('/login?returnTo=')
code,_,headers=call(BASE+'/test/session',data=b'{}')
assert code==302 and 'Set-Cookie' in headers
cookie=headers['Set-Cookie'].split(';')[0]
code,status,headers=call(url,cookie=cookie)
assert code==302 and status.get('checkedIn') and headers['Location']=='/stamp',(code,status)
code,repeated,_=call(url,cookie=cookie)
assert code==302 and repeated==status,'Repeat must not duplicate/extend the check-in'
code,remote,_=call(BASE+'/api/checkin/status',cookie=cookie)
assert code==200 and remote['checkedIn'] and remote['deviceId']==fields['did'][0]
code,local,_=call('http://127.0.0.1:8765/api/checkin',json.dumps({'url':url}).encode())
assert code==200 and local.get('ok'),(code,local)
print('PASS: actual displayed ESP32 QR accepted by Python, deployed Worker and local web test.')
print(f"device={fields['did'][0]} t={fields['t'][0]} URL_length={len(url)}; Worker 302 /stamp; repeated check-in unchanged.")
print('Device private key never exported; isolated Worker test session only, not event-app login.')
