"""Exercise the real local Worker/DO runtime using a disposable signing key."""
import base64
import hashlib
import json
import secrets
import subprocess
import time
import urllib.error
import urllib.request
from pathlib import Path
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils

root = Path(__file__).resolve().parents[1] / 'cloudflare'
runtime = root / '.runtime'
runtime.mkdir(exist_ok=True)
key = ec.generate_private_key(ec.SECP256R1())
der = key.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
public = base64.b64encode(der).decode()
kid = hashlib.sha256(der).hexdigest()
raw_public = base64.urlsafe_b64encode(key.public_key().public_bytes(serialization.Encoding.X962,serialization.PublicFormat.UncompressedPoint)).rstrip(b'=').decode()
device = '7ce8b1b17e60'
base = f'http://127.0.0.1:8767/api/devices/{device}'

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl): return None
opener=urllib.request.build_opener(NoRedirect())
last_headers={}
session_cookie=''
def request(path, body=None, form=None):
    global last_headers
    data = json.dumps(body).encode() if body is not None else None
    if form is not None: data=form.encode()
    req = urllib.request.Request(path, data=data, headers={'Content-Type': 'application/x-www-form-urlencoded' if form is not None else 'application/json','Accept':'application/json','Cookie':session_cookie})
    try:
        response=opener.open(req,timeout=5)
    except urllib.error.HTTPError as error:
        response=error
    with response:
        last_headers=dict(response.headers)
        data=response.read()
        return response.status,json.loads(data) if data and 'application/json' in response.headers.get('Content-Type','') else {}

def signed(iat=None):
    p = dict(device_id=device, kid=kid, iat=iat or int(time.time()), nonce=secrets.token_hex(16), uptime=12345)
    payload = f'ping-v1\n{device}\n{kid}\n{p["iat"]}\n{p["nonce"]}\n{p["uptime"]}'.encode()
    p['sig'] = base64.urlsafe_b64encode(key.sign(payload, ec.ECDSA(hashes.SHA256()))).rstrip(b'=').decode()
    return p

log = (runtime / 'test-do.log').open('w')
process = subprocess.Popen(['node', 'node_modules/wrangler/bin/wrangler.js', 'dev', '--ip', '127.0.0.1', '--port', '8767',
    '--var', f'DEVICE_PUBLIC_KEY:{public}', '--var',f'CHECKIN_PUBLIC_KEY:{raw_public}', '--persist-to', str(runtime / secrets.token_hex(6))], cwd=root, stdout=log, stderr=log)
try:
    ready = False
    for _ in range(90):
        if process.poll() is not None:
            raise RuntimeError('Worker exited; inspect cloudflare/.runtime/test-do.log')
        try:
            if request('http://127.0.0.1:8767/health')[0] == 200:
                ready = True
                break
        except (OSError, urllib.error.URLError):
            time.sleep(0.5)
    assert ready, 'Local runtime did not start; inspect cloudflare/.runtime/test-do.log'
    assert request(base + '/status')[1]['sequence'] == 0
    ping = signed()
    code, pong = request(base + '/ping', ping)
    assert code == 200 and pong['type'] == 'pong' and pong['nonce'] == ping['nonce'] and pong['sequence'] == 1, (code, pong)
    assert request(base + '/ping', ping)[0] == 409
    tampered = dict(ping, uptime=56789)
    assert request(base + '/ping', tampered)[0] == 401
    assert request(base + '/ping', signed(int(time.time()) - 120))[0] == 401
    assert request(base + '/ping', dict(ping, sig='x' * 96))[0] == 401
    status = request(base + '/status')[1]
    assert status['online'] and status['sequence'] == 1 and status['kid'] == kid
    time.sleep(2.1)
    assert request(base + '/ping', signed())[1]['sequence'] == 2
    assert request(base + '/ping', {'oversize': 'x' * 2100})[0] == 413
    print('PASS: real Worker + SQLite DO; signed ping/pong, persistent status, replay/tamper/stale/oversize rejected.')
    def qr(age=5,did=device):
        t=int(time.time())-age
        der=key.sign(f'v1.{did}.{t}'.encode(),ec.ECDSA(hashes.SHA256()))
        r,s=utils.decode_dss_signature(der)
        sig=base64.urlsafe_b64encode(r.to_bytes(32,'big')+s.to_bytes(32,'big')).rstrip(b'=').decode()
        return f'http://127.0.0.1:8767/checkin?v=1&did={did}&t={t}&sig={sig}'
    live=qr()
    assert request(live)[0]==302 and last_headers['Location'].startswith('/login?returnTo=')
    assert request('http://127.0.0.1:8767/test/session',form='returnTo=%2Fstamp')[0]==302
    session_cookie=last_headers['Set-Cookie'].split(';')[0]
    code,data=request(live)
    assert code==302 and data['checkedIn'] and last_headers['Location']=='/stamp',(code,data)
    expires=data['expiresAt']
    assert request(live)[1]['expiresAt']==expires,'Replay must be idempotent'
    assert request(qr(61))[0]==410
    assert request(qr(-7))[0]==400
    assert request(qr(did='door-unknown'))[0]==404
    assert request(live.split('&sig=')[0])[0]==400
    bad=live[:-2]+('AA' if live[-2:]!='AA' else 'BA')
    assert request(bad)[0] in (400,401)
    # Tamper a significant base64 character, retaining a canonical encoding.
    prefix,sig=live.split('&sig=')
    sig=('A' if sig[0]!='A' else 'B')+sig[1:]
    assert request(prefix+'&sig='+sig)[0]==401
    assert request(qr(-4))[0]==302
    assert request('http://127.0.0.1:8767/api/checkin/status')[1]['checkedIn']
    for _ in range(35): code,_=request(live)
    assert code==429
    print('PASS: contract HTTP 302/400/401/404/410/429, login return URL, per-user idempotence, status, skew and TTL.')
finally:
    subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    log.close()
