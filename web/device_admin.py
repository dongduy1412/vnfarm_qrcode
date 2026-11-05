"""Local operator relay; the admin token never reaches the browser/firmware."""
import json
from pathlib import Path
import urllib.error
import urllib.request

BASE_URL='https://vnfarm-qrcode-device-link.dong141220047.workers.dev'
DEVICE_ID='7ce8b1b17e60'
COMMANDS=('reconnect_wifi','forget_current_wifi','wipe_wifi')
TOKEN_FILE=Path(__file__).resolve().parents[1]/'cloudflare/.runtime/admin-token.txt'
class AdminUnavailable(RuntimeError):pass
class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*args):return None

def remote_wifi_request(command=None):
    if command is not None and command not in COMMANDS:raise ValueError('Invalid command')
    try:token=TOKEN_FILE.read_text(encoding='ascii').strip()
    except OSError:raise AdminUnavailable('Admin credentials are not configured') from None
    if not 32<=len(token)<=256:raise AdminUnavailable('Invalid admin configuration')
    data=json.dumps({'command':command}).encode() if command is not None else None
    request=urllib.request.Request(BASE_URL+f'/api/admin/checkin/devices/{DEVICE_ID}/commands',data=data,
      headers={'Authorization':'Bearer '+token,'Content-Type':'application/json','User-Agent':'Mozilla/5.0 (VNFARM operator)'})
    try:
        try:response=urllib.request.build_opener(NoRedirect()).open(request,timeout=10)
        except urllib.error.HTTPError as error:response=error
        with response:
            raw=response.read(16385)
            if len(raw)>16384:raise AdminUnavailable('Admin response too large')
            return response.status,json.loads(raw)
    except (OSError,ValueError):raise AdminUnavailable('Cannot contact the device-management Worker') from None
