"""Encode the user-supplied logo as a small RGB565 firmware asset."""
from pathlib import Path
from PIL import Image
import sys

root = Path(__file__).resolve().parents[1]
size = 80
image = Image.open(sys.argv[1]).convert('RGB').resize((size, size), Image.Resampling.LANCZOS)
values = [(r >> 3) << 11 | (g >> 2) << 5 | (b >> 3) for r, g, b in image.getdata()]
lines = ['#pragma once', '#include <stdint.h>', f'constexpr int QR_LOGO_SIZE = {size};', 'static const uint16_t qrLogo[] = {']
lines += [', '.join(f'0x{v:04x}' for v in values[i:i+16]) + ',' for i in range(0, len(values), 16)]
lines += ['};', '']
(root / 'include' / 'QrLogo.h').write_text('\n'.join(lines))
