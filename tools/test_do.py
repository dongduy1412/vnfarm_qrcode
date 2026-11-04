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
from cryptography.hazmat.primitives.asymmetric import ec

root = Path(__file__).resolve().parents[1] / 'cloudflare'
runtime = root / '.runtime'
runtime.mkdir(exist_ok=True)
key = ec.generate_private_key(ec.SECP256R1())
der = key.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
public = base64.b64encode(der).decode()
kid = hashlib.sha256(der).hexdigest()
device = '7ce8b1b17e60'
base = f'http://127.0.0.1:8767/api/devices/{device}'

def request(path, body=None):
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(path, data=data, headers={'Content-Type': 'application/json'})
    try:
        with urllib.request.urlopen(req, timeout=5) as response:
            return response.status, json.load(response)
    except urllib.error.HTTPError as error:
        return error.code, json.load(error)

def signed(iat=None):
    p = dict(device_id=device, kid=kid, iat=iat or int(time.time()), nonce=secrets.token_hex(16), uptime=12345)
    payload = f'ping-v1\n{device}\n{kid}\n{p["iat"]}\n{p["nonce"]}\n{p["uptime"]}'.encode()
    p['sig'] = base64.urlsafe_b64encode(key.sign(payload, ec.ECDSA(hashes.SHA256()))).rstrip(b'=').decode()
    return p

log = (runtime / 'test-do.log').open('w')
process = subprocess.Popen(['node', 'node_modules/wrangler/bin/wrangler.js', 'dev', '--ip', '127.0.0.1', '--port', '8767',
    '--var', f'DEVICE_PUBLIC_KEY:{public}', '--persist-to', str(runtime / secrets.token_hex(6))], cwd=root, stdout=log, stderr=log)
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
finally:
    subprocess.run(['taskkill', '/PID', str(process.pid), '/T', '/F'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    log.close()
