"""Local firmware test bench. Run: python web/server.py --lan-ip 192.168.1.221"""
from __future__ import annotations

import argparse
import hashlib
import ipaddress
import json
import re
import secrets
import sqlite3
import sys
import threading
import time
from http.cookies import SimpleCookie
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec

ROOT = Path(__file__).resolve().parents[1]
STATIC = Path(__file__).parent / "static"
sys.path.insert(0, str(ROOT / "tools"))
from verify_qr import verify_qr, parse_qr, load_raw_key, b64url, ContractError
from test_clock import CloudClock, ClockUnavailable

SESSION_SECONDS = 30 * 60  # Test session policy; independent of QR validity.

def usb_status():
    """Enumerate ports only; never open/reset the board or block flashing."""
    try:
        from serial.tools import list_ports
        ports = [{"port": p.device, "description": p.description, "serial_number": p.serial_number}
                 for p in list_ports.comports() if p.vid == 0x303A and p.pid == 0x1001]
        return {"supported": True, "ports": ports}
    except (ImportError, OSError):
        return {"supported": False, "ports": []}


class TestBench:
    def __init__(self, base_url: str, database: Path, clock=time.time):
        self.base_url = base_url
        self.clock = clock
        database.parent.mkdir(parents=True, exist_ok=True)
        self.db = sqlite3.connect(database, check_same_thread=False)
        self.db.execute("CREATE TABLE IF NOT EXISTS devices (device TEXT PRIMARY KEY, pem TEXT NOT NULL, kid TEXT NOT NULL, ttl INTEGER NOT NULL)")
        # Migrate legacy PEM registrations to the same raw public key.
        for device, stored, oldkid, ttl in self.db.execute("SELECT device,pem,kid,ttl FROM devices").fetchall():
            if stored.startswith("-----BEGIN PUBLIC KEY-----"):
                public = serialization.load_pem_public_key(stored.encode('ascii'))
                raw = public.public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
                self.db.execute("UPDATE devices SET pem=?,ttl=60 WHERE device=?", (b64url(raw),device))
        self.db.commit()
        self.lock = threading.RLock()
        self.sessions: dict[str, dict] = {}

    def devices(self):
        with self.lock:
            rows = self.db.execute("SELECT device, kid, ttl FROM devices ORDER BY device").fetchall()
        return [{"device_id": row[0], "kid": row[1], "ttl": row[2]} for row in rows]

    def register(self, data):
        device = data.get("device_id", "").replace(":", "").strip().lower()
        pem = data.get("public_key", "").strip()
        ttl = data.get("ttl")
        if not re.fullmatch(r"[a-z0-9-]{1,32}", device):
            raise ValueError("Device ID khong hop le.")
        if type(ttl) is not int or ttl not in (10,20,30,60):
            raise ValueError("TTL server phai la 10, 20, 30 hoac 60 giay.")
        public = load_raw_key(pem)
        der = public.public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
        kid = hashlib.sha256(der).hexdigest()
        canonical_pem = b64url(public.public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint))
        with self.lock:
            self.db.execute("INSERT OR REPLACE INTO devices VALUES (?, ?, ?, ?)", (device, canonical_pem, kid, ttl))
            self.db.commit()
            self.sessions = {token: s for token, s in self.sessions.items() if s["device_id"] != device}
        return {"device_id": device, "kid": kid, "ttl": ttl}

    def remove(self, device):
        with self.lock:
            self.db.execute("DELETE FROM devices WHERE device = ?", (device,))
            self.db.commit()
            self.sessions = {token: s for token, s in self.sessions.items() if s["device_id"] != device}

    def checkin(self, url, token=None):
        fields = parse_qr(url, self.base_url)
        device = fields['did']
        with self.lock:
            row = self.db.execute("SELECT pem, ttl FROM devices WHERE device = ?", (device,)).fetchone()
        if not row:
            raise ContractError('unknown_device',404)
        now = int(self.clock())
        verified = verify_qr(url,row[0],device_id=device,base_url=self.base_url,expected_ttl=row[1],skew=5,now=now)
        with self.lock:
            previous = self.sessions.get(token)
            if previous and previous['device_id']==device and previous['issued_at']==verified['issued_at']:
                return token, dict(previous)
            # Local-only test identity. Production uses the event app's login.
            if not previous: token = secrets.token_urlsafe(32)
            session = {**verified,'checked_in_at':now,'session_expires_at':now+SESSION_SECONDS}
            self.sessions = {k:v for k,v in self.sessions.items() if v['session_expires_at']>now}
            self.sessions[token] = session
        return token,session

    def session(self, token):
        with self.lock:
            session = self.sessions.get(token)
            if session and session["session_expires_at"] <= self.clock():
                self.sessions.pop(token, None)
                session = None
            return dict(session) if session else None

class Handler(BaseHTTPRequestHandler):
    server_version = "VNFarmTest/0.1"

    @property
    def bench(self):
        return self.server.bench

    def log_message(self, fmt, *args):
        # QR URLs contain signatures: keep access logs to method/path/status only.
        pass

    def host_ok(self):
        return self.headers.get("Host", "") in self.server.allowed_hosts

    def origin_ok(self):
        origin = self.headers.get("Origin")
        return not origin or origin == "http://" + self.headers.get("Host", "")

    def send(self, status, body, content_type="application/json; charset=utf-8", cookie=None):
        if isinstance(body, (dict, list)):
            body = json.dumps(body, ensure_ascii=False).encode("utf-8")
        elif isinstance(body, str):
            body = body.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' blob: data:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'")
        if cookie:
            self.send_header("Set-Cookie", cookie)
        self.end_headers()
        self.wfile.write(body)

    def token(self):
        cookie = SimpleCookie()
        try:
            cookie.load(self.headers.get("Cookie", ""))
            return cookie["vnfarm_session"].value if "vnfarm_session" in cookie else ""
        except Exception:
            return ""

    def read_body(self, maximum):
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            raise ValueError("Kích thước dữ liệu không hợp lệ.") from None
        if length < 0 or length > maximum:
            raise ValueError("Dữ liệu gửi lên quá lớn.")
        return self.rfile.read(length)

    def read_json(self):
        if self.headers.get("Content-Type", "").split(";")[0] != "application/json":
            raise ValueError("Cần gửi dữ liệu JSON.")
        data = json.loads(self.read_body(8192))
        if not isinstance(data, dict):
            raise ValueError("Dữ liệu phải là một object JSON.")
        return data

    def do_GET(self):
        if not self.host_ok():
            return self.send(400, {"error": "Địa chỉ máy chủ không hợp lệ."})
        path = urlsplit(self.path).path
        if path in ('/api/status', '/api/checkin/status'):
            try:
                current_session = self.bench.session(self.token())
            except ClockUnavailable:
                return self.send(503, {"error": "clock_unavailable"})
        if path == "/api/checkin/status":
            session = current_session
            return self.send(200, {"checkedIn": True, "expiresAt": session['session_expires_at'], "deviceId": session['device_id']} if session else {"checkedIn": False})
        if path == "/api/status":
            return self.send(200, {"base_url": self.bench.base_url, "devices": self.bench.devices(),
                                   "session": current_session,
                                   "can_register": ipaddress.ip_address(self.client_address[0]).is_loopback,
                                   "session_minutes": SESSION_SECONDS // 60, "usb": usb_status()})
        paths = {"/": ("index.html", "text/html; charset=utf-8"),
                 "/checkin": ("index.html", "text/html; charset=utf-8"),
                 "/app.js": ("app.js", "text/javascript; charset=utf-8"),
                 "/style.css": ("style.css", "text/css; charset=utf-8")}
        if path not in paths:
            return self.send(404, {"error": "Không tìm thấy trang."})
        filename, mime = paths[path]
        return self.send(200, (STATIC / filename).read_bytes(), mime)

    def do_POST(self):
        if not self.host_ok() or not self.origin_ok():
            return self.send(403, {"error": "Yêu cầu không cùng nguồn với web test."})
        path = urlsplit(self.path).path
        try:
            if path in ("/api/register", "/api/remove"):
                if not ipaddress.ip_address(self.client_address[0]).is_loopback:
                    return self.send(403, {"error": "Đăng ký/xóa key trên máy tính tại http://127.0.0.1:" + str(self.server.server_port)})
                data = self.read_json()
                if path == "/api/register":
                    return self.send(200, self.bench.register(data))
                self.bench.remove(data.get("device_id", ""))
                return self.send(200, {"ok": True})
            if path == "/api/checkin":
                data = self.read_json()
                token, session = self.bench.checkin(data.get("url", ""), self.token())
                return self.send(200, {"ok": True, "session": session},
                                 cookie=f"vnfarm_session={token}; Path=/; HttpOnly; SameSite=Lax; Max-Age={SESSION_SECONDS}")
            if path == "/api/logout":
                with self.bench.lock:
                    self.bench.sessions.pop(self.token(), None)
                return self.send(200, {"ok": True}, cookie="vnfarm_session=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0")
            return self.send(404, {"error": "Không tìm thấy API."})
        except ContractError as exc:
            return self.send(exc.status, {"error": exc.code})
        except ClockUnavailable:
            return self.send(503, {"error": "clock_unavailable"})
        except PermissionError as exc:
            return self.send(403, {"error": str(exc)})
        except (ValueError, TypeError, AttributeError, UnicodeError) as exc:
            return self.send(400, {"error": str(exc) or "Dữ liệu không hợp lệ."})


def make_server(host, port, bench):
    server = ThreadingHTTPServer((host, port), Handler)
    server.daemon_threads = True
    server.bench = bench
    server.allowed_hosts = {urlsplit(bench.base_url).netloc, f"127.0.0.1:{server.server_port}", f"localhost:{server.server_port}"}
    return server


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lan-ip", required=True, help="Wi-Fi IPv4 address of this computer")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--checkin-url", default="https://vnfarm-qrcode-device-link.dong141220047.workers.dev/checkin", help="Trusted QR URL, independent of the local web address")
    parser.add_argument("--clock-url", help="Optional trusted HTTPS health endpoint for local test clock")
    args = parser.parse_args()
    try:
        lan_ip = ipaddress.IPv4Address(args.lan_ip)
        if not lan_ip.is_private or lan_ip.is_loopback:
            raise ValueError("Use a private LAN IPv4 address")
    except ValueError as exc:
        parser.error(str(exc))
    clock = CloudClock(args.clock_url) if args.clock_url else time.time
    bench = TestBench(args.checkin_url, ROOT / "web" / ".runtime" / "devices.sqlite3", clock=clock)
    server = make_server("0.0.0.0", args.port, bench)
    server.allowed_hosts.add(f"{lan_ip}:{args.port}")
    print(f"May tinh / dang ky key: http://127.0.0.1:{args.port}", flush=True)
    print(f"Dien thoai / QR URL:    {bench.base_url}", flush=True)
    print("Local test: key, chu ky QR va ket noi. Ctrl+C de dung.", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        bench.db.close()

