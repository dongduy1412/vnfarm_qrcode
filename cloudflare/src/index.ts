import { DurableObject } from 'cloudflare:workers';
import { checkinHttp } from './checkin-http';

type Ping = { device_id: string; kid: string; iat: number; nonce: string; uptime: number; sig: string };
type LinkState = { sequence: number; last_seen: number; uptime: number; nonce: string; kid: string };
const reply = (body: unknown, status = 200) => Response.json(body, {
  status, headers: { 'Cache-Control': 'no-store' },
});
const decode = (text: string) => Uint8Array.from(atob(text.replace(/-/g, '+').replace(/_/g, '/')), c => c.charCodeAt(0));

// mbedTLS signs DER. Web Crypto ECDSA verifies fixed-width IEEE P1363 r||s.
function derToRaw(der: Uint8Array): Uint8Array {
  if (der.length < 8 || der.length > 72 || der[0] !== 0x30 || der[1] !== der.length - 2) throw Error('DER');
  let offset = 2;
  const raw = new Uint8Array(64);
  for (let i = 0; i < 2; i++) {
    if (der[offset++] !== 2) throw Error('DER integer');
    const length = der[offset++];
    if (length < 1 || length > 33 || offset + length > der.length || der[offset] & 0x80) throw Error('DER length');
    let integer = der.slice(offset, offset + length);
    offset += length;
    if (integer.length > 1 && integer[0] === 0) {
      if (!(integer[1] & 0x80)) throw Error('DER padding');
      integer = integer.slice(1);
    }
    if (integer.length > 32) throw Error('DER overflow');
    raw.set(integer, i * 32 + 32 - integer.length);
  }
  if (offset !== der.length) throw Error('DER trailing');
  return raw;
}

async function trustedPing(value: unknown, env: Env): Promise<Ping | null> {
  if (!value || typeof value !== 'object') return null;
  const p = value as Record<string, unknown>;
  if (p.device_id !== env.DEVICE_ID || typeof p.kid !== 'string' || !/^[a-f0-9]{64}$/.test(p.kid) ||
      !Number.isSafeInteger(p.iat) || Math.abs(Date.now() / 1000 - (p.iat as number)) > 60 ||
      !Number.isSafeInteger(p.uptime) || (p.uptime as number) < 0 || (p.uptime as number) > 4294967295 ||
      typeof p.nonce !== 'string' || !/^[a-f0-9]{32}$/.test(p.nonce) ||
      typeof p.sig !== 'string' || !/^[A-Za-z0-9_-]{90,96}$/.test(p.sig)) return null;
  try {
    const der = decode(env.DEVICE_PUBLIC_KEY);
    const hash = new Uint8Array(await crypto.subtle.digest('SHA-256', der));
    const kid = Array.from(hash, b => b.toString(16).padStart(2, '0')).join('');
    if (p.kid !== kid) return null;
    const key = await crypto.subtle.importKey('spki', der, { name: 'ECDSA', namedCurve: 'P-256' }, false, ['verify']);
    const payload = `ping-v1\n${p.device_id}\n${p.kid}\n${p.iat}\n${p.nonce}\n${p.uptime}`;
    if (!await crypto.subtle.verify({ name: 'ECDSA', hash: 'SHA-256' }, key, derToRaw(decode(p.sig)), new TextEncoder().encode(payload))) return null;
    return p as Ping;
  } catch { return null; }
}

export class DeviceLink extends DurableObject<Env> {
  constructor(ctx: DurableObjectState, env: Env) {
    super(ctx, env);
    this.ctx.storage.sql.exec('CREATE TABLE IF NOT EXISTS state (id INTEGER PRIMARY KEY CHECK(id=1), sequence INTEGER NOT NULL, last_seen INTEGER NOT NULL, uptime INTEGER NOT NULL, nonce TEXT NOT NULL, kid TEXT NOT NULL)');
    this.ctx.storage.sql.exec('CREATE TABLE IF NOT EXISTS nonces (nonce TEXT PRIMARY KEY, seen INTEGER NOT NULL)');
    this.ctx.storage.sql.exec('CREATE TABLE IF NOT EXISTS test_sessions (token_hash TEXT PRIMARY KEY, user_id TEXT NOT NULL, expires INTEGER NOT NULL)');
    this.ctx.storage.sql.exec('CREATE TABLE IF NOT EXISTS checkins (user_id TEXT NOT NULL, t INTEGER NOT NULL, expires INTEGER NOT NULL, PRIMARY KEY(user_id,t))');
    this.ctx.storage.sql.exec('CREATE TABLE IF NOT EXISTS checkin_limits (key TEXT PRIMARY KEY, bucket INTEGER NOT NULL, count INTEGER NOT NULL)');
  }

  createTestSession(tokenHash:string,user:string) {
    if(this.env.TEST_MODE!=='true') throw Error('Test sessions disabled');
    const now=Math.floor(Date.now()/1000);
    this.ctx.storage.sql.exec('DELETE FROM test_sessions WHERE expires < ?',now);
    this.ctx.storage.sql.exec('INSERT INTO test_sessions VALUES (?,?,?)',tokenHash,user,now+3600);
  }
  testUser(tokenHash:string):string|null {
    if(this.env.TEST_MODE!=='true') return null;
    return this.ctx.storage.sql.exec<{user_id:string}>('SELECT user_id FROM test_sessions WHERE token_hash=? AND expires>?',tokenHash,Math.floor(Date.now()/1000)).toArray()[0]?.user_id??null;
  }
  allowCheckin(ip:string,user:string):boolean {
    const bucket=Math.floor(Date.now()/60000);
    this.ctx.storage.sql.exec('DELETE FROM checkin_limits WHERE bucket < ?',bucket-1);
    for(const [key,maximum] of [[`ip:${ip}`,60],[`user:${user}`,30]] as const) {
      this.ctx.storage.sql.exec('INSERT INTO checkin_limits VALUES (?,?,1) ON CONFLICT(key) DO UPDATE SET count=CASE WHEN bucket=excluded.bucket THEN count+1 ELSE 1 END,bucket=excluded.bucket',key,bucket);
      if(this.ctx.storage.sql.exec<{count:number}>('SELECT count FROM checkin_limits WHERE key=?',key).one().count>maximum)return false;
    }
    return true;
  }
  recordCheckin(user:string,t:number) {
    const now=Math.floor(Date.now()/1000),expires=now+Number(this.env.CHECKIN_VALID_SECONDS);
    this.ctx.storage.sql.exec('DELETE FROM checkins WHERE expires < ?',now);
    // A repeat of (user, did, t) never extends or duplicates this check-in.
    this.ctx.storage.sql.exec('INSERT OR IGNORE INTO checkins VALUES (?,?,?)',user,t,expires);
    return this.checkinStatus(user);
  }
  checkinStatus(user:string):{checkedIn:false}|{checkedIn:true;expiresAt:number;deviceId:string} {
    const row=this.ctx.storage.sql.exec<{expires:number}>('SELECT expires FROM checkins WHERE user_id=? AND expires>? ORDER BY expires DESC LIMIT 1',user,Math.floor(Date.now()/1000)).toArray()[0];
    return row?{checkedIn:true,expiresAt:row.expires,deviceId:this.env.DEVICE_ID}:{checkedIn:false};
  }

  async ping(p: Ping) {
    // Re-verify at the DO boundary; RPC is not a public HTTP endpoint.
    if (!await trustedPing(p, this.env)) return { error: 'Invalid signature' };
    const now = Math.floor(Date.now() / 1000);
    const previous = this.ctx.storage.sql.exec<LinkState>('SELECT sequence,last_seen,uptime,nonce,kid FROM state WHERE id=1').toArray()[0];
    if (this.ctx.storage.sql.exec('SELECT nonce FROM nonces WHERE nonce=?', p.nonce).toArray().length) return { error: 'Replay rejected' };
    if (previous && now - previous.last_seen < 2) return { error: 'Too frequent' };
    const sequence = (previous?.sequence ?? 0) + 1;
    this.ctx.storage.transactionSync(() => {
      this.ctx.storage.sql.exec('INSERT INTO nonces (nonce,seen) VALUES (?,?)', p.nonce, now);
      this.ctx.storage.sql.exec('DELETE FROM nonces WHERE seen < ?', now - 180);
      this.ctx.storage.sql.exec('INSERT OR REPLACE INTO state (id,sequence,last_seen,uptime,nonce,kid) VALUES (1,?,?,?,?,?)', sequence, now, p.uptime, p.nonce, p.kid);
    });
    console.log(JSON.stringify({ event: 'device_ping', device_id: p.device_id, sequence }));
    return { type: 'pong', device_id: p.device_id, nonce: p.nonce, sequence, server_time: now };
  }

  status() {
    const state = this.ctx.storage.sql.exec<LinkState>('SELECT sequence,last_seen,uptime,nonce,kid FROM state WHERE id=1').toArray()[0];
    return {
      device_id: this.env.DEVICE_ID, transport: 'HTTPS polling',
      online: !!state && Math.floor(Date.now() / 1000) - state.last_seen < 90,
      sequence: state?.sequence ?? 0, last_seen: state?.last_seen ?? null,
      uptime: state?.uptime ?? null, kid: state?.kid ?? null,
    };
  }
}

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const checkin=await checkinHttp(request,env);
    if(checkin)return checkin;
    const path = new URL(request.url).pathname;
    if (path === '/health' && request.method === 'GET') return reply({ ok: true, service: 'vnfarm-device-link' });
    if (path !== `/api/devices/${env.DEVICE_ID}/ping` && path !== `/api/devices/${env.DEVICE_ID}/status`) return reply({ error: 'Not found' }, 404);
    const stub = env.DEVICES.getByName(env.DEVICE_ID);
    if (path.endsWith('/status') && request.method === 'GET') return reply(await stub.status());
    if (!path.endsWith('/ping') || request.method !== 'POST') return reply({ error: 'Method not allowed' }, 405);
    if (!request.headers.get('content-type')?.startsWith('application/json')) return reply({ error: 'Expected JSON' }, 415);
    // Bound the streamed input, even when Content-Length is absent or untrusted.
    const reader = request.body?.getReader();
    if (!reader) return reply({ error: 'Missing body' }, 400);
    const chunks: Uint8Array[] = [];
    let size = 0;
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      size += value.byteLength;
      if (size > 2048) { await reader.cancel(); return reply({ error: 'Body too large' }, 413); }
      chunks.push(value);
    }
    const bytes = new Uint8Array(size);
    let offset = 0;
    for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.byteLength; }
    let value: unknown;
    try { value = JSON.parse(new TextDecoder().decode(bytes)); }
    catch { return reply({ error: 'Invalid JSON' }, 400); }
    const ping = await trustedPing(value, env);
    if (!ping) return reply({ error: 'Invalid device signature or timestamp' }, 401);
    const result = await stub.ping(ping);
    return reply(result, 'error' in result ? 409 : 200);
  },
} satisfies ExportedHandler<Env>;
