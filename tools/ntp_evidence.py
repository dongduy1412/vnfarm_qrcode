"""Pure log checks. These checks require real device logs, not simulated evidence."""
import re


def sanitize(line):
    if line.startswith(('QR test URL:', 'QR public key:')):
        return line.split(':', 1)[0] + ': [omitted]'
    return line


def validate(lines, mode='boot'):
    boots = [i for i, line in enumerate(lines) if 'QR firmware ' in line]
    if not boots:
        raise ValueError('Missing boot banner: start capture BEFORE pressing RST.')
    start = boots[-1]
    if mode != 'boot' and len(boots) != 1:
        raise ValueError('Unexpected reboot during recovery/periodic observation.')
    current = lines[start:]
    syncs = []
    qrs = []
    for i, line in enumerate(current):
        if 'NTP sync confirmed:' in line:
            match = re.search(r'epoch=(\d+) uptime_ms=(\d+)', line)
            if match:
                syncs.append((i, int(match[1]), int(match[2])))
        if 'QR refreshed:' in line:
            qrs.append(i)
    if not syncs or not qrs or syncs[0][0] >= qrs[0]:
        raise ValueError('Need actual NTP confirmation before FIRST QR in this boot.')
    if mode == 'recovery':
        blocked = [i for i, line in enumerate(current) if 'NTP not synchronized; QR blocked' in line]
        if not blocked or blocked[0] >= syncs[0][0]:
            raise ValueError('Need blocked-NTP observation before recovery and first sync.')
    if mode == 'periodic':
        if len(syncs) < 2:
            raise ValueError('Need two actual sync notifications in the SAME boot.')
        # Do not count duplicate/nearby retry callbacks as an hourly resync.
        if not any(epoch - syncs[0][1] >= 3540 and uptime - syncs[0][2] >= 3540000
                   for _, epoch, uptime in syncs[1:]):
            raise ValueError('No observed hourly resync (minimum elapsed 59 minutes).')
    return f'PASS: {mode}: NTP before first QR; sync notifications={len(syncs)}'
