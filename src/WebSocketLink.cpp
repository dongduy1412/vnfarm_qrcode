#include "WebSocketLink.h"
#include "WorkerRootCAs.h"
#include "esp_websocket_client.h"
#include <ArduinoJson.h>
#include <esp_system.h>
#include <esp_partition.h>
#include <Preferences.h>
#include <WiFi.h>
#include <atomic>

namespace {
esp_websocket_client_handle_t client=nullptr;
QueueHandle_t incoming=nullptr;
std::atomic<bool> socketUp{false}, lost{false}, bootConfirmed{false};
String origin_,device_,token_,key_,uri_,header_,bootId_,bootEvent_,bootMessage_;
uint32_t sentAt=0,pingAt=0,retryAt=0,connectAt=0;
unsigned failures=0;
bool welcomed=false,bootSent=false;
std::atomic<bool> revoked{false};
char assembly[16385];size_t used=0;
struct Message{char* text;};
String uuid() {
  uint8_t b[16];esp_fill_random(b,16);b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128;
  char out[37];snprintf(out,sizeof(out),"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);return out;
}
void event(void*,esp_event_base_t,int32_t id,void* raw) {
  auto* data=static_cast<esp_websocket_event_data_t*>(raw);
  if(id==WEBSOCKET_EVENT_CONNECTED){socketUp=true;bootConfirmed=false;}
  if(id==WEBSOCKET_EVENT_DISCONNECTED){socketUp=false;bootConfirmed=false;lost=true;used=0;}
  if(id!=WEBSOCKET_EVENT_DATA)return;
  if(data->op_code==8){
    // A revoked/replaced session or schema rejection requires operator attention.
    if(data->data_len>=2){unsigned code=(uint8_t(data->data_ptr[0])<<8)|uint8_t(data->data_ptr[1]);
      if(code==4001||code==1008)revoked=true;}
    return;
  }
  if(data->op_code!=1&&data->op_code!=0)return;
  if(data->payload_offset==0&&data->op_code==1)used=0;
  if(data->payload_len<0||data->payload_len>16384||data->payload_offset<0||
     size_t(data->payload_offset)!=used||data->data_len<0||used+data->data_len>16384){used=0;return;}
  memcpy(assembly+used,data->data_ptr,data->data_len);used+=data->data_len;
  if(used!=size_t(data->payload_len))return;
  assembly[used]=0;Message message{static_cast<char*>(malloc(used+1))};
  if(message.text){memcpy(message.text,assembly,used+1);if(xQueueSend(incoming,&message,0)!=pdTRUE)free(message.text);}
  used=0;
}
bool transmit(const String& message){return client&&socketUp&&esp_websocket_client_send_text(client,message.c_str(),message.length(),500)==int(message.length());}
void shutdown(){
  if(client){esp_websocket_client_stop(client);esp_websocket_client_destroy(client);client=nullptr;}
  socketUp=false;bootConfirmed=false;welcomed=false;bootSent=false;
  Message m;while(incoming&&xQueueReceive(incoming,&m,0)==pdTRUE)free(m.text);
}
bool start(){
  esp_websocket_client_config_t config={};config.uri=uri_.c_str();config.headers=header_.c_str();
  config.disable_auto_reconnect=true;config.buffer_size=2048;config.task_stack=8192;
  config.ping_interval_sec=30;
  if(origin_.startsWith("https://")){config.cert_pem=WORKER_ROOT_CAS;config.transport=WEBSOCKET_TRANSPORT_OVER_SSL;}
  else config.transport=WEBSOCKET_TRANSPORT_OVER_TCP;
  client=esp_websocket_client_init(&config);if(!client)return false;
  esp_websocket_register_events(client,WEBSOCKET_EVENT_ANY,event,nullptr);
  if(esp_websocket_client_start(client)!=ESP_OK){shutdown();return false;}
  connectAt=millis();lost=false;return true;
}
}
namespace WebSocketLink {
bool begin(const String& origin,const String& deviceId,const String& token,const String& keyId){
  if(origin.length()==0||token.length()!=43||keyId.length()!=43)return false;
  origin_=origin;device_=deviceId;token_=token;key_=keyId;
  uri_=(origin.startsWith("https://")?String("wss://")+origin.substring(8):String("ws://")+origin.substring(7))+"/api/devices/"+deviceId+"/connection";
  header_="Authorization: Bearer "+token+"\r\n";
  incoming=xQueueCreate(4,sizeof(Message));if(!incoming)return false;
  bootId_=uuid();bootEvent_=uuid();
  StaticJsonDocument<1024> doc;doc["version"]=1;doc["type"]="BOOT_REPORT";doc["eventId"]=bootEvent_;
  auto p=doc.createNestedObject("payload");p["bootId"]=bootId_;p["keyId"]=key_;
  p["hardwareTarget"]="esp32-s3-devkitc-1";p["firmwareVersion"]="0.4.1";p["buildId"]="s3tim_0_4_1";
  const auto* partition=esp_partition_find_first(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_APP_FACTORY,nullptr);
  if(!partition){vQueueDelete(incoming);incoming=nullptr;return false;}
  p["otaSlotBytes"]=partition->size; // Application size only; OTA capability not advertised.
  p.createNestedArray("capabilities");p["appliedRevision"]=0;p["healthy"]=true;
  serializeJson(doc,bootMessage_);
  // Persist an immutable BOOT event. Reconnect retries retain eventId/payload.
  Preferences store;if(!store.begin("qr-ws",false))return false;
  bool saved=store.putString("boot",bootMessage_)==bootMessage_.length();store.end();
  if(!saved)return false;
  Serial.println("WS identity ready; waiting for authenticated connection");return true;
}
void tick(){
  if(!incoming)return;
  if(revoked){shutdown();return;}
  if(lost.exchange(false)){
    shutdown();retryAt=millis()+min(60000UL,1000UL<<min(failures++,5U))+(esp_random()%1000);
    Serial.println("WS disconnected; retry scheduled");
  }
  if(!client&&WiFi.status()==WL_CONNECTED&&static_cast<int32_t>(millis()-retryAt)>=0){
    if(!start())retryAt=millis()+10000;return;
  }
  if(client&&!welcomed&&static_cast<uint32_t>(millis()-connectAt)>20000){lost=true;return;}
  Message message;
  while(xQueueReceive(incoming,&message,0)==pdTRUE){
    if(strcmp(message.text,"pong")==0){free(message.text);continue;}
    DynamicJsonDocument doc(20000);auto error=deserializeJson(doc,message.text);free(message.text);
    if(error||doc["version"]!=1)continue;
    if(doc["type"]=="WELCOME"&&doc["payload"]["generation"]==1&&doc["payload"]["sessionId"].is<const char*>()){
      welcomed=true;bootSent=false;pingAt=millis();Serial.println("WS WELCOME received");
    }else if(welcomed&&doc["type"]=="EVENT_ACK"&&doc["payload"]["eventId"]==bootEvent_){
      bootConfirmed=true;failures=0;Serial.println("WS BOOT acknowledged; device online");
      Preferences store;if(store.begin("qr-ws",false)){store.remove("boot");store.end();}
    }else if(doc["type"]=="ERROR"){
      Serial.println("WS server rejected event; inspect protocol/configuration");revoked=true;
    }
    // No command capabilities advertised in this enrollment milestone.
  }
  if(welcomed&&!bootConfirmed&&(!bootSent||static_cast<uint32_t>(millis()-sentAt)>=10000)){
    bootSent=transmit(bootMessage_);sentAt=millis();
  }
  if(welcomed&&static_cast<uint32_t>(millis()-pingAt)>=30000){transmit("ping");pingAt=millis();}
}
bool isConnected(){return bootConfirmed.load();}
}
