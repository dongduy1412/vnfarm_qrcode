#include "QrContract.h"
extern "C" __declspec(dllexport) unsigned int roundedTime(unsigned int now,unsigned int interval) { return QrContract::timestamp(now,interval); }
extern "C" __declspec(dllexport) bool canonical(char* out,size_t length,const char* did,unsigned int time) { return QrContract::message(out,length,did,time); }
extern "C" __declspec(dllexport) bool rawSignature(const uint8_t* der,size_t length,uint8_t* raw) { return QrContract::derToRaw(der,length,raw); }
