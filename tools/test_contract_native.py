import base64
import ctypes
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
v=json.loads((root/'docs/contract-vectors.json').read_text())
dll=ctypes.CDLL(str(root/'.pio/contract-native.dll'))
dll.canonical.argtypes=[ctypes.c_char_p,ctypes.c_size_t,ctypes.c_char_p,ctypes.c_uint32]
dll.canonical.restype=ctypes.c_bool
dll.rawSignature.argtypes=[ctypes.c_char_p,ctypes.c_size_t,ctypes.c_void_p]
dll.rawSignature.restype=ctypes.c_bool
out=ctypes.create_string_buffer(64)
assert dll.roundedTime(v['t']+29,30)==v['t']
assert dll.roundedTime(v['t']+30,30)==v['t']+30
assert dll.canonical(out,64,v['did'].encode(),v['t']) and out.value.decode()==v['message']
assert not dll.canonical(out,4,v['did'].encode(),v['t'])
assert not dll.canonical(out,64,b'bad_ID',v['t'])
der=base64.urlsafe_b64decode(v['signatureDer']+'==')
raw=(ctypes.c_uint8*64)()
assert dll.rawSignature(der,len(der),raw)
assert base64.urlsafe_b64encode(bytes(raw)).rstrip(b'=').decode()==v['sig']
assert not dll.rawSignature(der+b'\0',len(der)+1,raw)
print('PASS: shared fixture matches exact firmware timestamp/message/DER-to-P1363 implementation.')
