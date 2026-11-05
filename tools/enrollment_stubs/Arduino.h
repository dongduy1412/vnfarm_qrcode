#pragma once
#include "../native_stubs/Arduino.h"
#include <cctype>
#include <cstdarg>
extern uint32_t mockMillis;
inline uint32_t millis(){return mockMillis;}
class Stream {public:virtual ~Stream()=default;virtual size_t write(uint8_t)=0;virtual int read()=0;virtual int peek()=0;virtual int available()=0;virtual void flush()=0;};
struct SerialMock{void printf(const char*,...){}void println(const char*){}};
inline SerialMock Serial;
