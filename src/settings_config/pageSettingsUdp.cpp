#include <Arduino.h>
#include <WebServer.h>
#include "pages.h"
#include "settings_config/settings_common.h"

void pageSettingsUdp(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String msg = "";

  if (server.method() == HTTP_POST) {
    if (server.hasArg("server_udp_ip"))   cfg->server_udp_ip = server.arg("server_udp_ip");
    if (server.hasArg("server_udp_port")) cfg->server_udp_port = toIntSafe(server.arg("server_udp_port"), cfg->server_udp_port);
    if (server.hasArg("send_interval_ms")) cfg->send_interval_ms = (uint32_t)toIntSafe(server.arg("send_interval_ms"), (int)cfg->send_interval_ms);
    if (server.hasArg("sensor_id"))       cfg->sensor_id = server.arg("sensor_id");

    saveConfig(*cfg);
    msg = "Gespeichert.";
  }

  String html = pagesHeaderAuth("Einstellungen – UDP", "/settings/udp");
  settingsSendOkBadge(html, msg);

  html += "<form method='POST'>";

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

  html += "<div class='card'><div class='actions'>"
          "<button class='btn-primary' type='submit'>Speichern</button>"
          "</div></div>";

  html += "</form>";
  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
