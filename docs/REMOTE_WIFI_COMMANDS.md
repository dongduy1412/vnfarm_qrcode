# Remote Wi-Fi commands — firmware 0.3.0

| Command | Saved networks | Next behavior |
|---|---|---|
| `reconnect_wifi` | Keep all (up to 5) | Restart and reconnect |
| `forget_current_wifi` | Remove the connected SSID only | Try remaining profiles; open setup if none reachable |
| `wipe_wifi` | Remove all | Restart into setup |

All commands preserve the signing key. This is **not factory reset**.
Forget fallback waits 15 seconds per remaining profile plus 5 seconds before
entering setup. Ordinary Wi-Fi drops still retry saved networks.

## Operator flow

Open http://127.0.0.1:8765/ on the operator PC. The Wi-Fi control card relays
commands through the test Worker. Phone/LAN clients cannot use this relay.
Alternatively use `python tools/device_command.py <command>`; omit the argument
to list history. Only one unacknowledged command may be queued per device.
The same pending command returns its existing ID; a different command gets 409.

Admin routes: GET/POST `/api/admin/checkin/devices/{did}/commands`.
POST body: `{"command":"wipe_wifi"}` (or either extension above).
Bearer authentication is required even in TEST_MODE; browser sessions do not
authorize management. The token stays in ignored
`cloudflare/.runtime/admin-token.txt`, never in browser or firmware.
`python tools/configure_device_admin.py` prepares this file and
`.runtime/admin-secrets.json`. From `cloudflare`, deploy with
`npx wrangler deploy --secrets-file .runtime/admin-secrets.json`.
Preserve these ignored operator credentials securely outside Git.

## Contract and recovery

- Firmware polls every 60 seconds over verified HTTPS to its build-time host.
- POST `/api/device/poll`: `{v:1,did,t,sig}`, signed message
  `v1.poll.<did>.<t>`.
- Empty response: `{command:null}`; command response: `{command,cmdId,t}`.
  `t` is refreshed on each delivery; queued work survives offline periods.
- The contract leaves server authentication undecided. This implementation
  chooses its verified HTTPS/fixed-host option, with redirects disabled;
  there is no additional server signature field.
- POST `/api/device/ack`: `{v:1,did,cmdId,t,sig}`, signed message
  `v1.ack.<did>.<cmdId>.<t>`, successful response 204. This explicitly defines
  the ACK signing string omitted from the mentor contract.
- Poll and ACK use ECDSA P-256/SHA-256, raw `r||s` base64url signatures.
  Server accepts age at most 60 seconds and future skew at most 5 seconds.
- Firmware records intent and target SSID in NVS before modifying profiles,
  marks completion before ACK, then attempts ACK over the existing connection
  before reboot (12-second deadline). Pending ACK retries every 10 seconds
  after connectivity returns. Wipe may require setup before this retry.
- An interrupted operation resumes before connection attempts. Completed IDs
  (last 8) prevent redelivery from removing newly configured networks.
  DO command history survives Worker restarts, with up to 100 stored entries.

## Verification, 2026-11-05

- 39 Python tests, TypeScript check and isolated DO HTTP integration passed.
- Actual SavedNetworks/WifiCommands code passed 42 simulated power-loss cases,
  including interrupted NVS writes, lost ACK and replay after new setup.
- Browser tests covered all three commands with mocked APIs, conflict feedback,
  mobile/desktop layout and hiding controls for LAN clients.
- Firmware 0.3.0 flashed to COM5 with verified hashes and watchdog reset.
- **Physical test:** reconnect command delivered, ACK stored, device restarted,
  both saved profiles retained, Wi-Fi/cloud connection recovered, public key
  unchanged. Actual displayed QR verified by deployed Worker and local lab.
- Forget and wipe were tested in isolated storage/server simulations, without
  deleting the user's live credentials during hardware verification.

Deployment remains on the personal **test Worker**. Project Worker/event-app
integration and factory reset are separate unfinished tasks.
