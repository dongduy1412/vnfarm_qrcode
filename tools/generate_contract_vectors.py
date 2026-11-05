"""Generate disposable, explicitly non-production P-256 protocol fixtures."""
import base64
import json
from pathlib import Path
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils

def b64(data): return base64.urlsafe_b64encode(data).rstrip(b'=').decode()
key = ec.generate_private_key(ec.SECP256R1())
did, t = 'door-1', 1793814000
message = f'v1.{did}.{t}'
der = key.sign(message.encode(), ec.ECDSA(hashes.SHA256()))
r,s = utils.decode_dss_signature(der)
sig = b64(r.to_bytes(32,'big')+s.to_bytes(32,'big'))
fixture = {
 'warning':'TEST ONLY: deliberately published fixture private key. Never provision a real device with this key.',
 'algorithm':'ecdsa-p256-sha256','publicKeyFormat':'x962-uncompressed-base64url','signatureFormat':'ieee-p1363-base64url',
 'privateKeyPkcs8':b64(key.private_bytes(serialization.Encoding.DER,serialization.PrivateFormat.PKCS8,serialization.NoEncryption())),
 'publicKey':b64(key.public_key().public_bytes(serialization.Encoding.X962,serialization.PublicFormat.UncompressedPoint)),
 'did':did,'t':t,'N':30,'TTL':60,'SKEW':5,'message':message,'sig':sig,'signatureDer':b64(der),
 'url':f'https://checkin.example.test/checkin?v=1&did={did}&t={t}&sig={sig}',
 'cases':[
   {'name':'valid','now':t+5,'expectedStatus':302},
   {'name':'tampered_signature','now':t+5,'expectedStatus':401},
   {'name':'expired','now':t+61,'expectedStatus':410},
   {'name':'future','now':t-6,'expectedStatus':400},
   {'name':'unknown_device','now':t+5,'expectedStatus':404},
   {'name':'missing_signature','now':t+5,'expectedStatus':400},
   {'name':'same_user_repeat','now':t+5,'expectedStatus':302,'newCheckin':False}
 ]}
path=Path(__file__).resolve().parents[1]/'docs'/'contract-vectors.json'
path.write_text(json.dumps(fixture,indent=2)+'\n')
print('Wrote test-only contract vectors; fixture key is not the device key.')
