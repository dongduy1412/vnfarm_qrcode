import assert from "node:assert/strict";
import {cmsRuntime} from "../../reference-vnfam-verification-do/scripts/cms-runtime";
import {b64,hash} from "../../reference-vnfam-verification-do/src/crypto";
const {mf,base}=await cmsRuntime(8792);
try{
 const keys=await crypto.subtle.generateKey({name:"ECDSA",namedCurve:"P-256"},true,["sign","verify"]);
 const raw=new Uint8Array(await crypto.subtle.exportKey("raw",keys.publicKey));
 const publicKey=b64(raw),keyId=await hash(raw);
 const sign=async(text:string)=>b64(new Uint8Array(await crypto.subtle.sign({name:"ECDSA",hash:"SHA-256"},keys.privateKey,new TextEncoder().encode(text))));
 const post=(path:string,body:unknown)=>fetch(base+path,{method:"POST",headers:{"content-type":"application/json"},body:JSON.stringify(body)});
 const created=await post("/api/device-registrations",{publicKey,hardwareTarget:"esp32-s3-devkitc-1",serial:"isolated-firmware-test"});assert.equal(created.status,201);
 const challenge=await created.json() as any;
 const body={nonce:challenge.nonce,proof:await sign(`REGISTER_V1|${challenge.registrationId}|${challenge.nonce}|${keyId}`)};
 const done=await post(`/api/device-registrations/${challenge.registrationId}/complete`,body);assert.equal(done.status,201);
 const identity=await done.json() as any;assert.equal(identity.keyId,keyId);assert.match(identity.deviceId,/^[a-f0-9]{32}$/);
 assert.equal((await post(`/api/device-registrations/${challenge.registrationId}/complete`,body)).status,409);
 const backend=await mf.getWorker("backend");
 const url="https://device.test/api/devices/"+identity.deviceId+"/connection";
 assert.equal((await backend.fetch(url,{headers:{Upgrade:"websocket"}})).status,401);
 const open=()=>backend.fetch(url,{headers:{Upgrade:"websocket",Authorization:"Bearer "+identity.token}});
 const boot={version:1,type:"BOOT_REPORT",eventId:crypto.randomUUID(),payload:{bootId:crypto.randomUUID(),keyId,hardwareTarget:"esp32-s3-devkitc-1",firmwareVersion:"0.4.1",buildId:"s3tim_0_4_1",otaSlotBytes:3145728,capabilities:[],appliedRevision:0,healthy:true}};
 async function session(){
  const response=await open();assert.equal(response.status,101);const ws=response.webSocket!;
  const inbox:any[]=[];ws.addEventListener("close",(e:any)=>console.log("WS test close",e.code,e.reason));ws.addEventListener("error",()=>console.log("WS test error"));ws.addEventListener("message",(event:any)=>inbox.push(event.data));ws.accept();
  async function next(){const end=Date.now()+15000;while(Date.now()<end){if(inbox.length)return inbox.shift();await new Promise(r=>setTimeout(r,10));}throw Error("WS message timeout");}
  assert.equal(JSON.parse(await next()).type,"WELCOME");console.log("WELCOME verified");ws.send(JSON.stringify(boot));
  const ack=JSON.parse(await next());assert.equal(ack.type,"EVENT_ACK");assert.equal(ack.payload.eventId,boot.eventId);
  ws.send("ping");assert.equal(await next(),"pong");return ws;
 }
 const first=await session();first.close(1000,"reconnect test");
 const second=await session();second.close(1000,"done");
 const db=await mf.getD1Database("DB","backend");
 const device=await db.prepare("SELECT bootId,confirmedAt FROM devices WHERE deviceId=?").bind(identity.deviceId).first<any>();
 assert.equal(device.bootId,boot.payload.bootId);assert.ok(device.confirmedAt);
 const t=Math.floor(Date.now()/30000)*30;const sig=await sign(`v1.${identity.deviceId}.${t}`);
 const checked=await fetch(base+`/checkin?v=1&did=${identity.deviceId}&t=${t}&sig=${sig}`);assert.equal(checked.status,200);assert.equal((await checked.json() as any).valid,true);
 console.log("PASS: isolated project server registration/complete, no approval, token auth, WELCOME, exact firmware BOOT schema/ACK, reconnect with same boot/event, ping/pong and real-key QR verification.");
}finally{await mf.dispose();}
