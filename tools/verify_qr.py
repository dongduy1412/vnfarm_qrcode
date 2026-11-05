"""Contract v1: P-256/SHA-256, raw uncompressed key, P1363 signature."""
import argparse
import base64
import re
import time
from urllib.parse import parse_qsl, urlsplit
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives.asymmetric import ec, utils
from cryptography.hazmat.primitives import hashes

class ContractError(ValueError):
    def __init__(self, code, status=400):
        super().__init__(code)
        self.code, self.status = code, status

def b64url(data):
    return base64.urlsafe_b64encode(data).rstrip(b'=').decode('ascii')

def decode64(text):
    if not isinstance(text, str) or not re.fullmatch(r'[A-Za-z0-9_-]+', text):
        raise ContractError('bad_request')
    try:
        result = base64.b64decode(text + '=' * (-len(text) % 4), altchars=b'-_', validate=True)
    except ValueError:
        raise ContractError('bad_request') from None
    if b64url(result) != text: raise ContractError('bad_request')
    return result

def load_raw_key(text):
    raw = decode64(text.strip() if isinstance(text, str) else '')
    if len(raw) != 65 or raw[0] != 4: raise ContractError('bad_request')
    try: return ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), raw)
    except ValueError: raise ContractError('bad_request') from None

def parse_qr(url, base_url):
    if not isinstance(url, str) or len(url) > 2048: raise ContractError('bad_request')
    parsed, expected = urlsplit(url), urlsplit(base_url)
    if expected.query or expected.fragment or parsed.fragment or expected.path != '/checkin' or (
        parsed.scheme, parsed.netloc, parsed.path) != (expected.scheme, expected.netloc, expected.path):
        raise ContractError('bad_request')
    try: pairs = parse_qsl(parsed.query, keep_blank_values=True, strict_parsing=True)
    except ValueError: raise ContractError('bad_request') from None
    fields = dict(pairs)
    if len(pairs) != 4 or len(fields) != 4 or set(fields) != {'v','did','t','sig'}: raise ContractError('bad_request')
    if fields['v'] != '1' or not re.fullmatch(r'[a-z0-9-]{1,32}', fields['did']) or not re.fullmatch(r'(0|[1-9][0-9]{0,10})', fields['t']):
        raise ContractError('bad_request')
    if len(decode64(fields['sig'])) != 64: raise ContractError('bad_request')
    return fields

def verify_qr(url, public_key, *, device_id, base_url, expected_ttl=60, skew=5, now=None):
    fields = parse_qr(url, base_url)
    if fields['did'] != device_id: raise ContractError('unknown_device', 404)
    if expected_ttl not in (10,20,30,60) or not isinstance(skew, int) or not 0 <= skew <= 60:
        raise ValueError('Invalid server TTL/SKEW configuration')
    key = load_raw_key(public_key)
    signature = decode64(fields['sig'])
    der = utils.encode_dss_signature(int.from_bytes(signature[:32], 'big'), int.from_bytes(signature[32:], 'big'))
    try: key.verify(der, f'v1.{fields["did"]}.{fields["t"]}'.encode('ascii'), ec.ECDSA(hashes.SHA256()))
    except InvalidSignature: raise ContractError('invalid_signature', 401) from None
    issued = int(fields['t'])
    age = (int(time.time()) if now is None else now) - issued
    if age > expected_ttl: raise ContractError('expired', 410)
    if age < -skew: raise ContractError('bad_request', 400)
    return {'device_id': device_id, 'issued_at': issued, 'expires_at': issued + expected_ttl}

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('url')
    parser.add_argument('--public-key', required=True, help='File containing raw base64url public key')
    parser.add_argument('--device-id', required=True)
    parser.add_argument('--base-url', required=True)
    parser.add_argument('--ttl', type=int, default=60)
    parser.add_argument('--skew', type=int, default=5)
    args = parser.parse_args()
    try:
        with open(args.public_key, encoding='ascii') as handle: public = handle.read().strip()
        print(verify_qr(args.url, public, device_id=args.device_id, base_url=args.base_url, expected_ttl=args.ttl, skew=args.skew))
    except (ValueError, OSError) as exc: parser.exit(1, f'QR rejected: {exc}\n')
