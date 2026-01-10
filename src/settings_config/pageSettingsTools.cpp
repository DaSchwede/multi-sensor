#include <Arduino.h>
#include <WebServer.h>
#include "pages.h"
#include "settings_config/settings_common.h"

void pageSettingsTools(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String html = pagesHeaderAuth("Einstellungen – Tools", "/settings/tools");

  html += "<div class='card'><h2>Tools</h2>";
  html += "<div class='actions'>"
          "<a class='btn btn-secondary' href='/backup'>Backup</a>"
          "<a class='btn btn-secondary' href='/restore'>Restore</a>"
          "<a class='btn btn-secondary' href='/ota'>OTA Update</a>"
          "<a class='btn btn-secondary' href='/factory_reset'>Factory Reset</a>"
          "</div>";
  html += "</div>";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
