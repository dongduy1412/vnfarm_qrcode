#pragma once
#include <Arduino.h>
#include "SavedNetworks.h"
#include "WifiCommandState.h"

class WifiCommands {
public:
  enum Result { Failed,Applied,Duplicate };
  bool begin(SavedNetworks& networks);
  Result accept(const char* id,const char* action,const String& currentSsid,SavedNetworks& networks);
  const char* pendingAck() const { return state_.phase==WifiCommandState::Phase::Applied?state_.id:""; }
  bool acknowledge(const char* id);
private:
  bool persist(const WifiCommandState& state);
  bool apply(SavedNetworks& networks);
  WifiCommandState state_;
};
