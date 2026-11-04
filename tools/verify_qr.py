"""Offline reference verifier; not a check-in backend or an HTTP endpoint."""
import argparse
import base64
import hashlib
import re
import time
from urllib.parse import parse_qsl, urlsplit

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec


def verify_qr(url, public_pem, *, device_id, base_url, expected_ttl, now=None):
    parsed = urlsplit(url)
    expected = urlsplit(base_url)
    if expected.query or (parsed.scheme, parsed.netloc, parsed.path, parsed.fragment) != (
            expected.scheme, expected.netloc, expected.path, expected.fragment):
        raise ValueError("Wrong check-in URL")
    pairs = parse_qsl(parsed.query, keep_blank_values=True, strict_parsing=True)
    fields = dict(pairs)
    if len(pairs) != len(fields) or set(fields) != {"v", "dev", "kid", "iat", "ttl", "sig"}:
        raise ValueError("Missing, unexpected or duplicate parameters")
    if fields["v"] != "1" or fields["dev"] != device_id:
        raise ValueError("Untrusted version/device")
    if not re.fullmatch(r"[0-9a-f]{12}", device_id):
        raise ValueError("Bad device ID")
    if not re.fullmatch(r"[1-9][0-9]{0,10}", fields["iat"]):
        raise ValueError("Bad timestamp")
    if not 10 <= expected_ttl <= 60 or fields["ttl"] != str(expected_ttl):
        raise ValueError("Unexpected TTL; use server policy, not an arbitrary client TTL")
    public_key = serialization.load_pem_public_key(public_pem)
    if not isinstance(public_key, ec.EllipticCurvePublicKey) or not isinstance(public_key.curve, ec.SECP256R1):
        raise ValueError("Expected P-256 key")
    der = public_key.public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
    if fields["kid"] != hashlib.sha256(der).hexdigest():
        raise ValueError("Public key does not match fingerprint")
    if not re.fullmatch(r"[A-Za-z0-9_-]+", fields["sig"]):
        raise ValueError("Bad signature encoding")
    signature = base64.b64decode(fields["sig"] + "=" * (-len(fields["sig"]) % 4), altchars=b"-_", validate=True)
    canonical = "\n".join(["v1", fields["dev"], fields["kid"], fields["iat"], fields["ttl"]]).encode("ascii")
    public_key.verify(signature, canonical, ec.ECDSA(hashes.SHA256()))
    now = int(time.time()) if now is None else now
    issued = int(fields["iat"])
    if issued > now or now - issued >= expected_ttl:
        raise ValueError("QR is from the future or has expired")
    return {"device_id": device_id, "issued_at": issued, "expires_at": issued + expected_ttl}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("url")
    parser.add_argument("--public-key", required=True)
    parser.add_argument("--device-id", required=True)
    parser.add_argument("--base-url", required=True)
    parser.add_argument("--ttl", type=int, required=True)
    args = parser.parse_args()
    with open(args.public_key, "rb") as handle:
        pem = handle.read()
    try:
        print(verify_qr(args.url, pem, device_id=args.device_id, base_url=args.base_url, expected_ttl=args.ttl))
    except Exception as exc:
        parser.exit(1, f"QR rejected: {type(exc).__name__}: {exc}\n")

