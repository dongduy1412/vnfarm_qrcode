"""Capture enrollment/BOOT logs; never request or save device credentials."""
import time
from pathlib import Path
import serial
lines=[]
with serial.Serial('COM5',115200,timeout=.25) as board:
    until=time.monotonic()+55
    while time.monotonic()<until:
        line=board.readline().decode(errors='replace').strip()
        if line and not line.startswith(('QR test URL:','QR public key:')):
            print(line,flush=True);lines.append(line)
        if 'QR refreshed:' in line and any('WS BOOT acknowledged' in x for x in lines):break
Path('web/.runtime/project-boot.log').write_text('\n'.join(lines),encoding='utf-8')
print('BOOT_ACK_OBSERVED',any('WS BOOT acknowledged' in x for x in lines))
