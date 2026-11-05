"""Send only the non-destructive reconnect command and check real USB/ACK."""
import sys
import time
from pathlib import Path
import serial
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'web'))
from device_admin import remote_wifi_request

def public_key(board):
    board.write(b'QR_INFO\n')
    deadline=time.monotonic()+8
    while time.monotonic()<deadline:
        line=board.readline().decode(errors='replace').strip()
        if line.startswith('QR public key: '):return line.split(': ',1)[1]
    raise RuntimeError('QR public key unavailable')

with serial.Serial('COM5',115200,timeout=.25) as board:
    before=public_key(board)
    code,command=remote_wifi_request('reconnect_wifi')
    assert code in (200,202),(code,command)
    command_id=command['cmdId']
    print('Queued reconnect_wifi:',command_id,flush=True)
    applied=restarted=connected=False
    deadline=time.monotonic()+110
    while time.monotonic()<deadline:
        try:line=board.readline().decode(errors='replace').strip()
        except serial.SerialException:
            board.close();time.sleep(1)
            try:board.open()
            except serial.SerialException:continue
            continue
        if line and not line.startswith(('QR test URL:','QR public key:')):print(line,flush=True)
        applied|='Device command applied: reconnect_wifi' in line
        restarted|='Remote Wi-Fi command acknowledged; restarting' in line
        if restarted and 'DO pong verified:' in line:
            connected=True;break
    assert applied and restarted and connected,'Command/reboot/reconnection not all observed'
    assert public_key(board)==before,'Public key changed'
code,history=remote_wifi_request()
assert code==200 and any(c['cmdId']==command_id and c['acked'] for c in history['commands'])
print('PASS: live reconnect, server ACK, reboot, Wi-Fi/cloud recovery, unchanged key.')
