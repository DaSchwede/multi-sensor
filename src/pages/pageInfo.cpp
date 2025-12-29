#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "pages.h"
#include "auth.h"

void pageInfo(ESP8266WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  String html = pagesHeaderAuth("Info", "/info");

  // 2-Spalten-Stacks (unabhängige Höhen)
  html += "<div class='columns'>";

  html += "<div class='col'>";
  html += cardSystem();
  html += cardNetzwerk();
  html += cardSensor();
  html += "</div>";

  html += "<div class='col'>";
  html += cardSpeicher();
  html += cardZeit();
  html += cardAktuelleEinstellungen();
  html += "</div>";

  html += "</div>"; // columns

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
