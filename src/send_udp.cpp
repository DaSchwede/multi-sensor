#include "send_udp.h"
#include <WiFiUdp.h>
#include <WiFi.h>
#include "ntp_time.h"
#include "settings.h"
#include <math.h>

static WiFiUDP udp;

// --- Payload bleibt wie bei dir (unverändert) ---
static String udpPayloadCsv(const AppConfig& cfg, const SensorData& d, unsigned long ts){
  String s; s.reserve(160);
  s += "id=" + cfg.sensor_id;
  s += ";ts=" + String(ts);
  if (!isnan(d.temperature_c)) s += ";t=" + String(d.temperature_c, 2);
  if (!isnan(d.humidity_rh))   s += ";h=" + String(d.humidity_rh, 2);
  if (!isnan(d.pressure_hpa))  s += ";p=" + String(d.pressure_hpa, 2);
  if (!isnan(d.co2_ppm))       s += ";co2=" + String((int)lroundf(d.co2_ppm));
  return s;
}

static String jsNum2(float v){ return isnan(v) ? "null" : String(v, 2); }
static String jsInt0(float v){ return isnan(v) ? "null" : String((int)lroundf(v)); }

static String udpPayloadJson(const AppConfig& cfg, const SensorData& d, unsigned long ts){
  String s; s.reserve(200);
  s += "{\"id\":\"" + cfg.sensor_id + "\",";
  s += "\"ts\":" + String(ts) + ",";
  s += "\"t\":" + jsNum2(d.temperature_c) + ",";
  s += "\"h\":" + jsNum2(d.humidity_rh) + ",";
  s += "\"p\":" + jsNum2(d.pressure_hpa) + ",";
  s += "\"co2_ppm\":" + jsInt0(d.co2_ppm);
  s += "}";
  return s;
}

// --- Zieladresse Cachen (verhindert DNS im Loop) ---
static IPAddress cachedIp;
static String cachedHost;
static uint32_t lastResolveMs = 0;
static const uint32_t RESOLVE_INTERVAL_MS = 30000; // alle 30s maximal neu auflösen

static bool resolveTarget(const String& hostOrIp, IPAddress &out) {
  // 1) Numeric IP?
  IPAddress ip;
  if (ip.fromString(hostOrIp)) {
    out = ip;
    return true;
  }

  // 2) Hostname nur gelegentlich auflösen
  const uint32_t now = millis();
  if (hostOrIp == cachedHost && cachedIp != IPAddress() && (now - lastResolveMs) < RESOLVE_INTERVAL_MS) {
    out = cachedIp;
    return true;
  }

  // Rate limit DNS
  if ((now - lastResolveMs) < RESOLVE_INTERVAL_MS && hostOrIp != cachedHost) {
    return false; // nicht spammen
  }

  lastResolveMs = now;

  IPAddress resolved;
  // WiFi.hostByName ist besser kontrollierbar als "beginPacket(hostname)"
  if (WiFi.hostByName(hostOrIp.c_str(), resolved)) {
    cachedHost = hostOrIp;
    cachedIp = resolved;
    out = resolved;
    return true;
  }

  // Resolve fehlgeschlagen -> cached nicht überschreiben, einfach skippen
  return false;
}

void SendUDP(const AppConfig& cfg, const SensorData& d) {
  if (WiFi.status() != WL_CONNECTED) return;

  unsigned long ts = ntpEpochUtc();
  if (ts == 0) ts = millis() / 1000;

  const String payload = (cfg.udp_format == 1)
    ? udpPayloadJson(cfg, d, ts)
    : udpPayloadCsv(cfg, d, ts);

  IPAddress target;
  if (!resolveTarget(cfg.server_udp_ip, target)) {
    // Optional: nur selten loggen, sonst nervt es
    static uint32_t lastLog = 0;
    if (millis() - lastLog > 10000) {
      lastLog = millis();
      Serial.println("UDP: Zielhost nicht auflösbar, sende nicht: " + cfg.server_udp_ip);
    }
    return;
  }

  udp.beginPacket(target, cfg.server_udp_port);
  udp.write((const uint8_t*)payload.c_str(), payload.length());
  udp.endPacket();
}
