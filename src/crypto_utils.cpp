#include "crypto_utils.h"
#include <mbedtls/sha1.h>

String sha1Hex(const String &in) {
  uint8_t hash[20];
  mbedtls_sha1_context ctx;

  mbedtls_sha1_init(&ctx);
  mbedtls_sha1_starts_ret(&ctx);
  mbedtls_sha1_update_ret(&ctx,
                          (const unsigned char*)in.c_str(),
                          in.length());
  mbedtls_sha1_finish_ret(&ctx, hash);
  mbedtls_sha1_free(&ctx);

  char hex[41];
  for (int i = 0; i < 20; i++) {
    sprintf(hex + i * 2, "%02x", hash[i]);
  }
  hex[40] = 0;

  return String(hex);
}
