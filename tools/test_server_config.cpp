#include "ServerConfig.h"
using ServerConfig::validManagementOrigin;
static_assert(validManagementOrigin("https://worker.example.com"), "HTTPS origin accepted");
static_assert(!validManagementOrigin("http://worker.example.com"), "HTTP rejected");
static_assert(!validManagementOrigin("https://"), "Empty hostname rejected");
static_assert(!validManagementOrigin("https://worker.example.com/"), "Trailing slash rejected");
static_assert(!validManagementOrigin("https://worker.example.com/api"), "Path rejected");
static_assert(!validManagementOrigin("https://user@worker.example.com"), "Credentials rejected");
static_assert(!validManagementOrigin("https://worker.example.com?x=1"), "Query rejected");
static_assert(!validManagementOrigin("https://worker.example.com#x"), "Fragment rejected");
static_assert(!validManagementOrigin("https://-bad.example.com"), "Leading hyphen rejected");
static_assert(!validManagementOrigin("https://bad-.example.com"), "Trailing hyphen rejected");
static_assert(!validManagementOrigin("https://bad..example.com"), "Empty label rejected");
static_assert(!validManagementOrigin("https://bad.example.com."), "Trailing dot rejected");
static_assert(!validManagementOrigin("https://bad.example.com:443"), "Explicit port unsupported");
