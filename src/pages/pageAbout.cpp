#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "pages.h"
#include "auth.h"
#include "version.h"

void pageAbout(ESP8266WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  String html = pagesHeaderAuth("Über", "/about");

  html += "<div class='card'>";
  html += "<h2>" + String(FW_NAME) + "</h2>";

  html += "<table class='tbl'>";
  html += "<tr><th>Firmware</th><td>" + String(FW_NAME) + "</td></tr>";
  html += "<tr><th>Version</th><td>" + String(FW_VERSION) + "</td></tr>";
  html += "<tr><th>Build-Datum</th><td>" + String(FW_DATE) + "</td></tr>";
  html += "<tr><th>Autor</th><td>" + String(FW_AUTHOR) + "</td></tr>";
  html += "<tr><th>Copyright</th><td>" + String(FW_COPYRIGHT) + "</td></tr>";
  html += "<tr><th>Lizenz</th><td><a href='/license'>anzeigen</a></td></tr>";
  html += "<tr><th>Änderungen</th><td>" + String(FW_DESCRIPTION) + "</td></tr>";
  html += "</table>";

  html += "</div>";
  html += pagesFooter();

  server.send(200, "text/html; charset=utf-8", html);
}
