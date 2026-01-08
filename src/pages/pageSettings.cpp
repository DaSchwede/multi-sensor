#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "pages.h"
#include "auth.h"
#include "settings.h"

static int toIntSafe(const String& s, int defVal){
  if (!s.length()) return defVal;
  return s.toInt();
}

void pageSettings(ESP8266WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  String msg = "";

  if (server.method() == HTTP_POST) {
    // Darstellung
    if (server.hasArg("ui_root_order")) cfg->ui_root_order = server.arg("ui_root_order");
    if (server.hasArg("ui_info_order")) cfg->ui_info_order = server.arg("ui_info_order");

    // Basis-Felder
    if (server.hasArg("server_udp_ip"))        cfg->server_udp_ip = server.arg("server_udp_ip");
    if (server.hasArg("server_udp_port"))  cfg->server_udp_port = toIntSafe(server.arg("server_udp_port"), cfg->server_udp_port);
    if (server.hasArg("send_interval_ms")) cfg->send_interval_ms = (uint32_t)toIntSafe(server.arg("send_interval_ms"), (int)cfg->send_interval_ms);
    if (server.hasArg("sensor_id"))        cfg->sensor_id = server.arg("sensor_id");

    // TZ
    cfg->tz_auto_berlin = server.hasArg("tz_auto_berlin");
    if (server.hasArg("tz_base_seconds"))  cfg->tz_base_seconds = toIntSafe(server.arg("tz_base_seconds"), cfg->tz_base_seconds);
    if (server.hasArg("dst_add_seconds"))  cfg->dst_add_seconds = toIntSafe(server.arg("dst_add_seconds"), cfg->dst_add_seconds);

    // MQTT
    cfg->mqtt_enabled = server.hasArg("mqtt_enabled");

    saveConfig(*cfg);
    msg = "Gespeichert.";
  }

  String html = pagesHeaderAuth("Einstellungen", "/settings");

  if (msg.length()) {
    html += "<div class='card'><p class='badge badge-ok' style='display:inline-block;'>" + msg + "</p></div>";
  }

  html += "<form method='POST'>";

  // UDP
  html += "<div class='card'><h2>UDP</h2>";
  html += "<div class='form-row'><label>Server IP</label>"
          "<input name='server_udp_ip' value='" + cfg->server_udp_ip + "'></div>";
  html += "<div class='form-row'><label>UDP Port</label>"
          "<input name='server_udp_port' type='number' value='" + String(cfg->server_udp_port) + "'></div>";
  html += "<div class='form-row'><label>Sendeintervall (ms)</label>"
          "<input name='send_interval_ms' type='number' value='" + String(cfg->send_interval_ms) + "'></div>";
  html += "<div class='form-row'><label>Sensor ID</label>"
          "<input name='sensor_id' value='" + cfg->sensor_id + "'></div>";
  html += "</div>";

  // Zeit
  html += "<div class='card'><h2>Zeit</h2>";
  html += "<div class='form-row'>"
          "<label><input type='checkbox' name='tz_auto_berlin' " + String(cfg->tz_auto_berlin ? "checked" : "") + "> Automatisch (Europe/Berlin)</label>"
          "</div>";
  html += "<div class='form-row'><label>Basis Offset (Sek.)</label>"
          "<input name='tz_base_seconds' type='number' value='" + String(cfg->tz_base_seconds) + "'></div>";
  html += "<div class='form-row'><label>DST Add (Sek.)</label>"
          "<input name='dst_add_seconds' type='number' value='" + String(cfg->dst_add_seconds) + "'></div>";
  html += "</div>";

  // MQTT
  html += "<div class='card'><h2>MQTT</h2>";
  html += "<div class='form-row'>"
          "<label><input type='checkbox' name='mqtt_enabled' " + String(cfg->mqtt_enabled ? "checked" : "") + "> MQTT aktiv</label>"
          "</div>";
  html += "</div>";

  // Darstellung (nur EINMAL)
  html += "<div class='card'><h2>Darstellung</h2>";

  html += "<div class='form-row'><label>Preset</label>"
          "<select id='ui_preset'>"
          "<option value=''>— auswählen —</option>"
          "<option value='standard'>Standard</option>"
          "<option value='minimal'>Minimal</option>"
          "<option value='debug'>Debug</option>"
          "</select>"
          "<div class='small'>Setzt die Reihenfolge-Felder automatisch.</div>"
          "</div>";

  html += "<div class='actions mt-8'>"
          "<button id='ui_preset_apply' type='button' class='btn btn-secondary'>Preset anwenden</button>"
          "</div>";

  html += "<div class='form-row'><label>Startseite Reihenfolge</label>"
          "<input id='ui_root_order' name='ui_root_order' value='" + cfg->ui_root_order + "'>"
          "<div id='ui_root_hint' class='small'></div>"
          "</div>";

  html += "<div class='chips' id='chips_root'></div>";

  html += "<div class='form-row mt-14'><label>Info-Seite Reihenfolge</label>"
          "<input id='ui_info_order' name='ui_info_order' value='" + cfg->ui_info_order + "'>"
          "<div id='ui_info_hint' class='small'></div>"
          "</div>";

  html += "<div class='chips' id='chips_info'></div>";

  html += "</div>"; // Darstellung

  // Tools
  html += "<div class='card'><h2>Tools</h2>";
  html += "<div class='actions'>"
          "<button class='btn-primary' type='submit'>Speichern</button>"
          "<a class='btn btn-secondary' href='/backup'>Backup</a>"
          "<a class='btn btn-secondary' href='/restore'>Restore</a>"
          "<a class='btn btn-secondary' href='/ota'>OTA Update</a>"
          "<a class='btn btn-secondary' href='/factory_reset'>Factory Reset</a>"
          "</div>";
  html += "</div>";

  html += "</form>";
  html += pagesFooter();

  server.send(200, "text/html; charset=utf-8", html);
}
