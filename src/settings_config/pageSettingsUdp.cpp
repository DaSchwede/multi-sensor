#include <Arduino.h>
#include <WebServer.h>
#include "pages.h"
#include "settings_config/settings_common.h"
#include <math.h>
#include "html_util.h"
#include "mqtt_client.h"
#include "send_udp.h"

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

    const String oldSensorId = cfg->sensor_id;

    if (server.hasArg("server_udp_ip")) {
      cfg->server_udp_ip = server.arg("server_udp_ip");
      cfg->server_udp_ip.trim();
    }
    if (server.hasArg("server_udp_port")) {
      int port = toIntSafe(server.arg("server_udp_port"), cfg->server_udp_port);
      if (port >= 1 && port <= 65535) cfg->server_udp_port = (uint16_t)port;
    }
    if (server.hasArg("send_interval_ms")) {
      long v = toIntSafe(server.arg("send_interval_ms"), (int)cfg->send_interval_ms);
      if (v < 1000) v = 1000;            // min. 1 s (schützt Broker/Miniserver)
      if (v > 3600000L) v = 3600000L;    // max. 1 h
      cfg->send_interval_ms = (uint32_t)v;
    }
    if (server.hasArg("sensor_id")) {
      cfg->sensor_id = server.arg("sensor_id");
      cfg->sensor_id.trim();
      settingsApplyDeviceDefaults(*cfg);  // leer -> Default aus MAC
    }

    uint32_t mask = 0;
    if (server.hasArg("f_temp"))  mask |= UF_TEMP;
    if (server.hasArg("f_hum"))   mask |= UF_HUM;
    if (server.hasArg("f_press")) mask |= UF_PRESS;
    if (server.hasArg("f_co2"))   mask |= UF_CO2;
    cfg->udp_fields_mask = mask;

    if (server.hasArg("udp_format")) {
      const int f = toIntSafe(server.arg("udp_format"), cfg->udp_format);
      cfg->udp_format = (f == 1) ? 1 : 0;
    }

    saveConfig(*cfg);
    if (cfg->sensor_id != oldSensorId) mqttBegin(*cfg);  // MQTT-Topics hängen an der Sensor-ID
    msg = "Gespeichert.";
  }

  // Live-Daten holen (kommt aus pagesInit(...))
  SensorData* live = pagesLive();

  const bool aTemp  = live ? isAvailFloat(live->temperature_c) : true;
  const bool aHum   = live ? isAvailFloat(live->humidity_rh)   : true;
  const bool aPress = live ? isAvailFloat(live->pressure_hpa)  : true;
  const bool aCo2   = live ? isAvailFloat(live->co2_ppm)       : true;

  String html = pagesHeaderAuth("Einstellungen – UDP", "/settings/udp");
  html += pagesTitle("UDP", "Messwerte per UDP an Loxone oder andere Systeme senden.");
  settingsSendOkBadge(html, msg);

  html += "<form method='POST'>";

  html += "<div class='card'><h2>UDP</h2>";

  html += "<div class='form-row'><label>UDP aktiv</label>"
          "<label class='switch'>"
          "<input type='checkbox' name='udp_enabled' " + String(cfg->udp_enabled ? "checked" : "") + ">"
          "<span class='slider'></span>"
          "</label></div>";

  html += "<div class='form-row'><label>Server IP / Host</label>"
          "<input name='server_udp_ip' value='" + htmlEscape(cfg->server_udp_ip) + "'></div>";
  html += "<div class='form-row'><label>UDP Port</label>"
          "<input name='server_udp_port' type='number' min='1' max='65535' value='" + String(cfg->server_udp_port) + "'></div>";
  html += "<div class='form-row'><label>Sendeintervall (ms)</label>"
          "<input name='send_interval_ms' type='number' min='1000' max='3600000' step='100' value='" + String(cfg->send_interval_ms) + "'></div>";
  html += "<div class='form-row'><label>Sensor ID</label>"
          "<input name='sensor_id' maxlength='40' value='" + htmlEscape(cfg->sensor_id) + "'></div>";
  html += "<div class='hint'>Die Sensor-ID wird auch für MQTT-Topics und Home Assistant verwendet. Leer = Standard aus der MAC-Adresse.</div>";

  html += "<div class='form-row'><label>Format</label><select name='udp_format' id='udp_format'>"
          "<option value='0'" + String(cfg->udp_format == 1 ? "" : " selected") + ">Text (CSV): id=…;ts=…;t=…</option>"
          "<option value='1'" + String(cfg->udp_format == 1 ? " selected" : "") + ">JSON: {\"id\":…,\"t\":…}</option>"
          "</select></div>";
  if (live) {
    html += "<div class='hint'><b>So sieht das Paket gerade aus:</b><br><code style='word-break:break-all'>" +
            htmlEscape(udpBuildPayload(*cfg, *live)) + "</code></div>";
  }
  html += "<div class='hint'><b>Text (CSV)</b> ist einfach in Loxone auszuwerten (Befehlserkennung z.B. <code>t=\\v</code>). "
          "<b>JSON</b> passt für Node-RED, ioBroker oder eigene Skripte. Fehlende Werte: im Text weggelassen, im JSON <code>null</code>.</div>";

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
