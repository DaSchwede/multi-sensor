#include "loxone_udp.h"
#include <WiFiUdp.h>
#include "ntp_time.h"   // ✅ für ntpEpochUtc()
#include "settings.h"

static WiFiUDP udp;

static String udpPayloadCsv(const AppConfig& cfg, const SensorData& d, unsigned long ts){
  String s;
  s.reserve(128);
  s += "id=" + cfg.sensor_id;
  s += ";ts=" + String(ts);
  s += ";t=" + String(d.temperature_c, 2);
  s += ";h=" + String(d.humidity_rh, 2);
  s += ";p=" + String(d.pressure_hpa, 2);
  return s;
}

static String udpPayloadJson(const AppConfig& cfg, const SensorData& d, unsigned long ts){
  String s;
  s.reserve(160);
  s += "{\"id\":\"" + cfg.sensor_id + "\",";
  s += "\"ts\":" + String(ts) + ",";
  s += "\"t\":" + String(d.temperature_c, 2) + ",";
  s += "\"h\":" + String(d.humidity_rh, 2) + ",";
  s += "\"p\":" + String(d.pressure_hpa, 2) + "}";
  return s;
}

void loxoneSendUDP(const AppConfig& cfg, const SensorData& d) {
  unsigned long ts = ntpEpochUtc();
  if (ts == 0) ts = millis() / 1000;  // Fallback

  String payload = (cfg.udp_format == 1)
    ? udpPayloadJson(cfg, d, ts)
    : udpPayloadCsv(cfg, d, ts);

  udp.beginPacket(cfg.loxone_ip.c_str(), cfg.loxone_udp_port);
  udp.write((const uint8_t*)payload.c_str(), payload.length());
  udp.endPacket();
}
