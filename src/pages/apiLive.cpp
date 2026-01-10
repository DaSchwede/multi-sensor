#include <Arduino.h>
#include <WebServer.h>
#include <math.h>
#include "pages.h"
#include "auth.h"

void apiLive(WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  SensorData* live = pagesLive();

  float t   = (live ? live->temperature_c : NAN);
  float h   = (live ? live->humidity_rh   : NAN);
  float p   = (live ? live->pressure_hpa  : NAN);
  float co2 = (live ? live->co2_ppm       : NAN);   // <-- NEU

  uint32_t lr = pagesLastReadMs();
  uint32_t ls = pagesLastSendMs();

  auto jsNum  = [](float v)->String { return isnan(v) ? "null" : String(v, 2); };
  auto jsInt  = [](float v)->String { return isnan(v) ? "null" : String((int)lroundf(v)); }; // <-- NEU

  String json = "{";
  json += "\"temperature_c\":" + jsNum(t) + ",";
  json += "\"humidity_rh\":"   + jsNum(h) + ",";
  json += "\"pressure_hpa\":"  + jsNum(p) + ",";
  json += "\"co2_ppm\":"       + jsInt(co2) + ",";   // <-- NEU
  json += "\"wifi_ok\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"last_read_ms\":" + String(lr) + ",";
  json += "\"last_send_ms\":" + String(ls);
  json += "}";

  server.send(200, "application/json; charset=utf-8", json);
}
