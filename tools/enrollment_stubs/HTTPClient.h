#pragma once
#include "WiFi.h"
#include <vector>
#include <stdexcept>
struct FakeResponse{int code;std::string body;};
inline std::vector<FakeResponse> responses;
inline std::vector<std::string> urls,bodies;
inline bool crashAtPost=false;
constexpr int HTTPC_DISABLE_FOLLOW_REDIRECTS=0;
class HTTPClient {
 String url_;FakeResponse response_;
public:
 void setConnectTimeout(int){}void setTimeout(int){}void setFollowRedirects(int){}void setUserAgent(const char*){}void addHeader(const char*,const char*){}
 bool begin(WiFiClient&,const String& url){url_=url;return true;}
 int POST(const String& body){urls.push_back(url_.c_str());bodies.push_back(body.c_str());if(crashAtPost)throw std::runtime_error("power loss");if(responses.empty())throw std::runtime_error("Unexpected HTTP request");response_=responses.front();responses.erase(responses.begin());return response_.code;}
 int writeToStream(Stream* stream){for(auto c:response_.body)if(stream->write(c)!=1)return -1;return response_.body.size();}
 void end(){}
};
