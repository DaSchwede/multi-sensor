#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "pages.h"
#include "auth.h"

static String cardByIdInfo(const String &id) {
  if (id == "system")   return cardSystem();
  if (id == "network")  return cardNetzwerk();
  if (id == "sensor")   return cardSensor();
  if (id == "memory")   return cardSpeicher();
  if (id == "time")     return cardZeit();
  if (id == "settings") return cardAktuelleEinstellungen();
  return ""; // unknown -> skip
}

void pageInfo(ESP8266WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  String html = pagesHeaderAuth("Info", "/info");

  auto order = pagesSplitCsv(cfg->ui_info_order);

  String left, right;
  int idx = 0;
  int unknown = 0;

  for (auto &id : order) {
    String card = cardByIdInfo(id);
    if (card.length()) {
      (idx++ % 2 == 0 ? left : right) += card;
    } else {
      unknown++;
    }
  }

  // Fallback: wenn leer oder alles unknown -> Standard befüllen
  if (idx == 0) {
    left  = cardSystem() + cardNetzwerk() + cardSensor();
    right = cardSpeicher() + cardZeit() + cardAktuelleEinstellungen();
    unknown = 0;
  }

  html += "<div class='columns'>"
          "<div class='col'>" + left + "</div>"
          "<div class='col'>" + right + "</div>"
          "</div>";

  if (unknown > 0) {
    html += "<div class='card'><p class='small'>Hinweis: " + String(unknown) +
            " unbekannte ID(s) in ui_info_order wurden ignoriert.</p></div>";
  }

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
