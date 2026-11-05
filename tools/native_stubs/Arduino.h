#pragma once
// Host-only adapter for compiling the real storage modules, not firmware.
#include <string>
#include <cstring>
#include <cstdint>
#include <cstddef>
class String {
  std::string value_;
public:
  String()=default;String(const char* s):value_(s?s:""){}String(const std::string& s):value_(s){}
  char operator[](size_t i)const{return value_[i];}
  auto begin()const{return value_.begin();}auto end()const{return value_.end();}
  bool startsWith(const char* prefix)const{return value_.rfind(prefix,0)==0;}
  friend String operator+(const String& a,const String& b){return String(a.value_+b.value_);}
  const char* c_str()const{return value_.c_str();}size_t length()const{return value_.size();}bool isEmpty()const{return value_.empty();}
  bool reserve(size_t n){value_.reserve(n);return true;}
  bool concat(const char* s,size_t n){value_.append(s,n);return true;}bool concat(const char* s){value_+=s;return true;}bool concat(char c){value_+=c;return true;}
  bool operator==(const String& b)const{return value_==b.value_;}bool operator!=(const String& b)const{return !(*this==b);}
};
