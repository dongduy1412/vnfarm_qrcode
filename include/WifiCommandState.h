#pragma once
#include <stdint.h>
#include <string.h>

enum class WifiAction:uint8_t { None,Reconnect,ForgetCurrent,Wipe };
inline WifiAction wifiAction(const char* text) {
  if(!text)return WifiAction::None;
  if(strcmp(text,"reconnect_wifi")==0)return WifiAction::Reconnect;
  if(strcmp(text,"forget_current_wifi")==0)return WifiAction::ForgetCurrent;
  if(strcmp(text,"wipe_wifi")==0)return WifiAction::Wipe;
  return WifiAction::None;
}
inline const char* wifiActionName(WifiAction action) {
  switch(action){case WifiAction::Reconnect:return "reconnect_wifi";case WifiAction::ForgetCurrent:return "forget_current_wifi";case WifiAction::Wipe:return "wipe_wifi";default:return "none";}
}
inline bool validCommandId(const char* id) {
  size_t n=strlen(id);if(n<3||n>64||id[0]!='c'||id[1]!='_')return false;
  for(size_t i=2;i<n;++i)if(!((id[i]>='a'&&id[i]<='z')||(id[i]>='A'&&id[i]<='Z')||(id[i]>='0'&&id[i]<='9')||id[i]=='-'||id[i]=='_'))return false;
  return true;
}
struct WifiCommandState {
  enum class Phase:uint8_t { Idle,Received,Applied };
  Phase phase=Phase::Idle;
  WifiAction action=WifiAction::None;
  char id[65]={};
  char target[33]={};
  char recent[8][65]={};
  uint8_t recentCount=0;
  bool alreadyApplied(const char* cmd)const {
    for(uint8_t i=0;i<recentCount;++i)if(strcmp(recent[i],cmd)==0)return true;
    return false;
  }
  bool receive(const char* cmd,WifiAction name,const char* ssid) {
    if(!validCommandId(cmd)||name==WifiAction::None||alreadyApplied(cmd)||phase!=Phase::Idle)return false;
    if(name==WifiAction::ForgetCurrent&&(!ssid||!strlen(ssid)||strlen(ssid)>32))return false;
    strcpy(id,cmd);action=name;phase=Phase::Received;
    if(name==WifiAction::ForgetCurrent)strcpy(target,ssid);
    return true;
  }
  bool applied() {
    if(phase!=Phase::Received)return false;
    if(recentCount==8){for(int i=1;i<8;++i)strcpy(recent[i-1],recent[i]);--recentCount;}
    strcpy(recent[recentCount++],id);phase=Phase::Applied;return true;
  }
  bool acknowledged(const char* cmd) {
    if(phase!=Phase::Applied||strcmp(id,cmd)!=0)return false;
    phase=Phase::Idle;action=WifiAction::None;memset(id,0,sizeof(id));memset(target,0,sizeof(target));return true;
  }
};
