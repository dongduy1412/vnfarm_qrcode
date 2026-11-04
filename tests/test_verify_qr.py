import base64
import hashlib
import sys
import unittest
from pathlib import Path
from urllib.parse import urlencode

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from verify_qr import verify_qr


class VerificationTests(unittest.TestCase):
    def setUp(self):
        self.key = ec.generate_private_key(ec.SECP256R1())
        public = self.key.public_key()
        self.pem = public.public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo)
        der = public.public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
        self.fields = {"v": "1", "dev": "7ce8b1b17e60", "kid": hashlib.sha256(der).hexdigest(),
                       "iat": "1793770000", "ttl": "30"}
        canonical = "\n".join(["v1", self.fields["dev"], self.fields["kid"], self.fields["iat"], self.fields["ttl"]])
        signature = self.key.sign(canonical.encode("ascii"), ec.ECDSA(hashes.SHA256()))
        self.fields["sig"] = base64.urlsafe_b64encode(signature).rstrip(b"=").decode("ascii")
        self.base = "https://checkin.example.test/checkin"

    def check(self, fields=None, now=1793770001, suffix="", pem=None):
        return verify_qr(self.base + "?" + urlencode(fields or self.fields) + suffix,
                         pem or self.pem, device_id="7ce8b1b17e60", base_url=self.base,
                         expected_ttl=30, now=now)

    def test_valid_signature(self):
        self.assertEqual(self.check()["issued_at"], 1793770000)

    def test_last_valid_second(self):
        self.check(now=1793770029)

    def test_expiry_boundary_rejected(self):
        with self.assertRaises(ValueError): self.check(now=1793770030)

    def test_future_rejected(self):
        with self.assertRaises(ValueError): self.check(now=1793769999)

    def test_timestamp_tampering_rejected(self):
        fields = dict(self.fields, iat="1793770001")
        with self.assertRaises(Exception): self.check(fields)

    def test_duplicate_parameter_rejected(self):
        with self.assertRaises(ValueError): self.check(suffix="&iat=1793770000")

    def test_ttl_policy_rejected(self):
        with self.assertRaises(ValueError): self.check(dict(self.fields, ttl="60"))

    def test_other_device_rejected(self):
        with self.assertRaises(ValueError): self.check(dict(self.fields, dev="000000000000"))

    def test_untrusted_key_rejected(self):
        other = ec.generate_private_key(ec.SECP256R1()).public_key()
        pem = other.public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo)
        with self.assertRaises(ValueError): self.check(pem=pem)

    def test_invalid_base64_rejected(self):
        with self.assertRaises(ValueError): self.check(dict(self.fields, sig="not+a+signature"))

    def test_hash_router_event_url(self):
        url = "https://vnfam.999.vn/?" + urlencode(self.fields) + "#/"
        checked = verify_qr(url, self.pem, device_id="7ce8b1b17e60",
                            base_url="https://vnfam.999.vn/#/", expected_ttl=30, now=1793770001)
        self.assertEqual(checked["issued_at"], 1793770000)

    def test_wrong_hash_route_rejected(self):
        url = "https://vnfam.999.vn/?" + urlencode(self.fields) + "#/other"
        with self.assertRaises(ValueError):
            verify_qr(url, self.pem, device_id="7ce8b1b17e60",
                      base_url="https://vnfam.999.vn/#/", expected_ttl=30, now=1793770001)


if __name__ == "__main__":
    unittest.main()

