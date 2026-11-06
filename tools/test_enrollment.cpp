// Compile the actual enrollment client against deterministic HTTP/NVS adapters.
#include "Enrollment.h"
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <cassert>
#include <cstdio>
#include <ctime>
uint32_t mockMillis=0;
static String testKid="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
DeviceKeys::DeviceKeys(){}DeviceKeys::~DeviceKeys(){}
String DeviceKeys::serverKeyId(){return testKid;}
String DeviceKeys::publicRaw(){return "fixture-public-key";}
bool DeviceKeys::signQr(const String& message,String& signature){
  assert(std::string(message.c_str()).find("REGISTER_V1|12345678-1234-1234-1234-123456789012|")==0);
  signature="fixture-signature";return true;
}
std::string challenge(){return "{\"registrationId\":\"12345678-1234-1234-1234-123456789012\",\"nonce\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\",\"expiresAt\":"+std::to_string((int64_t(time(nullptr))+600)*1000)+"}";}
std::string completed(){return "{\"deviceId\":\"0123456789abcdef0123456789abcdef\",\"generation\":1,\"keyId\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\",\"token\":\"BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB\"}";}
void reset(){FakeNvs::data.clear();FakeNvs::failAfter=-1;responses.clear();urls.clear();bodies.clear();mockMillis=0;crashAtPost=false;}
int main(){
  DeviceKeys keys;
  reset();Enrollment first;assert(first.begin("https://project.test",keys,"board"));
  responses={{201,challenge()},{201,completed()}};first.tick(keys);assert(!first.ready());first.tick(keys);assert(first.ready());
  assert(urls.size()==2&&urls[1].find("/complete")!=std::string::npos);
  assert(bodies[0].find("publicKey")!=std::string::npos&&bodies[1].find("proof")!=std::string::npos);
  Enrollment reboot;assert(reboot.begin("https://project.test",keys,"board"));assert(reboot.ready());reboot.tick(keys);assert(urls.size()==2);
  Enrollment other;assert(other.begin("https://other.test",keys,"board"));assert(!other.ready());
  assert(Enrollment::copyIdentity("https://project.test","http://192.168.1.62:8787",keys));
  Enrollment moved;assert(moved.begin("http://192.168.1.62:8787",keys,"board")&&moved.ready());
  assert(moved.token()==first.token()&&moved.deviceId()==first.deviceId());
  assert(!Enrollment::copyIdentity("https://missing.test","https://new.test",keys));
  Enrollment original;assert(original.begin("https://project.test",keys,"board")&&original.ready());
  testKid="CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC";Enrollment wrongKey;assert(!wrongKey.begin("https://project.test",keys,"board"));testKid="AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
  reset();Enrollment pending;assert(pending.begin("https://project.test",keys,"board"));responses={{201,challenge()}};pending.tick(keys);
  Enrollment resumed;assert(resumed.begin("https://project.test",keys,"board"));responses={{201,completed()}};resumed.tick(keys);assert(resumed.ready());
  // Transport failure after sending complete must not consume the one-shot token twice.
  reset();Enrollment ambiguous;assert(ambiguous.begin("https://project.test",keys,"board"));responses={{201,challenge()},{-1,""}};ambiguous.tick(keys);ambiguous.tick(keys);assert(!ambiguous.ready());
  Enrollment recovery;assert(recovery.begin("https://project.test",keys,"board"));mockMillis+=60000;recovery.tick(keys);assert(urls.size()==2);
  // Power loss after durable 'completing' but before the response also blocks retry.
  reset();Enrollment crash;assert(crash.begin("https://project.test",keys,"board"));responses={{201,challenge()}};crash.tick(keys);crashAtPost=true;
  try{crash.tick(keys);assert(false);}catch(const std::runtime_error&){}crashAtPost=false;
  Enrollment afterCrash;assert(afterCrash.begin("https://project.test",keys,"board"));afterCrash.tick(keys);assert(urls.size()==2&&!afterCrash.ready());
  // No network request when the challenge cannot be saved first.
  reset();Enrollment diskFailure;assert(diskFailure.begin("https://project.test",keys,"board"));responses={{201,challenge()}};FakeNvs::failAfter=0;
  try{diskFailure.tick(keys);assert(false);}catch(const FakeNvs::PowerLoss&){}assert(urls.size()==1);
  puts("PASS: real enrollment client, 2 requests, identity reuse, origin/key binding, challenge recovery, ambiguous complete, power loss and storage failure.");
}
