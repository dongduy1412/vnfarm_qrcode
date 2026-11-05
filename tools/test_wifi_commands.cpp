#include "WifiCommands.h"
#include "Preferences.h"
#include <assert.h>
#include <stdio.h>

void seed(SavedNetworks& networks) {
  FakeNvs::data.clear();FakeNvs::failAfter=-1;
  FakeNvs::data["qr-keys"]["private-der"]="SYNTHETIC-DEVICE-KEY-DO-NOT-CHANGE";
  FakeNvs::data["qr-config"]={{"ssid","home"},{"pass","test-home-pass"},{"configured","true"},{"url","unchanged-url"}};
  assert(networks.remember("office","test-office-pass"));
  assert(networks.remember("home","test-home-pass"));
}

int main() {
  unsigned checks=0;
  for(const char* name:{"reconnect_wifi","forget_current_wifi","wipe_wifi"}) {
    for(int cutoff=0;cutoff<14;++cutoff) {
      SavedNetworks networks;seed(networks);
      WifiCommands commands;assert(commands.begin(networks));
      auto key=FakeNvs::data["qr-keys"];
      FakeNvs::failAfter=cutoff;
      try{commands.accept("c_simulated",name,"home",networks);}catch(const FakeNvs::PowerLoss&){}
      FakeNvs::failAfter=-1;
      SavedNetworks restored;
      assert(restored.load(FakeNvs::data["qr-config"]["ssid"],FakeNvs::data["qr-config"]["pass"]));
      WifiCommands recovered;assert(recovered.begin(restored));
      assert(FakeNvs::data["qr-keys"]==key);
      assert(FakeNvs::data["qr-config"]["url"]=="unchanged-url");
      if(!strlen(recovered.pendingAck())) {
        assert(cutoff==0&&restored.size()==2); // Intent was never durably received.
      } else {
        size_t expected=strcmp(name,"wipe_wifi")==0?0:strcmp(name,"forget_current_wifi")==0?1:2;
        assert(restored.size()==expected);
        if(expected==1)assert(strcmp(restored.at(0).ssid,"office")==0);
        if(expected==0)assert(FakeNvs::data["qr-config"].count("ssid")==0&&FakeNvs::data["qr-config"].count("pass")==0);
        // A newly configured network must survive loss of the ACK/replayed poll.
        assert(restored.remember("new-network","test-new-password"));
        assert(recovered.accept("c_simulated",name,"new-network",restored)==WifiCommands::Duplicate);
        assert(restored.size()==expected+1);
        assert(recovered.acknowledge("c_simulated"));
        WifiCommands afterAck;assert(afterAck.begin(restored));
        assert(!strlen(afterAck.pendingAck()));
        assert(afterAck.accept("c_simulated",name,"new-network",restored)==WifiCommands::Duplicate);
        assert(restored.size()==expected+1);
      }
      ++checks;
    }
  }
  WifiCommandState state;
  assert(!state.receive("bad.id",WifiAction::Wipe,""));
  assert(!state.receive("c_forget",WifiAction::ForgetCurrent,""));
  assert(state.receive("c_one",WifiAction::Reconnect,""));
  assert(!state.acknowledged("c_one"));assert(state.applied());
  assert(!state.acknowledged("c_other"));assert(state.acknowledged("c_one"));
  assert(!state.receive("c_one",WifiAction::Wipe,""));
  printf("PASS: %u real firmware storage scenarios with power loss, ACK loss, replay, key preservation and network retention.\n",checks);
}
