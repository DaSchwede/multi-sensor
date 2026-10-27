#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "pages.h"
#include "wifi_mgr.h"
#include "html_util.h"
#include "settings_config/settings_common.h"

void pageSettingsWifi(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  // Actions (POST)
  if (server.method() == HTTP_POST && server.hasArg("action")) {
    const String a = server.arg("action");

    if (a == "allow_wifi_ui") {
      wifiMgrRequestWifiUi(600);          // 10 Minuten /wifi im Heimnetz, ohne Setup-AP
      server.sendHeader("Location", "/wifi", true);
      server.send(302, "text/plain", "");
      return;
    }
    if (a == "start_portal") {
      wifiMgrRequestWifiUi(600);          // 10 Minuten /wifi erlauben
      wifiMgrRequestStartPortalKeepSta(); // Portal starten, STA bleibt verbunden
      server.sendHeader("Location", "/wifi", true);
      server.send(302, "text/plain", "");
      return;
    }
    if (a == "forget_wifi") {
      wifiMgrRequestWifiUi(600);
      wifiMgrRequestForget();
      server.sendHeader("Location", "/wifi", true);
      server.send(302, "text/plain", "");
      return;
    }
  }

  const bool connected = (WiFi.status() == WL_CONNECTED);

  String html = pagesHeaderAuth("Einstellungen – WLAN", "/settings/wifi");
  html += pagesTitle("WLAN", "Verbindungsstatus, Setup-WLAN und Zugangsdaten.");

  html += "<div class='card'><h2>Status</h2><table class='tbl'>";
  html += "<tr><th>Verbunden</th><td>" + String(connected ? "Ja" : "Nein") + "</td></tr>";
  html += "<tr><th>SSID</th><td>" + (WiFi.SSID().length() ? htmlEscape(WiFi.SSID()) : String("-")) + "</td></tr>";
  html += "<tr><th>IP</th><td>" + (connected ? WiFi.localIP().toString() : String("-")) + "</td></tr>";
  html += "<tr><th>RSSI</th><td>" + (connected ? String(WiFi.RSSI()) + " dBm" : String("-")) + "</td></tr>";
  html += "<tr><th>Portal aktiv</th><td>" + String(wifiMgrPortalActive() ? "Ja" : "Nein") + "</td></tr>";
  html += "<tr><th>Setup-AP</th><td>" + htmlEscape(wifiMgrApSsid()) + "</td></tr>";
  html += "<tr><th>Setup-AP Passwort</th><td><code>" + htmlEscape(wifiMgrApPass()) + "</code></td></tr>";
  html += "</table>";
  html += "<div class='hint'>Der Setup-AP öffnet sich automatisch, wenn das WLAN mehrfach nicht erreichbar ist. "
          "Er ist mit dem Passwort oben geschützt – bitte notieren. "
          "Nur bei der Ersteinrichtung (noch keine WLAN-Daten) ist er offen.</div>";
  html += "</div>";

  html += "<form method='POST'>";
  html += "<div class='card'><h2>Aktionen</h2><div class='actions'>";
  html += "<button class='btn-primary' type='submit' name='action' value='allow_wifi_ui'>WLAN ändern (im Heimnetz)</button>";
  html += "<button class='btn-secondary' type='submit' name='action' value='start_portal'>Setup-WLAN einschalten</button>";
  html += "<button class='btn-danger' type='submit' name='action' value='forget_wifi' "
          "onclick=\"return confirm('WLAN-Daten wirklich löschen?');\">WLAN vergessen</button>";
  html += "</div>";
  html += "<div class='hint'><b>WLAN ändern (im Heimnetz):</b> öffnet die WLAN-Auswahl für 10 Minuten hier im Browser. "
          "<b>Setup-WLAN einschalten:</b> startet zusätzlich das Setup-WLAN, z.B. um mit dem Handy direkt zu verbinden. "
          "Fällt das WLAN länger als 3 Minuten aus, startet das Setup-WLAN automatisch.</div>";
  html += "</div></form>";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
