#include "WifiCommands.h"
#include <ArduinoJson.h>
#include <Preferences.h>

bool WifiCommands::persist(const WifiCommandState& state) {
  DynamicJsonDocument doc(2048);
  doc["v"]=1;doc["phase"]=static_cast<uint8_t>(state.phase);doc["command"]=wifiActionName(state.action);
  doc["cmdId"]=state.id;doc["target"]=state.target;
  auto history=doc.createNestedArray("recent");for(uint8_t i=0;i<state.recentCount;++i)history.add(state.recent[i]);
  if(doc.overflowed())return false;
  String json;serializeJson(doc,json);
  Preferences store;if(!store.begin("qr-commands",false))return false;
  bool ok=store.putString("state",json)==json.length()&&store.getString("state","")==json;
  store.end();return ok;
}

bool WifiCommands::begin(SavedNetworks& networks) {
  Preferences store;if(!store.begin("qr-commands",false))return false;
  String encoded=store.getString("state","");store.end();
  if(encoded.length()) {
    if(encoded.length()>4096)return false;
    DynamicJsonDocument doc(4096);
    if(deserializeJson(doc,encoded)||doc["v"]!=1||!doc["phase"].is<uint8_t>()||doc["phase"].as<uint8_t>()>2||
       !doc["cmdId"].is<const char*>()||!doc["target"].is<const char*>()||!doc["recent"].is<JsonArray>()||doc["recent"].size()>8)return false;
    const char* cmd=doc["cmdId"];const char* target=doc["target"];
    state_.phase=static_cast<WifiCommandState::Phase>(doc["phase"].as<uint8_t>());
    state_.action=wifiAction(doc["command"]|"");
    if(strlen(cmd)>64||strlen(target)>32)return false;
    if(state_.phase!=WifiCommandState::Phase::Idle&&(!validCommandId(cmd)||state_.action==WifiAction::None))return false;
    if(state_.action==WifiAction::ForgetCurrent&&!strlen(target))return false;
    strcpy(state_.id,cmd);strcpy(state_.target,target);
    for(JsonVariant entry:doc["recent"].as<JsonArray>()) {
      if(!entry.is<const char*>()||!validCommandId(entry.as<const char*>()))return false;
      strcpy(state_.recent[state_.recentCount++],entry.as<const char*>());
    }
  }
  // Resume an interrupted storage operation before any connection attempt.
  return state_.phase!=WifiCommandState::Phase::Received||apply(networks);
}

bool WifiCommands::apply(SavedNetworks& networks) {
  if(state_.phase!=WifiCommandState::Phase::Received)return false;
  if(state_.action!=WifiAction::Reconnect) {
    Preferences config;if(!config.begin("qr-config",false))return false;
    bool ok=config.putBool("configured",false)==1;
    config.end();if(!ok)return false;
    if(state_.action==WifiAction::Wipe){if(!networks.clear())return false;}
    else if(!networks.forget(state_.target))return false;
    if(!config.begin("qr-config",false))return false;
    if(networks.size()) {
      String ssid=networks.at(0).ssid,password=networks.at(0).password;
      ok=config.putString("ssid",ssid)==ssid.length()&&config.putString("pass",password)==password.length()&&
         config.getString("ssid","")==ssid&&config.getString("pass","")==password;
      if(ok)ok=config.putBool("fallback",state_.action==WifiAction::ForgetCurrent)==1;
      if(ok)ok=config.putBool("configured",true)==1;
    } else {
      ok=(!config.isKey("ssid")||config.remove("ssid"))&&(!config.isKey("pass")||config.remove("pass"));
      if(ok)ok=config.putBool("fallback",false)==1;
    }
    config.end();if(!ok)return false;
  }
  WifiCommandState next=state_;if(!next.applied()||!persist(next))return false;
  state_=next;return true;
}

WifiCommands::Result WifiCommands::accept(const char* id,const char* action,const String& currentSsid,SavedNetworks& networks) {
  if(state_.alreadyApplied(id))return Duplicate;
  if(state_.phase==WifiCommandState::Phase::Received&&strcmp(state_.id,id)==0)return apply(networks)?Applied:Failed;
  WifiCommandState next=state_;
  if(!next.receive(id,wifiAction(action),currentSsid.c_str())||!persist(next))return Failed;
  state_=next;return apply(networks)?Applied:Failed;
}

bool WifiCommands::acknowledge(const char* id) {
  WifiCommandState next=state_;if(!next.acknowledged(id)||!persist(next))return false;
  state_=next;return true;
}
