#pragma once
#include <atomic>
#include <cstdint>

// RAM only: a plausible RTC clock alone must never authorize a new boot.
class NtpGate {
public:
  static constexpr uint32_t earliest = 1704067200;
  void synchronized(int64_t epoch) {
    confirmed_.store(epoch >= earliest && epoch <= UINT32_MAX ? static_cast<uint32_t>(epoch) : 0);
  }
  uint32_t lastSync() const { return confirmed_.load(); }
  bool ready(int64_t now) const { return lastSync() >= earliest && now >= earliest; }
private:
  std::atomic<uint32_t> confirmed_{0};
};
