#include <Arduino.h>
#include <WebServer.h>
#include "pages.h"
#include "settings_config/settings_common.h"

void pageSettingsTime(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String msg = "";

  if (server.method() == HTTP_POST) {
    cfg->tz_auto_berlin = server.hasArg("tz_auto_berlin");
    if (server.hasArg("tz_base_seconds")) cfg->tz_base_seconds = toIntSafe(server.arg("tz_base_seconds"), cfg->tz_base_seconds);
    if (server.hasArg("dst_add_seconds")) cfg->dst_add_seconds = toIntSafe(server.arg("dst_add_seconds"), cfg->dst_add_seconds);

    saveConfig(*cfg);
    msg = "Gespeichert.";
  }

  String html = pagesHeaderAuth("Einstellungen – Zeit", "/settings/time");
  settingsSendOkBadge(html, msg);

  html += "<form method='POST'>";

  html += "<div class='card'><h2>Zeit</h2>";
  html += "<div class='form-row'>"
          "<label><input type='checkbox' name='tz_auto_berlin' " + String(cfg->tz_auto_berlin ? "checked" : "") + "> Automatisch (Europe/Berlin)</label>"
          "</div>";
  html += "<div class='form-row'><label>Basis Offset (Sek.)</label>"
          "<input name='tz_base_seconds' type='number' value='" + String(cfg->tz_base_seconds) + "'></div>";
  html += "<div class='form-row'><label>DST Add (Sek.)</label>"
          "<input name='dst_add_seconds' type='number' value='" + String(cfg->dst_add_seconds) + "'></div>";
  html += "</div>";

  html += "<div class='card'><div class='actions'>"
          "<button class='btn-primary' type='submit'>Speichern</button>"
          "</div></div>";

  html += "</form>";
  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
