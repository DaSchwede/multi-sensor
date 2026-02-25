#include <Arduino.h>
#include <WebServer.h>

#include "pages.h"
#include "ntp_time.h"
#include "settings_config/settings_common.h"

void pageNTP(WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  // exakt gleicher Header-Aufbau wie Systeminfo
  String html = pagesHeaderAuth("Info – Zeit / NTP", "/info/time");

  html += "<div style='display:flex; align-items:center; justify-content:space-between; gap:10px;'>";
  html += "<h1 style='margin:0;'>NTP-Informationen</h1>";
  html += "<button class='icon-btn' type='button' title='Aktualisieren' onclick='location.reload()'>&#x21bb;</button>";
  html += "</div>";

  // ===== Konfigurationszusammenfassung =====
  html += "<div class='card'><h2>Konfigurationszusammenfassung</h2><table class='tbl'>";

  html += "<tr><th>Server</th><td>" + cfg->ntp_server + "</td></tr>";

  html += "<tr><th>Zeitzone</th><td>";
  if (cfg->tz_auto_berlin)
    html += "CET-1CEST,M3.5.0,M10.5.0/3";
  else
    html += "Manuell (UTC Offset)";
  html += "</td></tr>";

  html += "<tr><th>Zeitzonenbeschreibung</th><td>";
  html += cfg->tz_auto_berlin ? "Europe/Berlin" : "Manuelle Offset-Konfiguration";
  html += "</td></tr>";

  html += "</table></div>";

  // ===== Aktuelle Zeit =====
  html += "<div class='card'><h2>Aktuelle Zeit</h2><table class='tbl'>";

  bool valid = ntpIsValid();

  html += "<tr><th>Status</th><td>";
  html += valid ? "synchronisiert" : "nicht synchronisiert";
  html += "</td></tr>";

  html += "<tr><th>Lokale Uhrzeit</th><td>";
  html += valid ? ntpDateTimeString(*cfg) : "—";
  html += "</td></tr>";

  html += "<tr><th>UTC Epoch</th><td>";
  unsigned long utc = ntpEpochUtc();
  html += utc ? String(utc) : "—";
  html += "</td></tr>";

  html += "</table></div>";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}