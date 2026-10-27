#include "device_id.h"

static void macBytes(uint8_t out[6]) {
  const uint64_t m = ESP.getEfuseMac();
  for (int i = 0; i < 6; i++) out[i] = (uint8_t)(m >> (8 * i));
}

static String hexBytes(const uint8_t *b, int n) {
  char buf[13];
  for (int i = 0; i < n && i < 6; i++) snprintf(buf + i * 2, 3, "%02X", b[i]);
  buf[n * 2] = 0;
  return String(buf);
}

String deviceShortId() {
  uint8_t b[6];
  macBytes(b);
  return hexBytes(b + 3, 3);
}

String deviceMacHex() {
  uint8_t b[6];
  macBytes(b);
  return hexBytes(b, 6);
}
