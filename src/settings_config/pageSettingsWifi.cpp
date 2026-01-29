#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "pages.h"
#include "wifi_mgr.h"
#include "settings_config/settings_common.h"

static String wifiStatusStr(wl_status_t s) {
  switch (s) {
    case WL_IDLE_STATUS:     return "IDLE";
    case WL_NO_SSID_AVAIL:   return "SSID nicht verfuegbar";
    case WL_SCAN_COMPLETED:  return "SCAN abgeschlossen";
    case WL_CONNECTED:       return "Verbunden";
    case WL_CONNECT_FAILED:  return "Connect fehlgeschlagen";
    case WL_CONNECTION_LOST: return "Verbindung verloren";
    case WL_DISCONNECTED:    return "Getrennt";
    default:                 return "Unbekannt";
  }
}

void pageSettingsWifi(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String msg = "";

  // Actions (POST)
  if (server.method() == HTTP_POST) {
    if (server.hasArg("action")) {
      String a = server.arg("action");

      if (a == "start_portal") {
        wifiMgrRequestWifiUi(300);          // 5 Minuten /wifi erlauben
        wifiMgrRequestStartPortalKeepSta(); // Portal starten, STA bleibt verbunden

        server.sendHeader("Location", "/wifi", true);
        server.send(302, "text/plain", "");
      return;

      } else if (a == "forget_wifi") {
        wifiMgrRequestWifiUi(300);
        wifiMgrRequestForget();
        server.sendHeader("Location", "/wifi", true);
        server.send(302, "text/plain", "");
        return;
      }
    }
  }

  // Statusdaten
  const bool connected   = (WiFi.status() == WL_CONNECTED);
  const bool portal      = wifiMgrPortalActive();
  const bool hasCreds    = wifiMgrHasCredentials();
  const String staSsid   = WiFi.SSID();
  const String staIp     = WiFi.localIP().toString();
  const int rssi         = WiFi.RSSI();
  const String apSsid    = wifiMgrApSsid();

  String html = pagesHeaderAuth("Einstellungen – WLAN", "/settings/wifi");
  settingsSendOkBadge(html, msg);

  html += "<div class='card'><h2>Status</h2><table class='tbl'>";
  html += "<tr><th>Verbunden</th><td>" + String(WiFi.status() == WL_CONNECTED ? "Ja" : "Nein") + "</td></tr>";
  html += "<tr><th>SSID</th><td>" + (WiFi.SSID().length() ? WiFi.SSID() : String("-")) + "</td></tr>";
  html += "<tr><th>IP</th><td>" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("-")) + "</td></tr>";
  html += "<tr><th>RSSI</th><td>" + (WiFi.status() == WL_CONNECTED ? String(WiFi.RSSI()) + " dBm" : String("-")) + "</td></tr>";
  html += "<tr><th>Portal aktiv</th><td>" + String(wifiMgrPortalActive() ? "Ja" : "Nein") + "</td></tr>";
  html += "<tr><th>Setup-AP</th><td>" + wifiMgrApSsid() + "</td></tr>";
  html += "</table></div>";

  html += "<form method='POST'>";
  html += "<div class='card'><h2>Aktionen</h2><div class='actions'>";
  html += "<button class='btn-primary' type='submit' name='action' value='start_portal'>WLAN aendern</button>";
  html += "<button class='btn-danger' type='submit' name='action' value='forget_wifi' "
          "onclick=\"return confirm('WLAN-Daten wirklich loeschen?');\">WLAN vergessen</button>";
  html += "</div></div></form>";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}