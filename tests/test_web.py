import base64
import hashlib
import http.cookiejar
import json
import sys
import tempfile
import threading
import time
import unittest
import urllib.error
import urllib.request
from pathlib import Path
from urllib.parse import urlencode

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "web"))
from server import TestBench, make_server


class WebTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.bench = TestBench("http://127.0.0.1:1/checkin", Path(self.tmp.name) / "db.sqlite3")
        self.server = make_server("127.0.0.1", 0, self.bench)
        self.base = f"http://127.0.0.1:{self.server.server_port}"
        self.bench.base_url = self.base + "/checkin"
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.client = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))
        self.key = ec.generate_private_key(ec.SECP256R1())
        public = self.key.public_key()
        self.pem = public.public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
        self.pem = base64.urlsafe_b64encode(self.pem).rstrip(b"=").decode()
        der = public.public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
        self.kid = hashlib.sha256(der).hexdigest()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.bench.db.close()
        self.tmp.cleanup()

    def request(self, path, data=None, *, content_type="application/json", headers=None):
        body = data if isinstance(data, bytes) else json.dumps(data).encode() if data is not None else None
        req_headers = {"Content-Type": content_type, **(headers or {})}
        request = urllib.request.Request(self.base + path, data=body, headers=req_headers)
        try:
            response = self.client.open(request)
        except urllib.error.HTTPError as exc:
            response = exc
        with response:
            return response.status, json.loads(response.read())

    def register(self):
        status, _ = self.request("/api/register", {"device_id": "7c:e8:b1:b1:7e:60", "public_key": self.pem, "ttl": 60})
        self.assertEqual(status, 200)

    def qr(self, issued=None):
        t = issued if issued is not None else int(time.time())//30*30
        fields = {"v":"1","did":"7ce8b1b17e60","t":str(t)}
        der=self.key.sign(f'v1.7ce8b1b17e60.{t}'.encode(),ec.ECDSA(hashes.SHA256()))
        r, sig_s=utils.decode_dss_signature(der)
        fields['sig']=base64.urlsafe_b64encode(r.to_bytes(32,'big')+sig_s.to_bytes(32,'big')).rstrip(b'=').decode()
        return self.bench.base_url+'?'+urlencode(fields)

    def test_photo_api_not_in_test_scope(self):
        status, data = self.request("/api/photo", b"image", content_type="image/png")
        self.assertEqual(status, 404)

    def test_register_verify_logout_flow(self):
        self.register()
        self.assertEqual(self.request("/api/checkin", {"url": self.qr()})[0], 200)
        status = self.request("/api/status")[1]
        self.assertEqual(status["session"]["device_id"], "7ce8b1b17e60")
        self.assertLessEqual(status["session"]["issued_at"], status["session"]["checked_in_at"])
        self.assertNotIn("photos", status["session"])
        self.request("/api/logout", {})
        self.assertIsNone(self.request("/api/status")[1]["session"])

    def test_expired_qr_does_not_create_session(self):
        self.register()
        self.assertEqual(self.request("/api/checkin", {"url": self.qr(int(time.time()) - 61)})[0], 410)
        self.assertIsNone(self.request("/api/status")[1]["session"])

    def test_tampered_qr_does_not_create_session(self):
        self.register()
        url = self.qr().replace("did=7ce8b1b17e60", "did=7ce8b1b17e61")
        self.assertEqual(self.request("/api/checkin", {"url": url})[0], 404)
        self.assertIsNone(self.request("/api/status")[1]["session"])

    def test_unregistered_device_rejected(self):
        self.assertEqual(self.request("/api/checkin", {"url": self.qr()})[0], 404)

    def test_key_update_revokes_old_session(self):
        self.register()
        self.request("/api/checkin", {"url": self.qr()})
        self.register()
        self.assertIsNone(self.request("/api/status")[1]["session"])

    def test_remove_key_revokes_old_session(self):
        self.register()
        self.request("/api/checkin", {"url": self.qr()})
        self.request("/api/remove", {"device_id": "7ce8b1b17e60"})
        status = self.request("/api/status")[1]
        self.assertIsNone(status["session"])
        self.assertEqual(status["devices"], [])

    def test_invalid_public_key_rejected(self):
        self.assertEqual(self.request("/api/register", {"device_id": "7ce8b1b17e60", "public_key": "bad", "ttl": 60})[0], 400)

    def test_cross_origin_post_rejected(self):
        self.assertEqual(self.request("/api/logout", {}, headers={"Origin": "http://other.test"})[0], 403)

    def test_wrong_host_rejected(self):
        self.assertEqual(self.request("/api/status", headers={"Host": "other.test"})[0], 400)

    def test_connection_status_does_not_claim_firmware_online(self):
        status, data = self.request("/api/status")
        self.assertEqual(status, 200)
        self.assertIn("supported", data["usb"])
        self.assertIsNone(data["session"])
        self.assertNotIn("firmware_online", data)

    def test_repeat_same_code_keeps_session(self):
        self.register()
        qr = self.qr()
        first = self.request('/api/checkin', {'url': qr})[1]['session']
        second = self.request('/api/checkin', {'url': qr})[1]['session']
        self.assertEqual(first, second)
        self.assertEqual(len(self.bench.sessions), 1)

    def test_contract_status_api(self):
        self.assertEqual(self.request('/api/checkin/status')[1], {'checkedIn': False})
        self.register()
        self.request('/api/checkin', {'url': self.qr()})
        status = self.request('/api/checkin/status')[1]
        self.assertTrue(status['checkedIn'])
        self.assertEqual(status['deviceId'], '7ce8b1b17e60')

    def test_legacy_pem_registration_migrates_without_key_rotation(self):
        import sqlite3
        db_path = Path(self.tmp.name) / 'legacy.sqlite3'
        pem = self.key.public_key().public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo).decode()
        with sqlite3.connect(db_path) as db:
            db.execute('CREATE TABLE devices (device TEXT PRIMARY KEY,pem TEXT,kid TEXT,ttl INTEGER)')
            db.execute('INSERT INTO devices VALUES (?,?,?,30)', ('7ce8b1b17e60',pem,self.kid))
        db.close()
        migrated = TestBench(self.bench.base_url, db_path)
        try:
            row = migrated.db.execute('SELECT pem,kid,ttl FROM devices').fetchone()
            self.assertEqual(row, (self.pem,self.kid,60))
        finally:
            migrated.db.close()

    def test_wifi_admin_relay_does_not_expose_credentials(self):
        from unittest.mock import patch
        with patch('server.remote_wifi_request',return_value=(202,{'command':'reconnect_wifi','cmdId':'c_test'})) as relay:
            code,body=self.request('/api/wifi/commands',{'command':'reconnect_wifi'})
            self.assertEqual(code,202)
            self.assertEqual(body['cmdId'],'c_test')
            self.assertNotIn('token',body)
            relay.assert_called_once_with('reconnect_wifi')
        with patch('server.remote_wifi_request') as relay:
            self.assertEqual(self.request('/api/wifi/commands',{'command':'rotate_key'})[0],400)
            self.assertEqual(self.request('/api/wifi/commands',{'command':'wipe_wifi'},headers={'Origin':'http://other.test'})[0],403)
            relay.assert_not_called()

    def test_wifi_admin_requires_local_computer(self):
        from unittest.mock import patch,MagicMock
        remote=MagicMock(is_loopback=False)
        with patch('server.ipaddress.ip_address',return_value=remote),patch('server.remote_wifi_request') as relay:
            self.assertEqual(self.request('/api/wifi/commands')[0],403)
            self.assertEqual(self.request('/api/wifi/commands',{'command':'wipe_wifi'})[0],403)
            relay.assert_not_called()


if __name__ == "__main__":
    unittest.main()

