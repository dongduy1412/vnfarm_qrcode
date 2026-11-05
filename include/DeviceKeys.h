#pragma once
#include <Arduino.h>
#include <mbedtls/pk.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

class DeviceKeys {
 public:
  DeviceKeys();
  ~DeviceKeys();
  bool begin();
  bool sign(const String& data, String& signature);
  bool signQr(const String& data, String& signature);
  String publicPem();
  String publicRaw();
  const String& keyId() const { return keyId_; }
 private:
  mbedtls_pk_context key_;
  mbedtls_ctr_drbg_context rng_;
  mbedtls_entropy_context entropy_;
  String keyId_;
  bool ready_ = false;
};

