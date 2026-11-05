#include "ServerOrigin.h"
#include <cassert>
int main() {
  const char* good[] = {"http://192.168.1.12:8787", "http://10.0.0.1", "http://172.16.0.1:65535", "https://project.workers.dev", "https://project.example.com:443"};
  const char* bad[] = {"http://8.8.8.8", "http://localhost:8787", "http://127.0.0.1:8787", "http://172.32.0.1", "http://192.168.1.12:0", "http://192.168.1.12:65536", "http://192.168.1.12:-1", "http://192.168.001.12:8787", "http://192.168.1.999", "http://192.168.1.12/api", "http://192.168.1.12?x=1", "https://user@project.dev", "https://project.dev/", "https://project.dev:abc", "https://project.dev#x"};
  for (auto value : good) assert(ServerOrigin::valid(value));
  for (auto value : bad) assert(!ServerOrigin::valid(value));
}
