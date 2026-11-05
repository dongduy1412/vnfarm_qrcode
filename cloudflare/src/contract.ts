export class ContractError extends Error {
  constructor(public code: string, public status = 400) { super(code); }
}
export function decode64(text: string): Uint8Array<ArrayBuffer> {
  if (!/^[A-Za-z0-9_-]+$/.test(text)) throw new ContractError('bad_request');
  let bytes: Uint8Array<ArrayBuffer>;
  try { bytes = Uint8Array.from(atob(text.replace(/-/g, '+').replace(/_/g, '/')), c => c.charCodeAt(0)); }
  catch { throw new ContractError('bad_request'); }
  if (encode64(bytes) !== text) throw new ContractError('bad_request');
  return bytes;
}
export function encode64(bytes: Uint8Array): string {
  return btoa(String.fromCharCode(...bytes)).replace(/\+/g,'-').replace(/\//g,'_').replace(/=+$/,'');
}
export async function verifyContract(url: URL, getKey: (did: string) => Promise<string | null>, ttl = 60, skew = 5, now = Math.floor(Date.now()/1000)) {
  if (![10,20,30,60].includes(ttl) || !Number.isInteger(skew) || skew<0 || skew>60) throw Error('Invalid server TTL/SKEW configuration');
  if (url.pathname !== '/checkin' || url.hash || url.href.length > 2048) throw new ContractError('bad_request');
  const entries = [...url.searchParams];
  const p = Object.fromEntries(entries);
  if (entries.length !== 4 || Object.keys(p).length !== 4 || Object.keys(p).sort().join(',') !== 'did,sig,t,v' ||
      p.v !== '1' || !/^[a-z0-9-]{1,32}$/.test(p.did) || !/^(0|[1-9][0-9]{0,10})$/.test(p.t)) throw new ContractError('bad_request');
  const signature = decode64(p.sig);
  if (signature.length !== 64) throw new ContractError('bad_request');
  const registered = await getKey(p.did);
  if (!registered) throw new ContractError('unknown_device', 404);
  const raw = decode64(registered);
  if (raw.length !== 65 || raw[0] !== 4) throw Error('Invalid registered P-256 public key');
  const key = await crypto.subtle.importKey('raw', raw, {name:'ECDSA',namedCurve:'P-256'}, false, ['verify']);
  if (!await crypto.subtle.verify({name:'ECDSA',hash:'SHA-256'}, key, signature, new TextEncoder().encode(`v1.${p.did}.${p.t}`))) throw new ContractError('invalid_signature',401);
  const issued = Number(p.t), age = now - issued;
  if (age > ttl) throw new ContractError('expired',410);
  if (age < -skew) throw new ContractError('bad_request');
  return { deviceId:p.did, issuedAt:issued, qrExpiresAt:issued+ttl };
}
