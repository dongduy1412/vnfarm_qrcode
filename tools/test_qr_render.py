"""Decode images from the exact C++ renderer used on the LCD.

Requires the native DLL built from qr_preview_native.cpp, numpy, Pillow,
cryptography and zxing-cpp. Never uses or exports the device private key.
"""
import base64
import ctypes
from pathlib import Path
import random
import numpy as np
from PIL import Image, ImageFilter
import zxingcpp
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec

root = Path(__file__).resolve().parents[1]
dll = ctypes.CDLL(str(root / '.pio' / 'qr-preview.dll'))
dll.renderPreview.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_uint16)]
dll.renderPreview.restype = ctypes.c_int
key = ec.generate_private_key(ec.SECP256R1())
rng = random.Random(42)
count = 0
for i in range(36):
    base = 'https://vnfam.999.vn/' if i < 24 else 'https://example.com/' + 'x' * (45, 75, 141)[i % 3]
    iat = 1793780000 + i * 30
    kid = ''.join(rng.choice('0123456789abcdef') for _ in range(64))
    payload = f'v1\n7ce8b1b17e60\n{kid}\n{iat}\n30'.encode()
    signature = base64.urlsafe_b64encode(key.sign(payload, ec.ECDSA(hashes.SHA256()))).rstrip(b'=').decode()
    text = f'{base}?v=1&dev=7ce8b1b17e60&kid={kid}&iat={iat}&ttl=30&sig={signature}#/'
    pixels = (ctypes.c_uint16 * (240 * 240))()
    assert dll.renderPreview(text.encode(), pixels), len(text)
    p = np.ctypeslib.as_array(pixels).reshape(240, 240).astype(np.uint32)
    rgb = np.stack([((p >> 11) & 31) * 255 // 31, ((p >> 5) & 63) * 255 // 63, (p & 31) * 255 // 31], axis=-1).astype(np.uint8)
    image = Image.fromarray(rgb)
    if i == 0:
        (root / 'docs' / 'previews').mkdir(exist_ok=True)
        image.save(root / 'docs' / 'previews' / 'qr-branded-240.png')
        image.resize((960, 960), Image.Resampling.NEAREST).save(root / 'docs' / 'previews' / 'qr-branded-large.png')
    for variant in (image, image.rotate(90), image.filter(ImageFilter.GaussianBlur(0.35))):
        result = zxingcpp.read_barcode(variant)
        assert result and result.text == text, f'Decode failed: case {i}, {len(text)} bytes'
        count += 1
assert not dll.renderPreview(b'x' * 383, pixels), 'Oversize input must fail'
print(f'PASS: {count} QR decodes, real signed sample URLs, versions 17-20, oversize rejected.')
