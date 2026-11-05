#pragma once
#include "Arduino.h"
#include <map>
#include <stdexcept>
namespace FakeNvs {
inline std::map<std::string,std::map<std::string,std::string>> data;
inline int failAfter=-1;
struct PowerLoss:std::exception{};
inline void write(){if(failAfter==0)throw PowerLoss();if(failAfter>0)--failAfter;}
}
class Preferences {
  std::string name_;
public:
  bool begin(const char* name,bool){name_=name;return true;}void end(){}
  bool isKey(const char* key){return FakeNvs::data[name_].count(key)>0;}
  String getString(const char* key,const char* fallback){auto& m=FakeNvs::data[name_];return m.count(key)?String(m[key]):String(fallback);}
  size_t putString(const char* key,const String& value){FakeNvs::write();FakeNvs::data[name_][key]=value.c_str();return value.length();}
  size_t putBool(const char* key,bool value){FakeNvs::write();FakeNvs::data[name_][key]=value?"true":"false";return 1;}
  bool getBool(const char* key,bool fallback){auto& m=FakeNvs::data[name_];return m.count(key)?m[key]=="true":fallback;}
  bool remove(const char* key){FakeNvs::write();return FakeNvs::data[name_].erase(key)>0;}
};
