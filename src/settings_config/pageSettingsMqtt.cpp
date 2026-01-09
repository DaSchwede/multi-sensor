#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "pages.h"
#include "settings_config/settings_common.h"

void pageSettingsMqtt(ESP8266WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String msg = "";

  if (server.method() == HTTP_POST) {
    cfg->mqtt_enabled = server.hasArg("mqtt_enabled");
    saveConfig(*cfg);
    msg = "Gespeichert.";
  }

  String html = pagesHeaderAuth("Einstellungen – MQTT", "/settings/mqtt");
  settingsSendOkBadge(html, msg);

  html += "<form method='POST'>";

  html += "<div class='card'><h2>MQTT</h2>";
  html += "<div class='form-row'>"
          "<label><input type='checkbox' name='mqtt_enabled' " + String(cfg->mqtt_enabled ? "checked" : "") + "> MQTT aktiv</label>"
          "</div>";
  html += "</div>";

  html += "<div class='card'><div class='actions'>"
          "<button class='btn-primary' type='submit'>Speichern</button>"
          "</div></div>";

  html += "</form>";
  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
