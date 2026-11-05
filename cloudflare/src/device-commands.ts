import { ContractError, decode64 } from './contract';

export type DeviceRequest={v:1;did:string;t:number;sig:string;cmdId?:string};
export const WIFI_COMMANDS=['reconnect_wifi','forget_current_wifi','wipe_wifi'] as const;
export type WifiCommand=typeof WIFI_COMMANDS[number];
export type DeviceCommand={cmdId:string;command:WifiCommand;created:number;delivered:number|null;acked:number|null};
const json=(body:unknown,status=200)=>Response.json(body,{status,headers:{'Cache-Control':'no-store'}});
export async function authenticatedAdmin(request:Request,env:Env):Promise<boolean> {
  const authorization=request.headers.get('Authorization')??'';
  if(!env.ADMIN_API_TOKEN || !authorization.startsWith('Bearer ') || authorization.length>512)return false;
  const encode=new TextEncoder();
  const [provided,expected]=await Promise.all([
    crypto.subtle.digest('SHA-256',encode.encode(authorization.slice(7))),
    crypto.subtle.digest('SHA-256',encode.encode(env.ADMIN_API_TOKEN)),
  ]);
  return crypto.subtle.timingSafeEqual(provided,expected);
}
export async function readDeviceJson(request:Request):Promise<unknown> {
  if(!request.headers.get('Content-Type')?.startsWith('application/json'))throw new ContractError('bad_request');
  const reader=request.body?.getReader();if(!reader)throw new ContractError('bad_request');
  let length=0;const chunks:Uint8Array[]=[];
  while(true){const {done,value}=await reader.read();if(done)break;length+=value.length;if(length>2048){await reader.cancel();throw new ContractError('body_too_large',413);}chunks.push(value);}
  const all=new Uint8Array(length);let offset=0;for(const c of chunks){all.set(c,offset);offset+=c.length;}
  try{return JSON.parse(new TextDecoder().decode(all));}catch{throw new ContractError('bad_request');}
}
export async function verifyDeviceRequest(value:unknown,purpose:'poll'|'ack',env:Env):Promise<DeviceRequest> {
  if(!value||typeof value!=='object'||Array.isArray(value))throw new ContractError('bad_request');
  const p=value as Record<string,unknown>;
  const expected=purpose==='poll'?'did,sig,t,v':'cmdId,did,sig,t,v';
  if(Object.keys(p).sort().join(',')!==expected||p.v!==1||typeof p.did!=='string'||!/^[a-z0-9-]{1,32}$/.test(p.did)||
    !Number.isSafeInteger(p.t)||(p.t as number)<0||typeof p.sig!=='string')throw new ContractError('bad_request');
  if(p.did!==env.DEVICE_ID)throw new ContractError('unknown_device',404);
  if(purpose==='ack'&&(typeof p.cmdId!=='string'||!/^c_[A-Za-z0-9_-]{1,62}$/.test(p.cmdId)))throw new ContractError('bad_request');
  const signature=decode64(p.sig);if(signature.length!==64)throw new ContractError('bad_request');
  const message=purpose==='poll'?`v1.poll.${p.did}.${p.t}`:`v1.ack.${p.did}.${p.cmdId}.${p.t}`;
  const key=await crypto.subtle.importKey('raw',decode64(env.CHECKIN_PUBLIC_KEY),{name:'ECDSA',namedCurve:'P-256'},false,['verify']);
  if(!await crypto.subtle.verify({name:'ECDSA',hash:'SHA-256'},key,signature,new TextEncoder().encode(message)))throw new ContractError('invalid_signature',401);
  const age=Math.floor(Date.now()/1000)-(p.t as number);
  if(age>60)throw new ContractError('expired',410);
  if(age < -5)throw new ContractError('bad_request');
  return p as DeviceRequest;
}

export async function deviceCommandHttp(request:Request,env:Env):Promise<Response|null> {
  const path=new URL(request.url).pathname;
  const admin=/^\/api\/admin\/checkin\/devices\/([a-z0-9-]{1,32})\/commands$/.exec(path);
  if(!admin&&path!=='/api/device/poll'&&path!=='/api/device/ack')return null;
  try {
    const stub=env.DEVICES.getByName(env.DEVICE_ID);
    if(admin){
      if(!await authenticatedAdmin(request,env))return json({error:'unauthorized'},401);
      if(admin[1]!==env.DEVICE_ID)return json({error:'unknown_device'},404);
      if(request.method==='GET')return json({commands:await stub.listCommands()});
      if(request.method!=='POST')return json({error:'method_not_allowed'},405);
      const value=await readDeviceJson(request);
      if(!value||typeof value!=='object'||Object.keys(value).length!==1||!WIFI_COMMANDS.includes((value as {command:WifiCommand}).command))return json({error:'bad_request'},400);
      const queued=await stub.enqueueCommand((value as {command:WifiCommand}).command);
      if(queued.conflict)return json({error:'command_pending',pending:queued},409);
      return json(queued,queued.existing?200:202);
    }
    if(request.method!=='POST')return json({error:'method_not_allowed'},405);
    const purpose=path.endsWith('/poll')?'poll':'ack';
    const signed=await verifyDeviceRequest(await readDeviceJson(request),purpose,env);
    if(purpose==='poll')return json(await stub.pollCommand());
    const result=await stub.ackCommand(signed.cmdId!);
    if(result==='unknown')return json({error:'unknown_command'},404);
    if(result==='undelivered')return json({error:'command_not_delivered'},409);
    return new Response(null,{status:204,headers:{'Cache-Control':'no-store'}});
  }catch(error){if(error instanceof ContractError)return json({error:error.code},error.status);throw error;}
}
