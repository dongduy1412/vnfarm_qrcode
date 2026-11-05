"""Capture real boot/NTP evidence. Start capture, then press RST (not BOOT).

Does not reset, flash, change Wi-Fi, or block network traffic itself.
For recovery, block outbound UDP/123 on the BOARD'S router before boot;
keep Wi-Fi/Internet available, wait for 'QR blocked', then unblock UDP/123.
"""
import argparse
import time
from datetime import datetime
from pathlib import Path
import serial
from ntp_evidence import sanitize, validate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='COM5')
    parser.add_argument('--mode', choices=('boot', 'recovery', 'periodic'), default='boot')
    parser.add_argument('--seconds', type=float, help='Capture duration; periodic default 3900s')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    seconds = args.seconds if args.seconds is not None else (3900 if args.mode == 'periodic' else 180)
    if seconds <= 0:
        parser.error('--seconds must be positive')
    output = args.output or Path(__file__).resolve().parents[1] / 'web/.runtime' / (
        f'ntp-{args.mode}-{datetime.now():%Y%m%d-%H%M%S}.log')
    output.parent.mkdir(parents=True, exist_ok=True)
    lines = []
    print('Start capture BEFORE pressing RST. Do not hold BOOT. Log:', output, flush=True)
    with serial.Serial(args.port, 115200, timeout=.25) as board, output.open('w', encoding='utf-8') as log:
        deadline = time.monotonic() + seconds
        try:
            while time.monotonic() < deadline:
                line = board.readline().decode(errors='replace').strip()
                if not line:
                    continue
                safe = sanitize(line)
                lines.append(safe)
                log.write(safe + '\n')
                log.flush()
                print(safe, flush=True)
        except KeyboardInterrupt:
            print('Capture stopped; checking evidence collected so far.')
    try:
        print(validate(lines, args.mode))
    except ValueError as error:
        raise SystemExit(f'NOT VERIFIED: {error} Log: {output}')


if __name__ == '__main__':
    main()
