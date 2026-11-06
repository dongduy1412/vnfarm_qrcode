"""Read-only USB monitor. Does not reset device or print QR signatures/tokens."""
import argparse
import time
import serial

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--port', default='COM5')
a = p.parse_args()
board = serial.Serial()
board.port = a.port
board.baudrate = 115200
board.timeout = .3
board.dtr = False
board.rts = False
board.open()
try:
    last = 0
    while True:
        if time.monotonic() - last > 10:
            board.write(b'DEVICE_INFO\n')
            last = time.monotonic()
        line = board.readline().decode(errors='replace').strip()
        if line and not line.startswith(('QR test URL:', 'QR public key:')):
            print(line, flush=True)
except KeyboardInterrupt:
    pass
finally:
    board.close()
