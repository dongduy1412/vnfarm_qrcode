# ESP32 ↔ Cloudflare Durable Object connection test

## Current: contract-v1 QR check-in (firmware 0.2.0)

The Worker now also implements `/checkin` and `/api/checkin/status` using
P-256/SHA-256, message `v1.did.t`, raw public key and raw 64-byte signature
encoded as unpadded base64url. QR TTL=60 and SKEW=5 are server settings.
See [protocol](../docs/QR_PROTOCOL.md) and [migration](../docs/CONTRACT_MIGRATION.md).

This deployed standalone Worker has `TEST_MODE=true`. Open `/login` and start
an anonymous test session, then scan a fresh board QR. Unauthenticated scans
redirect to login with the return URL; valid signed scans return 302 `/stamp`.
`/stamp` is an explicitly labelled test result page and awards no real stamps.
The DO persists per-user idempotent check-ins and applies per-IP/user request
limits. Session tokens are random, stored hashed, HttpOnly/Secure/SameSite.

Production must replace the test session adapter with the event app's existing
authenticated user and connect its real stamp route/APIs/CMS. The event app
source has not been supplied. `CHECKIN_VALID_SECONDS=1800` is a test choice,
not a finalized event policy. Test sessions cannot impersonate event accounts.
No remote reset/key rotation commands or admin CMS are added by this change.

The sections below document the separate original heartbeat demo, which still
works unchanged. Its SPKI/DER key/signature format is **not** the QR format.

Deployed Worker: https://vnfarm-qrcode-device-link.dong141220047.workers.dev

Read-only device status:
https://vnfarm-qrcode-device-link.dong141220047.workers.dev/api/devices/7ce8b1b17e60/status

Firmware 0.1.9 sends a signed HTTPS POST every 30 seconds to:

```text
https://vnfarm-qrcode-device-link.dong141220047.workers.dev/api/devices/7ce8b1b17e60/ping
```

This is a connection/heartbeat demo using HTTPS polling, not a persistent
WebSocket or a remote-command implementation. The check-in QR still opens the
event website, independently of this endpoint.

## Flow

1. The board connects to saved Wi-Fi and obtains NTP time.
2. The loop signs a ping with the existing private key; a background task sends
   it over TLS with CA/hostname verification. QR drawing continues separately.
3. The Worker checks the registered public key, key fingerprint, device ID,
   timestamp (within 60 seconds), message fields and signature.
4. The Worker routes the message to `DEVICES.getByName(device_id)` using RPC.
5. The SQLite-backed `DeviceLink` verifies again, rejects recently used nonces,
   persists its counter/last-seen state and returns `pong` with the nonce.
6. The board validates the HTTP response, message type, nonce and device ID and
   prints `DO pong verified: sequence=... server_time=...` over USB.

`online` means a valid signed ping was received within the last 90 seconds.
It is derived from last-seen time, not a live WebSocket connection. The status
endpoint is intentionally public and read-only for this demo; it contains no
private key or Wi-Fi credentials. `/health` checks the Worker only, whereas
the device status and signed ping/pong exercise the DO.

## Ping contract

JSON fields: `device_id`, `kid`, `iat` (Unix seconds), `nonce` (16 random bytes as
32 lowercase hex characters), `uptime` (seconds), `sig`.

The exact signed ASCII bytes, separated by LF with no trailing newline:

```text
ping-v1
<device_id>
<kid>
<iat>
<nonce>
<uptime>
```

Sign with ECDSA P-256/SHA-256, DER signature encoded as unpadded base64url.
This domain-separated payload is different from the check-in QR signature.
Only the registered public key is in `wrangler.jsonc`; private keys stay on
the board. The CA trust anchor is public certificate data in `CloudRootCA.h`.

## Development and deployment

```powershell
cd cloudflare
npm ci
npm run types
npm run check
npx wrangler deploy --dry-run
npm run deploy
```

Use the intended account in `wrangler.jsonc`; account IDs and public keys are
configuration, not credentials. Authenticate with Wrangler locally; do not put
Cloudflare OAuth/API tokens in source. The compatibility date is 2026-11-04,
the latest date accepted by Cloudflare at deployment (local time was Nov 5).

From the project root, `python tools/test_do.py` starts an isolated local
Workers runtime/SQLite DO using a disposable signing key. It checks signed
ping/pong, increasing state, replay, tampering, stale timestamps and oversized
requests. It does not use the hardware private key or change deployed trust.
Install the Python requirements and `npm ci` before running it.

`QR_CLOUD_BASE_URL` can override the firmware endpoint at build time. If moving
to another host, update its verified CA anchor with
`python tools/generate_cloud_ca.py <hostname>` before building. Trust anchor
rotation may require a future firmware update; TLS verification is enabled.

## Verified on real hardware — Nov 5, 2026 (Asia/Saigon)

- Worker deployment and SQLite DO migration succeeded.
- Firmware 0.1.9 uploaded to ESP32-S3 `7ce8b1b17e60`, COM5; flash hashes passed.
- Actual USB output: `DO pong verified: sequence=3 server_time=1793813579`.
- Remote status simultaneously returned `online: true`, `sequence: 3`,
  `uptime: 66`, and the unchanged registered key fingerprint.
- QR refresh log continued with the event URL and 30-second lifetime.
- Local DO integration checks, TypeScript check and existing 23 Python tests
  passed.

Remote key rotation, Wi-Fi reset commands, management authentication/UI and
production event check-in integration remain separate unfinished work.
