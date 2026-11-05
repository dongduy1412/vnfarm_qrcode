#include "NtpGate.h"
#include <cassert>
#include <cstdio>
int main() {
  const int64_t now=1793866400;
  NtpGate boot;
  assert(!boot.ready(now)); // Valid retained RTC, no NTP response this boot.
  assert(!boot.ready(0));
  boot.synchronized(0);
  assert(!boot.ready(now));
  boot.synchronized(now);
  assert(boot.ready(now));
  assert(!boot.ready(0));
  boot.synchronized(now+3600);
  assert(boot.ready(now+3600) && boot.lastSync()==now+3600);
  NtpGate nextBoot;
  assert(!nextBoot.ready(now+3600)); // Restart cannot inherit authorization.
  nextBoot.synchronized(now+3601);
  assert(nextBoot.ready(now+3601));
  nextBoot.synchronized(-1);
  assert(!nextBoot.ready(now+3601));
  puts("PASS: retained RTC blocked, invalid time blocked, first sync, periodic sync and new boot.");
}
