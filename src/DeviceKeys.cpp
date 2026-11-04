#include "DeviceKeys.h"
#include <Preferences.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha256.h>

DeviceKeys::DeviceKeys() {
  mbedtls_pk_init(&key_);
  mbedtls_ctr_drbg_init(&rng_);
  mbedtls_entropy_init(&entropy_);
}

DeviceKeys::~DeviceKeys() {
  mbedtls_pk_free(&key_);
  mbedtls_ctr_drbg_free(&rng_);
  mbedtls_entropy_free(&entropy_);
}

bool DeviceKeys::begin() {
  const char* purpose = "vnfarm-qr-signing-v1";
  if (mbedtls_ctr_drbg_seed(&rng_, mbedtls_entropy_func, &entropy_,
      reinterpret_cast<const unsigned char*>(purpose), strlen(purpose)) != 0) return false;
  Preferences store;
  if (!store.begin("qr-keys", false)) return false;
  size_t storedSize = store.getBytesLength("private-der");
  unsigned char der[512] = {};
  bool ok = false;
  if (storedSize > 0) {
    // Never silently replace an unreadable key: the server still trusts the old key.
    ok = storedSize <= sizeof(der) && store.getBytes("private-der", der, storedSize) == storedSize;
    if (ok) ok = mbedtls_pk_parse_key(&key_, der, storedSize, nullptr, 0) == 0;
    if (ok) ok = mbedtls_pk_get_type(&key_) == MBEDTLS_PK_ECKEY &&
        mbedtls_pk_ec(key_)->grp.id == MBEDTLS_ECP_DP_SECP256R1;
  } else {
    ok = mbedtls_pk_setup(&key_, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) == 0;
    if (ok) ok = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(key_),
        mbedtls_ctr_drbg_random, &rng_) == 0;
    if (ok) {
      int size = mbedtls_pk_write_key_der(&key_, der, sizeof(der));
      ok = size > 0 && store.putBytes("private-der", der + sizeof(der) - size, size) == size;
    }
  }
  memset(der, 0, sizeof(der));
  store.end();
  if (!ok) return false;
  unsigned char publicDer[256] = {};
  int size = mbedtls_pk_write_pubkey_der(&key_, publicDer, sizeof(publicDer));
  if (size <= 0) return false;
  unsigned char digest[32];
  if (mbedtls_sha256_ret(publicDer + sizeof(publicDer) - size, size, digest, 0) != 0) return false;
  char hex[65];
  for (size_t i = 0; i < sizeof(digest); ++i) snprintf(hex + i * 2, 3, "%02x", digest[i]);
  keyId_ = hex;
  ready_ = true;
  return true;
}

String DeviceKeys::publicPem() {
  unsigned char pem[256] = {};
  if (!ready_ || mbedtls_pk_write_pubkey_pem(&key_, pem, sizeof(pem)) != 0) return "";
  return String(reinterpret_cast<char*>(pem));
}

bool DeviceKeys::sign(const String& data, String& signature) {
  if (!ready_) return false;
  unsigned char digest[32];
  if (mbedtls_sha256_ret(reinterpret_cast<const unsigned char*>(data.c_str()),
      data.length(), digest, 0) != 0) return false;
  unsigned char der[MBEDTLS_PK_SIGNATURE_MAX_SIZE];
  size_t size = 0;
  if (mbedtls_pk_sign(&key_, MBEDTLS_MD_SHA256, digest, sizeof(digest), der, &size,
      mbedtls_ctr_drbg_random, &rng_) != 0) return false;
  // Verify locally before displaying; this does not replace backend verification.
  if (mbedtls_pk_verify(&key_, MBEDTLS_MD_SHA256, digest, sizeof(digest), der, size) != 0) return false;
  unsigned char base64[256];
  size_t outSize = 0;
  if (mbedtls_base64_encode(base64, sizeof(base64), &outSize, der, size) != 0) return false;
  base64[outSize] = 0;
  signature = reinterpret_cast<char*>(base64);
  signature.replace("+", "-");
  signature.replace("/", "_");
  while (signature.endsWith("=")) signature.remove(signature.length() - 1);
  return true;
}

