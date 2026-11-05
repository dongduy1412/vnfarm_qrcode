import { ContractError, encode64, verifyContract } from './contract';

const json = (body: unknown, status = 200, headers: HeadersInit = {}) => Response.json(body, {status,headers:{'Cache-Control':'no-store','Referrer-Policy':'no-referrer',...headers}});
const escape = (s: string) => s.replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]!));
const html = (body: string, status=200) => new Response(`<!doctype html><html lang="vi"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>VNFARM · Contract test</title><body><h1>VNFARM · Thử giao thức QR</h1><p>Worker test, chưa kết nối tài khoản hoặc tem của sự kiện.</p>${body}</body></html>`,{status,headers:{'Content-Type':'text/html; charset=utf-8','Cache-Control':'no-store','Referrer-Policy':'no-referrer','Content-Security-Policy':"default-src 'none'; form-action 'self'; frame-ancestors 'none'; base-uri 'none'"}});
const redirect = (path:string, cookie?: string) => new Response(null,{status:302,headers:{Location:path,'Cache-Control':'no-store',...(cookie?{'Set-Cookie':cookie}:{})}});
async function hash(text:string) { return encode64(new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(text)))); }
function cookie(request:Request) {
  return /(?:^|;\s*)vnfarm_test_session=([A-Za-z0-9_-]{43})(?:;|$)/.exec(request.headers.get('cookie') ?? '')?.[1] ?? null;
}
function safeReturn(value:string) {
  if (value.length > 2048 || !value.startsWith('/checkin?') || /[\r\n]/.test(value)) return '/stamp';
  const url = new URL(value,'https://test.invalid');
  return url.origin === 'https://test.invalid' && url.pathname === '/checkin' ? url.pathname+url.search : '/stamp';
}

// This adapter supplies isolated anonymous test sessions. Production apps must
// replace it with their existing authenticated user resolver; never trust did/user
// values submitted by the scanner as proof of login.
export async function checkinHttp(request:Request,env:Env):Promise<Response|null> {
  const url = new URL(request.url), path=url.pathname;
  if (!['/checkin','/api/checkin/status','/login','/test/session','/stamp'].includes(path)) return null;
  const stub=env.DEVICES.getByName(env.DEVICE_ID);
  const token=cookie(request);
  const user=token ? await stub.testUser(await hash(token)) : null;
  if (path==='/login' && request.method==='GET') {
    if (env.TEST_MODE!=='true') return html('<p>Cần tích hợp đăng nhập của ứng dụng sự kiện.</p>',503);
    const back=safeReturn(url.searchParams.get('returnTo')??'');
    return html(`<p>Bắt đầu một phiên người chơi thử, sau đó quét QR mới. Phiên này không phải tài khoản sự kiện.</p><form method="post" action="/test/session"><input type="hidden" name="returnTo" value="${escape(back)}"><button>Bắt đầu phiên test</button></form>`);
  }
  if (path==='/test/session' && request.method==='POST') {
    if (env.TEST_MODE!=='true') return json({error:'not_found'},404);
    if (request.headers.get('origin') && request.headers.get('origin')!==url.origin) return json({error:'bad_request'},403);
    const reader=request.body?.getReader();
    if (!reader) return json({error:'bad_request'},400);
    const chunks:Uint8Array[]=[]; let length=0;
    while(true) {const {done,value}=await reader.read();if(done)break;length+=value.length;if(length>4096){await reader.cancel();return json({error:'bad_request'},413);}chunks.push(value);}
    const all=new Uint8Array(length);let offset=0;for(const part of chunks){all.set(part,offset);offset+=part.length;}
    const back=safeReturn(new URLSearchParams(new TextDecoder().decode(all)).get('returnTo')??'');
    const value=encode64(crypto.getRandomValues(new Uint8Array(32)));
    await stub.createTestSession(await hash(value),crypto.randomUUID());
    return redirect(back,`vnfarm_test_session=${value}; HttpOnly; Secure; SameSite=Lax; Path=/; Max-Age=3600`);
  }
  if (path==='/api/checkin/status' && request.method==='GET') return json(user?await stub.checkinStatus(user):{checkedIn:false});
  if (path==='/stamp' && request.method==='GET') {
    const status=user?await stub.checkinStatus(user):{checkedIn:false};
    return html(status.checkedIn?`<h2>Check-in thử thành công</h2><p>Chữ ký và thời hạn hợp lệ; kết quả đã lưu trong DO.</p><p>Đây là trang nhận kết quả test, chưa cấp tem hay phần thưởng.</p><p><a href="/api/checkin/status">Xem trạng thái</a></p>`:'<p>Chưa check-in. Hãy bắt đầu phiên test, rồi quét QR mới trên thiết bị.</p><a href="/login">Bắt đầu phiên test</a>');
  }
  if(path==='/checkin' && request.method==='GET') {
    if(!user) return redirect('/login?returnTo='+encodeURIComponent(url.pathname+url.search));
    const ip=await hash(request.headers.get('CF-Connecting-IP')??'local');
    if(!await stub.allowCheckin(ip,user)) return json({error:'rate_limited'},429);
    try {
      const verified=await verifyContract(url,async did=>did===env.DEVICE_ID?env.CHECKIN_PUBLIC_KEY:null,Number(env.CHECKIN_TTL),Number(env.CHECKIN_SKEW));
      const status=await stub.recordCheckin(user,verified.issuedAt);
      if(request.headers.get('accept')?.includes('application/json')) return json(status,302,{Location:'/stamp'});
      return redirect('/stamp');
    } catch(error) {
      if(!(error instanceof ContractError)) throw error;
      if(request.headers.get('accept')?.includes('application/json')) return json({error:error.code},error.status);
      return html(`<h2>Không thể check-in</h2><p>${escape(error.code)}</p><p>Hãy quét lại mã mới trên thiết bị.</p><a href="/stamp">Quét lại</a>`,error.status);
    }
  }
  return json({error:'bad_request'},405);
}
