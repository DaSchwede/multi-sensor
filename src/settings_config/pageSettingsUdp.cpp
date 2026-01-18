#include <Arduino.h>
#include <WebServer.h>
#include "pages.h"
#include "settings_config/settings_common.h"
#include <math.h>

// Bits (müssen identisch zu send_udp.cpp / settings sein)
static constexpr uint32_t UF_TEMP  = (1u << 0);
static constexpr uint32_t UF_HUM   = (1u << 1);
static constexpr uint32_t UF_PRESS = (1u << 2);
static constexpr uint32_t UF_CO2   = (1u << 3);

static bool isAvailFloat(float v) { return !isnan(v); }

static String fmt2(float v, const char* unit) {
  if (isnan(v)) return String("—");
  return String(v, 2) + unit;
}
static String fmt0(float v, const char* unit) {
  if (isnan(v)) return String("—");
  return String((int)lroundf(v)) + unit;
}

static String badge(bool ok) {
  return ok ? "<span class='badge ok'>verfügbar</span>" : "<span class='badge warn'>keine Daten</span>";
}

void pageSettingsUdp(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String msg = "";

  if (server.method() == HTTP_POST) {
    cfg->udp_enabled = server.hasArg("udp_enabled");

    if (server.hasArg("server_udp_ip"))   cfg->server_udp_ip = server.arg("server_udp_ip");
    if (server.hasArg("server_udp_port")) cfg->server_udp_port = toIntSafe(server.arg("server_udp_port"), cfg->server_udp_port);
    if (server.hasArg("send_interval_ms")) cfg->send_interval_ms = (uint32_t)toIntSafe(server.arg("send_interval_ms"), (int)cfg->send_interval_ms);
    if (server.hasArg("sensor_id"))       cfg->sensor_id = server.arg("sensor_id");

    uint32_t mask = 0;
    if (server.hasArg("f_temp"))  mask |= UF_TEMP;
    if (server.hasArg("f_hum"))   mask |= UF_HUM;
    if (server.hasArg("f_press")) mask |= UF_PRESS;
    if (server.hasArg("f_co2"))   mask |= UF_CO2;
    cfg->udp_fields_mask = mask;

    saveConfig(*cfg);
    msg = "Gespeichert.";
  }

  // Live-Daten holen (kommt aus pagesInit(...))
  SensorData* live = pagesLive();

  const bool aTemp  = live ? isAvailFloat(live->temperature_c) : true;
  const bool aHum   = live ? isAvailFloat(live->humidity_rh)   : true;
  const bool aPress = live ? isAvailFloat(live->pressure_hpa)  : true;
  const bool aCo2   = live ? isAvailFloat(live->co2_ppm)       : true;

  String html = pagesHeaderAuth("Einstellungen – UDP", "/settings/udp");
  settingsSendOkBadge(html, msg);

  html += "<form method='POST'>";

  html += "<div class='card'><h2>UDP</h2>";

  html += "<div class='form-row'><label>UDP aktiv</label>"
          "<label class='switch'>"
          "<input type='checkbox' name='udp_enabled' " + String(cfg->udp_enabled ? "checked" : "") + ">"
          "<span class='slider'></span>"
          "</label></div>";

  html += "<div class='form-row'><label>Server IP / Host</label>"
          "<input name='server_udp_ip' value='" + cfg->server_udp_ip + "'></div>";
  html += "<div class='form-row'><label>UDP Port</label>"
          "<input name='server_udp_port' type='number' value='" + String(cfg->server_udp_port) + "'></div>";
  html += "<div class='form-row'><label>Sendeintervall (ms)</label>"
          "<input name='send_interval_ms' type='number' value='" + String(cfg->send_interval_ms) + "'></div>";
  html += "<div class='form-row'><label>Sensor ID</label>"
          "<input name='sensor_id' value='" + cfg->sensor_id + "'></div>";

  html += "</div>";

  // Dynamische Liste
  html += "<div class='card'><h2>Zu sendende Werte</h2>";
  html += "<div class='hint'>Häkchen bestimmen, was per UDP übertragen wird. Status basiert auf aktuellen Live-Daten.</div>";

  const uint32_t m = cfg->udp_fields_mask;

  html += "<div class='pick-row'>"
          "<label><input type='checkbox' name='f_temp'  " + String((m & UF_TEMP)  ? "checked" : "") + "> Temperatur (°C)</label>"
          + badge(aTemp) + "<span class='val'>" + (live ? fmt2(live->temperature_c, " °C") : "—") + "</span>"
          "</div>";

  html += "<div class='pick-row'>"
          "<label><input type='checkbox' name='f_hum'   " + String((m & UF_HUM)   ? "checked" : "") + "> Luftfeuchte (%)</label>"
          + badge(aHum) + "<span class='val'>" + (live ? fmt2(live->humidity_rh, " %") : "—") + "</span>"
          "</div>";

  html += "<div class='pick-row'>"
          "<label><input type='checkbox' name='f_press' " + String((m & UF_PRESS) ? "checked" : "") + "> Luftdruck (hPa)</label>"
          + badge(aPress) + "<span class='val'>" + (live ? fmt2(live->pressure_hpa, " hPa") : "—") + "</span>"
          "</div>";

  html += "<div class='pick-row'>"
          "<label><input type='checkbox' name='f_co2'   " + String((m & UF_CO2)   ? "checked" : "") + "> CO₂ (ppm)</label>"
          + badge(aCo2) + "<span class='val'>" + (live ? fmt0(live->co2_ppm, " ppm") : "—") + "</span>"
          "</div>";

  html += "<div class='hint'>Wenn alles abgewählt ist, wird absichtlich nichts gesendet.</div>";
  html += "</div>";

  html += "<div class='card'><div class='actions'>"
          "<button class='btn-primary' type='submit'>Speichern</button>"
          "</div></div>";

  html += "</form>";
  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
