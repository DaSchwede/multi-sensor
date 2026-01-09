#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "pages.h"
#include "settings_config/settings_common.h"

void pageSettingsUi(ESP8266WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String msg = "";

  if (server.method() == HTTP_POST) {
    if (server.hasArg("ui_root_order")) cfg->ui_root_order = server.arg("ui_root_order");
    if (server.hasArg("ui_info_order")) cfg->ui_info_order = server.arg("ui_info_order");

    saveConfig(*cfg);
    msg = "Gespeichert.";
  }

  String html = pagesHeaderAuth("Einstellungen – Darstellung", "/settings/ui");
  settingsSendOkBadge(html, msg);

  html += "<form method='POST'>";

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

  html += "</div>"; // card

  html += "<div class='card'><div class='actions'>"
          "<button class='btn-primary' type='submit'>Speichern</button>"
          "</div></div>";

  html += "</form>";
  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
