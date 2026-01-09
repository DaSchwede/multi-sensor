#include "send_udp.h"
#include <WiFiUdp.h>
#include "ntp_time.h"   // ✅ für ntpEpochUtc()
#include "settings.h"
#include <math.h>


static WiFiUDP udp;

static String udpPayloadCsv(const AppConfig& cfg, const SensorData& d, unsigned long ts){
  String s;
  s.reserve(160);
  s += "id=" + cfg.sensor_id;
  s += ";ts=" + String(ts);

  if (!isnan(d.temperature_c)) s += ";t=" + String(d.temperature_c, 2);
  if (!isnan(d.humidity_rh))   s += ";h=" + String(d.humidity_rh, 2);
  if (!isnan(d.pressure_hpa))  s += ";p=" + String(d.pressure_hpa, 2);

  // CO2 (ppm, ohne Nachkommastellen)
  if (!isnan(d.co2_ppm))       s += ";co2=" + String((int)lroundf(d.co2_ppm));

  return s;
}


static String jsNum2(float v){ return isnan(v) ? "null" : String(v, 2); }
static String jsInt0(float v){ return isnan(v) ? "null" : String((int)lroundf(v)); }

static String udpPayloadJson(const AppConfig& cfg, const SensorData& d, unsigned long ts){
  String s;
  s.reserve(200);
  s += "{\"id\":\"" + cfg.sensor_id + "\",";
  s += "\"ts\":" + String(ts) + ",";
  s += "\"t\":" + jsNum2(d.temperature_c) + ",";
  s += "\"h\":" + jsNum2(d.humidity_rh) + ",";
  s += "\"p\":" + jsNum2(d.pressure_hpa) + ",";
  s += "\"co2_ppm\":" + jsInt0(d.co2_ppm);
  s += "}";
  return s;
}


void SendUDP(const AppConfig& cfg, const SensorData& d) {
  unsigned long ts = ntpEpochUtc();
  if (ts == 0) ts = millis() / 1000;  // Fallback

  String payload = (cfg.udp_format == 1)
    ? udpPayloadJson(cfg, d, ts)
    : udpPayloadCsv(cfg, d, ts);

  udp.beginPacket(cfg.server_udp_ip.c_str(), cfg.server_udp_port);
  udp.write((const uint8_t*)payload.c_str(), payload.length());
  udp.endPacket();
}
