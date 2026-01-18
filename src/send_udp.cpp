#include "send_udp.h"
#include <WiFiUdp.h>
#include <WiFi.h>
#include "ntp_time.h"
#include "settings.h"
#include <math.h>

static WiFiUDP udp;

// Bitmasken (müssen zu deiner Settings-Seite passen)
static constexpr uint32_t UF_TEMP  = (1u << 0);
static constexpr uint32_t UF_HUM   = (1u << 1);
static constexpr uint32_t UF_PRESS = (1u << 2);
static constexpr uint32_t UF_CO2   = (1u << 3);

static bool hasField(uint32_t mask, uint32_t bit) { return (mask & bit) != 0; }

// --- Payload: CSV (nur selektierte Felder) ---
static String udpPayloadCsv(const AppConfig& cfg, const SensorData& d, unsigned long ts) {
  const uint32_t m = cfg.udp_fields_mask;

  String s; s.reserve(160);
  s += "id=" + cfg.sensor_id;
  s += ";ts=" + String(ts);

  if (hasField(m, UF_TEMP)  && !isnan(d.temperature_c)) s += ";t="   + String(d.temperature_c, 2);
  if (hasField(m, UF_HUM)   && !isnan(d.humidity_rh))   s += ";h="   + String(d.humidity_rh, 2);
  if (hasField(m, UF_PRESS) && !isnan(d.pressure_hpa))  s += ";p="   + String(d.pressure_hpa, 2);
  if (hasField(m, UF_CO2)   && !isnan(d.co2_ppm))       s += ";co2=" + String((int)lroundf(d.co2_ppm));

  return s;
}

static String jsNum2(float v){ return isnan(v) ? "null" : String(v, 2); }
static String jsInt0(float v){ return isnan(v) ? "null" : String((int)lroundf(v)); }

// --- Payload: JSON (nur selektierte Felder) ---
// Tipp: Keys weglassen statt "null", spart Traffic.
static String udpPayloadJson(const AppConfig& cfg, const SensorData& d, unsigned long ts) {
  const uint32_t m = cfg.udp_fields_mask;

  String s; s.reserve(200);
  s += "{\"id\":\"" + cfg.sensor_id + "\"";
  s += ",\"ts\":" + String(ts);

  if (hasField(m, UF_TEMP))  s += ",\"t\":"       + jsNum2(d.temperature_c);
  if (hasField(m, UF_HUM))   s += ",\"h\":"       + jsNum2(d.humidity_rh);
  if (hasField(m, UF_PRESS)) s += ",\"p\":"       + jsNum2(d.pressure_hpa);
  if (hasField(m, UF_CO2))   s += ",\"co2_ppm\":" + jsInt0(d.co2_ppm);

  s += "}";
  return s;
}

// --- Zieladresse cachen (wie bei dir) ---
static IPAddress cachedIp;
static String cachedHost;
static uint32_t lastResolveMs = 0;
static const uint32_t RESOLVE_INTERVAL_MS = 30000; // alle 30s max. neu auflösen

static bool resolveTarget(const String& hostOrIp, IPAddress &out) {
  IPAddress ip;
  if (ip.fromString(hostOrIp)) {
    out = ip;
    return true;
  }

  const uint32_t now = millis();
  if (hostOrIp == cachedHost && cachedIp != IPAddress() && (now - lastResolveMs) < RESOLVE_INTERVAL_MS) {
    out = cachedIp;
    return true;
  }

  if ((now - lastResolveMs) < RESOLVE_INTERVAL_MS && hostOrIp != cachedHost) {
    return false;
  }

  lastResolveMs = now;

  IPAddress resolved;
  if (WiFi.hostByName(hostOrIp.c_str(), resolved)) {
    cachedHost = hostOrIp;
    cachedIp = resolved;
    out = resolved;
    return true;
  }

  return false;
}

void SendUDP(const AppConfig& cfg, const SensorData& d) {
  // 1) Master-Schalter
  if (!cfg.udp_enabled) return;

  // 2) Wenn nichts ausgewählt -> absichtlich nichts senden
  if (cfg.udp_fields_mask == 0) return;

  // 3) WLAN muss stehen
  if (WiFi.status() != WL_CONNECTED) return;

  // Timestamp
  unsigned long ts = ntpEpochUtc();
  if (ts == 0) ts = millis() / 1000;

  // Ziel auflösen
  IPAddress target;
  if (!resolveTarget(cfg.server_udp_ip, target)) {
    static uint32_t lastLog = 0;
    if (millis() - lastLog > 10000) {
      lastLog = millis();
      Serial.println("UDP: Zielhost nicht auflösbar, sende nicht: " + cfg.server_udp_ip);
    }
    return;
  }

  // Payload bauen (nur ausgewählte Felder)
  const String payload = (cfg.udp_format == 1)
    ? udpPayloadJson(cfg, d, ts)
    : udpPayloadCsv(cfg, d, ts);

  // Senden
  udp.beginPacket(target, cfg.server_udp_port);
  udp.write((const uint8_t*)payload.c_str(), payload.length());
  udp.endPacket();
}
