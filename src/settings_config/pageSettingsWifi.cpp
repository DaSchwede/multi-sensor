#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "wifi_mgr.h"
#include "pages.h"
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

  // -----------------------------
  // POST Actions
  // -----------------------------
  if (server.method() == HTTP_POST && server.hasArg("action")) {
    const String a = server.arg("action");

    if (a == "open_portal") {
      // Portal aktivieren, STA darf parallel verbunden bleiben
      wifiMgrRequestStartPortalKeepSta();
      wifiMgrRequestWifiUi(600); // 10 Minuten /wifi auch im Heimnetz erlauben
      msg = "Setup-Portal aktiviert. Im Setup-WLAN oder ueber /wifi (10 Min) konfigurieren.";
      server.sendHeader("Location", "/wifi", true);
      server.send(302, "text/plain", "");
      return;

    }

    if (a == "forget_wifi") {
      wifiMgrRequestForget();
      msg = "WLAN-Daten geloescht. Setup-Portal gestartet.";
      

    }

    if (a == "allow_wifi_ui") {
      wifiMgrRequestWifiUi(600); // 10 Minuten
      msg = "WLAN-Setup-Seite (/wifi) fuer 10 Minuten freigeschaltet.";
      server.sendHeader("Location", "/wifi", true);
      server.send(302, "text/plain", "");
      return;

    }
  }

  const wl_status_t st = WiFi.status();
  const bool connected = (st == WL_CONNECTED);

  String html = pagesHeaderAuth("Einstellungen – WLAN", "/settings/wifi");
  settingsSendOkBadge(html, msg);

  // -----------------------------
  // Status
  // -----------------------------
  html += "<div class='card'><h2>Status</h2><table class='tbl'>";
  html += "<tr><th>Status</th><td>" + wifiStatusStr(st) + "</td></tr>";
  html += "<tr><th>SSID</th><td>" + (WiFi.SSID().length() ? WiFi.SSID() : String("-")) + "</td></tr>";
  html += "<tr><th>IP (STA)</th><td>" + (connected ? WiFi.localIP().toString() : String("-")) + "</td></tr>";
  html += "<tr><th>RSSI</th><td>" + (connected ? String(WiFi.RSSI()) + " dBm" : String("-")) + "</td></tr>";
  html += "<tr><th>Setup-Portal</th><td>" + String(wifiMgrPortalActive() ? "aktiv" : "aus") + "</td></tr>";

  if (wifiMgrPortalActive()) {
    html += "<tr><th>Setup-SSID</th><td>" + wifiMgrApSsid() + "</td></tr>";
    html += "<tr><th>Setup-Passwort</th><td>" + wifiMgrApPass() + "</td></tr>";
    html += "<tr><th>Setup-IP (AP)</th><td>" + WiFi.softAPIP().toString() + "</td></tr>";
  }
  html += "</table></div>";

  // -----------------------------
  // Aktionen
  // -----------------------------
  html += "<form method='POST'>";

  html += "<div class='card'><h2>Aktionen</h2>";
  html += "<div class='hint'>"
          "WLAN-Konfiguration erfolgt ueber das Setup-Portal (<code>/wifi</code>). "
          "Im Portal-Modus wird ein eigenes Setup-WLAN aktiviert."
          "</div>";

  html += "<div class='actions'>";
  html += "<button class='btn-primary' type='submit' name='action' value='open_portal'>WLAN neu einrichten</button>";
  html += "<button class='btn-primary' type='submit' name='action' value='allow_wifi_ui'>/wifi im Heimnetz oeffnen</button>";
  html += "<button class='btn-danger' type='submit' name='action' value='forget_wifi' "
          "onclick=\"return confirm('WLAN-Daten wirklich loeschen?');\">WLAN vergessen</button>";
  html += "</div>";

  html += "</div></form>";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
