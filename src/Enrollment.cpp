#include "Enrollment.h"
#include "WorkerRootCAs.h"
#include "ServerOrigin.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <mbedtls/sha256.h>
#include <time.h>
namespace {
bool b64(const String& s,size_t n) {
  if(s.length()!=n)return false;
  for(char c:s)if(!isalnum(static_cast<unsigned char>(c))&&c!='_'&&c!='-')return false;
  return true;
}
bool deviceIdValid(const String& s) {
  if(s.length()!=32)return false;
  for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
  return true;
}
// Bound both Content-Length and chunked responses. Never log response bodies.
class Reply:public Stream {
public:
  char bytes[2048]={};size_t count=0;
  size_t write(uint8_t c)override{if(count>=sizeof(bytes)-1)return 0;bytes[count++]=c;return 1;}
  int read()override{return -1;}int peek()override{return -1;}int available()override{return 0;}void flush()override{}
};
int post(const String& url,JsonDocument& body,JsonDocument& output) {
  WiFiClientSecure tls;tls.setCACert(WORKER_ROOT_CAS);tls.setHandshakeTimeout(5);
  WiFiClient lan;HTTPClient http;http.setConnectTimeout(4000);http.setTimeout(5000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  bool ok=url.startsWith("https://")?http.begin(tls,url):http.begin(lan,url);
  if(!ok)return -1;
  http.setUserAgent("Mozilla/5.0 (VNFARM enrollment/0.4.1)");
  http.addHeader("Content-Type","application/json");String encoded;serializeJson(body,encoded);
  int code=http.POST(encoded);
  if(code<0 && url.startsWith("https://")) {
    char detail[160]={};int tlsCode=tls.lastError(detail,sizeof(detail));
    Serial.printf("Enrollment transport failure: HTTP=%d TLS=%d %s\n",code,tlsCode,detail);
  }
  if(code>0){Reply reply;if(http.writeToStream(&reply)<0||deserializeJson(output,reply.bytes,reply.count))code=-2;}
  http.end();return code;
}
}
bool Enrollment::save() {
  DynamicJsonDocument doc(2048);doc["v"]=1;doc["origin"]=origin_;doc["keyId"]=kid_;
  doc["phase"]=phase_;doc["registrationId"]=registration_;doc["nonce"]=nonce_;
  doc["expiresAt"]=expires_;doc["deviceId"]=did_;doc["token"]=token_;doc["generation"]=1;
  String encoded;serializeJson(doc,encoded);if(doc.overflowed())return false;
  Preferences p;if(!p.begin("qr-identity",false))return false;
  bool ok=p.putString(slot_.c_str(),encoded)==encoded.length()&&p.getString(slot_.c_str(),"")==encoded;
  p.end();return ok;
}
bool Enrollment::begin(const String& origin,DeviceKeys& keys,const String& serial) {
  if(!ServerOrigin::valid(origin.c_str()))return false;
  origin_=origin;serial_=serial;kid_=keys.serverKeyId();phase_="new";status_="DANG KY SERVER";
  if(!b64(kid_,43))return false;
  uint8_t hash[32];mbedtls_sha256_ret(reinterpret_cast<const uint8_t*>(origin.c_str()),origin.length(),hash,0);
  char slot[15];for(int i=0;i<7;++i)snprintf(slot+2*i,3,"%02x",hash[i]);slot_=slot;
  Preferences p;if(!p.begin("qr-identity",false))return false;
  String encoded=p.getString(slot_.c_str(),"");p.end();
  if(!encoded.length())return true;
  DynamicJsonDocument doc(2048);
  if(deserializeJson(doc,encoded)||doc["v"]!=1||doc["origin"]!=origin_||doc["keyId"]!=kid_)return false;
  phase_=doc["phase"].as<String>();registration_=doc["registrationId"].as<String>();
  nonce_=doc["nonce"].as<String>();expires_=doc["expiresAt"]|int64_t(0);
  did_=doc["deviceId"].as<String>();token_=doc["token"].as<String>();
  if(phase_=="ready")return doc["generation"]==1&&deviceIdValid(did_)&&b64(token_,43);
  if(phase_=="challenge")return registration_.length()==36&&b64(nonce_,43);
  // 'completing' survives crashes: the response may have been consumed once.
  status_="CAN KHOI PHUC GHEP NOI";phase_="blocked";return true;
}
bool Enrollment::copyIdentity(const String& from,const String& to,DeviceKeys& keys) {
  if(!ServerOrigin::valid(to.c_str()))return false;
  Enrollment source;
  bool sourceValid=source.begin(from,keys,"");
  Serial.printf("Identity migration source: origin=%s valid=%d ready=%d phase=%s\n",from.c_str(),int(sourceValid),int(source.ready()),source.phase_.c_str());
  if(!sourceValid||!source.ready())return false;
  Enrollment destination;
  bool destinationValid=destination.begin(to,keys,"");
  Serial.printf("Identity migration destination: origin=%s valid=%d ready=%d phase=%s\n",to.c_str(),int(destinationValid),int(destination.ready()),destination.phase_.c_str());
  if(!destinationValid)return false;
  // Never overwrite a different ready identity at the destination.
  if(destination.ready())return destination.deviceId()==source.deviceId()&&destination.token()==source.token();
  source.origin_=to;source.slot_=destination.slot_;
  if(!source.save()){Serial.println("Identity migration: NVS write/readback failed");return false;}
  Enrollment verify;
  return verify.begin(to,keys,"")&&verify.ready()&&verify.deviceId()==source.deviceId()&&verify.token()==source.token();
}
void Enrollment::tick(DeviceKeys& keys) {
  if(ready()||phase_=="blocked"||WiFi.status()!=WL_CONNECTED||static_cast<int32_t>(millis()-retryAt_)<0)return;
  retryAt_=millis()+30000;
  DynamicJsonDocument request(1024),response(2048);
  if(phase_=="new") {
    request["publicKey"]=keys.publicRaw();request["hardwareTarget"]="esp32-s3-devkitc-1";request["serial"]=serial_;
    int code=post(origin_+"/api/device-registrations",request,response);
    Serial.printf("Enrollment challenge HTTP=%d\n",code);
    if(code==409){phase_="blocked";status_="CAN KHOI PHUC GHEP NOI";return;}
    if(code!=201)return;
    registration_=response["registrationId"].as<String>();nonce_=response["nonce"].as<String>();
    expires_=response["expiresAt"]|int64_t(0);
    if(registration_.length()!=36||!b64(nonce_,43)||expires_<=int64_t(time(nullptr))*1000)return;
    phase_="challenge";
    if(!save()){phase_="blocked";status_="LOI LUU DANG KY";return;}
    retryAt_=millis();return;
  }
  if(phase_!="challenge")return;
  if(expires_<=int64_t(time(nullptr))*1000){phase_="new";return;}
  String proof;if(!keys.signQr("REGISTER_V1|"+registration_+"|"+nonce_+"|"+kid_,proof))return;
  request["nonce"]=nonce_;request["proof"]=proof;
  phase_="completing";
  if(!save()){phase_="blocked";status_="LOI LUU DANG KY";return;}
  int code=post(origin_+"/api/device-registrations/"+registration_+"/complete",request,response);
  Serial.printf("Enrollment complete HTTP=%d\n",code);
  if(code!=201){phase_="blocked";status_="CAN KHOI PHUC GHEP NOI";return;}
  did_=response["deviceId"].as<String>();token_=response["token"].as<String>();
  if(!deviceIdValid(did_)||!b64(token_,43)||response["keyId"]!=kid_||response["generation"]!=1){phase_="blocked";status_="SAI IDENTITY SERVER";return;}
  phase_="ready";nonce_="";
  if(!save()){phase_="blocked";status_="LOI LUU IDENTITY";return;}
  status_="DA DANG KY";Serial.println("Enrollment ready; identity persisted, credentials hidden");
}
