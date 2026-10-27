#include "crypto_utils.h"
#include <mbedtls/sha1.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <esp_system.h>

static const uint32_t PBKDF2_ITERATIONS = 1000;   // ~0,1 s auf dem ESP32-C3 (SHA-256 in Hardware)
static const size_t   SALT_LEN = 16;
static const size_t   KEY_LEN  = 32;

static String toHex(const uint8_t *b, size_t n) {
  static const char *H = "0123456789abcdef";
  String s;
  s.reserve(n * 2);
  for (size_t i = 0; i < n; i++) { s += H[b[i] >> 4]; s += H[b[i] & 0x0F]; }
  return s;
}

static bool fromHex(const String &s, uint8_t *out, size_t n) {
  if (s.length() != n * 2) return false;
  auto v = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  for (size_t i = 0; i < n; i++) {
    const int hi = v(s[2 * i]), lo = v(s[2 * i + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

// Vergleich in konstanter Zeit (verrät nicht, ab welchem Zeichen es abweicht)
static bool equalsCT(const String &a, const String &b) {
  if (a.length() != b.length()) return false;
  uint8_t d = 0;
  for (size_t i = 0; i < a.length(); i++) d |= (uint8_t)(a[i] ^ b[i]);
  return d == 0;
}

String sha1Hex(const String &in) {
  uint8_t hash[20];
  mbedtls_sha1_context ctx;

  mbedtls_sha1_init(&ctx);
  mbedtls_sha1_starts_ret(&ctx);
  mbedtls_sha1_update_ret(&ctx, (const unsigned char*)in.c_str(), in.length());
  mbedtls_sha1_finish_ret(&ctx, hash);
  mbedtls_sha1_free(&ctx);

  return toHex(hash, sizeof(hash));
}

static bool pbkdf2(const String &password, const uint8_t *salt, size_t saltLen,
                   uint32_t iterations, uint8_t *out, size_t outLen) {
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  bool ok = info && mbedtls_md_setup(&ctx, info, 1) == 0 &&
            mbedtls_pkcs5_pbkdf2_hmac(&ctx,
                                      (const unsigned char *)password.c_str(), password.length(),
                                      salt, saltLen, iterations, outLen, out) == 0;
  mbedtls_md_free(&ctx);
  return ok;
}

String passwordHash(const String &password) {
  uint8_t salt[SALT_LEN];
  esp_fill_random(salt, sizeof(salt));
  uint8_t key[KEY_LEN];
  if (!pbkdf2(password, salt, sizeof(salt), PBKDF2_ITERATIONS, key, sizeof(key))) {
    return String();   // sollte nie passieren
  }
  return "pbkdf2$" + String(PBKDF2_ITERATIONS) + "$" + toHex(salt, sizeof(salt)) + "$" + toHex(key, sizeof(key));
}

bool passwordHashIsLegacy(const String &stored) {
  return stored.length() == 40 && stored.indexOf('$') < 0;
}

// zerlegt "pbkdf2$iter$salt$hash"
static bool parsePbkdf2(const String &stored, uint32_t &iter, uint8_t *salt, uint8_t *key) {
  if (!stored.startsWith("pbkdf2$")) return false;
  const int a = stored.indexOf('$', 7);
  if (a < 0) return false;
  const int b = stored.indexOf('$', a + 1);
  if (b < 0) return false;
  const long it = stored.substring(7, a).toInt();
  if (it < 1 || it > 100000) return false;
  iter = (uint32_t)it;
  return fromHex(stored.substring(a + 1, b), salt, SALT_LEN) &&
         fromHex(stored.substring(b + 1), key, KEY_LEN);
}

bool passwordHashLooksValid(const String &stored) {
  if (passwordHashIsLegacy(stored)) return true;
  uint32_t it; uint8_t salt[SALT_LEN], key[KEY_LEN];
  return parsePbkdf2(stored, it, salt, key);
}

bool passwordVerify(const String &password, const String &stored) {
  if (passwordHashIsLegacy(stored)) {
    return equalsCT(sha1Hex(password), stored);
  }
  uint32_t iter;
  uint8_t salt[SALT_LEN], key[KEY_LEN], test[KEY_LEN];
  if (!parsePbkdf2(stored, iter, salt, key)) return false;
  if (!pbkdf2(password, salt, SALT_LEN, iter, test, KEY_LEN)) return false;
  uint8_t d = 0;
  for (size_t i = 0; i < KEY_LEN; i++) d |= (uint8_t)(test[i] ^ key[i]);
  return d == 0;
}
