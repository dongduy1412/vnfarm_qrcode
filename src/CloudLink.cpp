#include "CloudLink.h"
#include "CloudRootCA.h"
#include "WifiCommandState.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <time.h>

#ifndef QR_CLOUD_BASE_URL
#define QR_CLOUD_BASE_URL "https://vnfarm-qrcode-device-link.dong141220047.workers.dev"
#endif

namespace {
enum Kind { Ping,Poll,Ack };
struct Message { Kind kind;char body[640];char device[33];char nonce[33];char cmdId[65]; };
QueueHandle_t messages=nullptr,events=nullptr;
uint32_t pingAt=0,pollAt=0,ackAt=0;
bool pingSent=false,pollSent=false,ackSent=false;
class ResponseBuffer:public Stream {
public:
  char data[768]={};size_t length=0;
  size_t write(uint8_t byte)override{if(length>=sizeof(data)-1)return 0;data[length++]=static_cast<char>(byte);return 1;}
  size_t write(const uint8_t* bytes,size_t count)override{if(count>sizeof(data)-1-length)return 0;memcpy(data+length,bytes,count);length+=count;return count;}
  int available()override{return 0;}int read()override{return -1;}int peek()override{return -1;}void flush()override{}
};

void sendTask(void*) {
  Message message;
  while(true) {
    if(xQueueReceive(messages,&message,portMAX_DELAY)!=pdTRUE)continue;
    if(WiFi.status()!=WL_CONNECTED)continue;
    WiFiClientSecure tls;tls.setCACert(CLOUD_ROOT_CA);tls.setHandshakeTimeout(5);
    HTTPClient http;http.setConnectTimeout(4000);http.setTimeout(4000);http.setReuse(false);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.setUserAgent("Mozilla/5.0 (compatible; VNFARM-ESP32/0.3.0)");
    String path=message.kind==Ping?String("/api/devices/")+message.device+"/ping":message.kind==Poll?"/api/device/poll":"/api/device/ack";
    if(!http.begin(tls,String(QR_CLOUD_BASE_URL)+path)){Serial.println("DO HTTP initialization failed");continue;}
    http.addHeader("Content-Type","application/json");
    int code=http.POST(reinterpret_cast<uint8_t*>(message.body),strlen(message.body));
    bool valid=false;
    if(message.kind==Ack&&code==204) {
      CloudLink::Event event{};event.type=CloudLink::Event::Acknowledged;strcpy(event.cmdId,message.cmdId);
      valid=xQueueSend(events,&event,0)==pdTRUE;
    } else if(code==200) {
      ResponseBuffer buffer;int copied=http.writeToStream(&buffer);
      StaticJsonDocument<768> response;
      if(copied>0&&!deserializeJson(response,buffer.data,buffer.length)) {
        if(message.kind==Ping&&response["type"]=="pong"&&response["device_id"]==message.device&&response["nonce"]==message.nonce&&response["sequence"].is<uint32_t>()) {
          Serial.printf("DO pong verified: sequence=%lu server_time=%lu\n",response["sequence"].as<unsigned long>(),response["server_time"].as<unsigned long>());valid=true;
        } else if(message.kind==Poll&&response.containsKey("command")) {
          if(response["command"].isNull()){Serial.println("Device poll verified: no command");valid=true;}
          else if(response["command"].is<const char*>()&&response["cmdId"].is<const char*>()&&response["t"].is<uint32_t>()) {
            const char* command=response["command"];const char* id=response["cmdId"];
            int64_t age=static_cast<int64_t>(time(nullptr))-response["t"].as<uint32_t>();
            if(wifiAction(command)!=WifiAction::None&&validCommandId(id)&&age>=-5&&age<=60) {
              CloudLink::Event event{};event.type=CloudLink::Event::Command;strcpy(event.cmdId,id);strcpy(event.command,command);
              valid=xQueueSend(events,&event,0)==pdTRUE;
            }
          }
        }
      }
    }
    if(!valid)Serial.printf("DO request failed: kind=%u HTTP=%d\n",static_cast<unsigned>(message.kind),code);
    http.end();
  }
}

bool submit(Kind kind,DeviceKeys& keys,const String& deviceId,const char* cmdId="") {
  if(!messages||WiFi.status()!=WL_CONNECTED||deviceId.length()>32||time(nullptr)<1704067200)return false;
  Message message{};message.kind=kind;deviceId.toCharArray(message.device,sizeof(message.device));
  uint32_t now=static_cast<uint32_t>(time(nullptr));
  StaticJsonDocument<640> json;String payload,signature;
  if(kind==Ping) {
    uint8_t random[16];esp_fill_random(random,sizeof(random));
    for(size_t i=0;i<sizeof(random);++i)snprintf(message.nonce+i*2,3,"%02x",random[i]);
    uint32_t uptime=millis()/1000;
    payload="ping-v1\n"+deviceId+"\n"+keys.keyId()+"\n"+String(now)+"\n"+message.nonce+"\n"+String(uptime);
    if(!keys.sign(payload,signature))return false;
    json["device_id"]=deviceId;json["kid"]=keys.keyId();json["iat"]=now;json["nonce"]=message.nonce;json["uptime"]=uptime;
  } else {
    json["v"]=1;json["did"]=deviceId;json["t"]=now;
    if(kind==Ack) {
      if(!validCommandId(cmdId))return false;
      strcpy(message.cmdId,cmdId);json["cmdId"]=message.cmdId;
      payload="v1.ack."+deviceId+"."+message.cmdId+"."+String(now);
    }else payload="v1.poll."+deviceId+"."+String(now);
    if(!keys.signQr(payload,signature))return false;
  }
  json["sig"]=signature;
  if(json.overflowed()||measureJson(json)>=sizeof(message.body))return false;
  serializeJson(json,message.body,sizeof(message.body));
  return (kind==Ack?xQueueSendToFront(messages,&message,0):xQueueSend(messages,&message,0))==pdTRUE;
}
}

bool CloudLink::begin() {
  if(messages)return true;
  messages=xQueueCreate(3,sizeof(Message));events=xQueueCreate(4,sizeof(Event));
  if(!messages||!events||xTaskCreate(sendTask,"cloud-link",12288,nullptr,1,nullptr)!=pdPASS) {
    if(messages)vQueueDelete(messages);if(events)vQueueDelete(events);messages=nullptr;events=nullptr;return false;
  }
  Serial.printf("DO link endpoint: %s\n",QR_CLOUD_BASE_URL);return true;
}
bool CloudLink::acknowledge(DeviceKeys& keys,const String& deviceId,const char* cmdId) {
  if(!submit(Ack,keys,deviceId,cmdId))return false;
  ackAt=millis();ackSent=true;return true;
}
bool CloudLink::nextEvent(Event& event){return events&&xQueueReceive(events,&event,0)==pdTRUE;}
void CloudLink::tick(DeviceKeys& keys,const String& deviceId,const char* pendingAck) {
  uint32_t now=millis();
  if(pendingAck&&strlen(pendingAck)&&(!ackSent||static_cast<uint32_t>(now-ackAt)>=10000))acknowledge(keys,deviceId,pendingAck);
  if(!pingSent||static_cast<uint32_t>(now-pingAt)>=30000){if(submit(Ping,keys,deviceId)){pingSent=true;pingAt=now;}}
  if((!pendingAck||!strlen(pendingAck))&&(!pollSent||static_cast<uint32_t>(now-pollAt)>=60000)) {
    if(submit(Poll,keys,deviceId)){pollSent=true;pollAt=now;}
  }
}
