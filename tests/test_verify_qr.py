import json
import sys
import unittest
from pathlib import Path
from urllib.parse import urlencode
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from verify_qr import verify_qr, b64url, ContractError

class VerificationTests(unittest.TestCase):
    def setUp(self):
        self.vector=json.loads((Path(__file__).resolve().parents[1]/'docs/contract-vectors.json').read_text())
        self.key=serialization.load_der_private_key(__import__('base64').urlsafe_b64decode(self.vector['privateKeyPkcs8']+'=='),None)
        self.public=self.vector['publicKey']; self.did='door-1'; self.t=self.vector['t']
        self.base='https://checkin.example.test/checkin'

    def signed(self, t=None, did=None):
        t=self.t if t is None else t; did=did or self.did
        der=self.key.sign(f'v1.{did}.{t}'.encode(),ec.ECDSA(hashes.SHA256()))
        r,s=utils.decode_dss_signature(der)
        return {'v':'1','did':did,'t':str(t),'sig':b64url(r.to_bytes(32,'big')+s.to_bytes(32,'big'))}

    def check(self, fields=None, age=5, suffix='', public=None):
        return verify_qr(self.base+'?'+urlencode(fields or self.signed())+suffix,public or self.public,
            device_id=self.did,base_url=self.base,expected_ttl=60,skew=5,now=self.t+age)

    def reject(self, status, fields=None, **kwargs):
        with self.assertRaises(ContractError) as error: self.check(fields,**kwargs)
        self.assertEqual(error.exception.status,status)

    def test_shared_vector(self):
        self.assertEqual(verify_qr(self.vector['url'],self.public,device_id=self.did,base_url=self.base,now=self.t+5)['issued_at'],self.t)
    def test_valid(self): self.assertEqual(self.check()['issued_at'],self.t)
    def test_ttl_boundary_accepted(self): self.check(age=60)
    def test_expired(self): self.reject(410,age=61)
    def test_skew_boundary_accepted(self): self.check(age=-5)
    def test_future(self): self.reject(400,age=-6)
    def test_tampered_signature(self):
        f=self.signed(); f['sig']=('A' if f['sig'][0]!='A' else 'B')+f['sig'][1:]; self.reject(401,f)
    def test_tampered_timestamp(self):
        f=self.signed(); f['t']=str(self.t+1); self.reject(401,f)
    def test_unknown_device(self): self.reject(404,self.signed(did='door-2'))
    def test_missing_signature(self):
        f=self.signed(); del f['sig']; self.reject(400,f)
    def test_duplicate(self): self.reject(400,suffix='&t='+str(self.t))
    def test_extra_ttl_not_allowed(self): self.reject(400,dict(self.signed(),ttl='60'))
    def test_old_protocol_not_allowed(self): self.reject(400,{'v':'1','dev':'7ce8b1b17e60','iat':str(self.t),'kid':'0'*64,'ttl':'30','sig':'A'*86})
    def test_invalid_base64(self): self.reject(400,dict(self.signed(),sig='bad+base64'))
    def test_padding_rejected(self):
        f=self.signed(); f['sig']+='='; self.reject(400,f)
    def test_der_signature_rejected(self): self.reject(400,dict(self.signed(),sig=self.vector['signatureDer']))
    def test_wrong_key(self):
        other=ec.generate_private_key(ec.SECP256R1()).public_key()
        self.reject(401,public=b64url(other.public_bytes(serialization.Encoding.X962,serialization.PublicFormat.UncompressedPoint)))
    def test_bad_device_id(self): self.reject(400,dict(self.signed(),did='DOOR_1'))
    def test_leading_zero_time(self): self.reject(400,dict(self.signed(),t='0'+str(self.t)))
    def test_fragment_rejected(self): self.reject(400,suffix='#/')
    def test_wrong_host(self):
        with self.assertRaises(ContractError): verify_qr(self.vector['url'],self.public,device_id=self.did,base_url='https://other.test/checkin',now=self.t+5)

if __name__=='__main__': unittest.main()
