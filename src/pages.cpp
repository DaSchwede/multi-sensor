#include "pages.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <vector>

#include "auth.h"
#include "version.h"
#include "ntp_time.h"

// ============================================================================
// Globals (gesetzt über pagesInit)
// ============================================================================
static AppConfig*   gCfg       = nullptr;
static SensorData*  gLive      = nullptr;
static uint32_t*    gLastReadMs = nullptr;
static uint32_t*    gLastSendMs = nullptr;

void pagesInit(AppConfig &cfg, SensorData *liveData, uint32_t *lastReadMs, uint32_t *lastSendMs) {
  gCfg = &cfg;
  gLive = liveData;
  gLastReadMs = lastReadMs;
  gLastSendMs = lastSendMs;
}

// Getter (für Subpages)
AppConfig* pagesCfg() { return gCfg; }
SensorData* pagesLive() { return gLive; }
uint32_t pagesLastReadMs() { return gLastReadMs ? *gLastReadMs : 0; }
uint32_t pagesLastSendMs() { return gLastSendMs ? *gLastSendMs : 0; }

// ============================================================================
// Small helpers
// ============================================================================
static String navLink(const String& href, const String& label, const String& current) {
  String cls = (href == current) ? "active" : "";
  return "<a class='" + cls + "' href='" + href + "'>" + label + "</a>";
}

static String uptimeString() {
  uint32_t s = millis() / 1000;
  uint32_t d = s / 86400; s %= 86400;
  uint32_t h = s / 3600;  s %= 3600;
  uint32_t m = s / 60;    s %= 60;

  char buf[32];
  snprintf(buf, sizeof(buf), "%ud %02u:%02u:%02u", d, h, m, s);
  return String(buf);
}

static String loadLicenseText() {
  if (LittleFS.exists("/license.txt")) {
    File f = LittleFS.open("/license.txt", "r");
    String s = f.readString();
    f.close();
    return s;
  }
  return String(
    "license.txt nicht gefunden.\n"
    "Bitte LittleFS hochladen: pio run -t uploadfs\n"
  );
}

static std::vector<String> splitCsv(const String& csv) {
  std::vector<String> out;
  int start = 0;

  while (true) {
    int idx = csv.indexOf(',', start);
    if (idx < 0) {
      String part = csv.substring(start);
      part.trim();
      if (part.length()) out.push_back(part);
      break;
    }
    String part = csv.substring(start, idx);
    part.trim();
    if (part.length()) out.push_back(part);
    start = idx + 1;
  }
  return out;
}

// Exported wrappers (für Subpages)
String pagesUptimeString() { return uptimeString(); }
String pagesLoadLicenseText() { return loadLicenseText(); }
std::vector<String> pagesSplitCsv(const String &csv) { return splitCsv(csv); }

// ============================================================================
// Header/Footer (Auth/Public)
// ============================================================================
static String headerHtmlAuth(const String &title, const String &currentPath) {
  return String(
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<link rel='stylesheet' href='/style.css'>"
    "<script defer src='/script.js'></script>"
    "<link rel='icon' type='image/svg+xml' href='/favicon.svg'>"
    "<title>") + String(FW_NAME) + " - " + title + "</title>"
    "</head><body>"
    "<div class='topbar'><img src='/logo_name_weiss.svg' alt='Multi Sensors' class='logo'></div>"
    "<div class='menubar'>"
      + navLink("/", "Startseite", currentPath)
      + navLink("/info", "Info", currentPath)
      + navLink("/settings", "Einstellungen", currentPath)
      + navLink("/about", "Über", currentPath)
      + "<span class='right'><a href='/logout'>Abmelden</a></span>"
    "</div>"
    "<div class='content'>";
}

static String headerHtmlPublic(ESP8266WebServer &server, const String &title, const String &currentPath) {
  bool authed = isAuthenticated(server);

  String menu;
  if (authed) {
    menu =
      navLink("/", "Startseite", currentPath) +
      navLink("/info", "Info", currentPath) +
      navLink("/settings", "Einstellungen", currentPath) +
      navLink("/about", "Über", currentPath) +
      navLink("/license", "Lizenz", currentPath) +
      "<span class='right'><a href='/logout'>Abmelden</a></span>";
  } else {
    menu =
      navLink("/login", "Login", currentPath) +
      navLink("/license", "Lizenz", currentPath) +
      "<span class='right'><a href='/login'>Anmelden</a></span>";
  }

  return String(
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<link rel='stylesheet' href='/style.css'>"
    "<script defer src='/script.js'></script>"
    "<link rel='icon' type='image/svg+xml' href='/favicon.svg'>"
    "<title>") + String(FW_NAME) + " - " + title + "</title>"
    "</head><body>"
    "<div class='topbar'>" + String(FW_NAME) + "</div>"
    "<div class='menubar'>" + menu + "</div>"
    "<div class='content'>";
}

static String footerHtml() {
  return "</div></body></html>";
}

// Exported wrappers
String pagesHeaderAuth(const String &title, const String &currentPath) { return headerHtmlAuth(title, currentPath); }
String pagesHeaderPublic(ESP8266WebServer &server, const String &title, const String &currentPath) { return headerHtmlPublic(server, title, currentPath); }
String pagesFooter() { return footerHtml(); }

// ============================================================================
// Card builders (werden von pageInfo/pageRoot genutzt)
// ============================================================================
String cardSystem() {
  String h;
  h += "<div class='card'><h2>System</h2><table class='tbl'>";
  h += "<tr><th>Firmware</th><td>" + String(FW_NAME) + "</td></tr>";
  h += "<tr><th>Version</th><td>" + String(FW_VERSION) + "</td></tr>";
  h += "<tr><th>Build</th><td>" + String(FW_DATE) + "</td></tr>";
  h += "<tr><th>Chip ID</th><td>" + String(ESP.getChipId(), HEX) + "</td></tr>";
  h += "<tr><th>CPU</th><td>" + String(ESP.getCpuFreqMHz()) + " MHz</td></tr>";
  h += "<tr><th>Uptime</th><td>" + uptimeString() + "</td></tr>";
  h += "</table></div>";
  return h;
}

String cardNetzwerk() {
  String h;
  h += "<div class='card'><h2>Netzwerk</h2><table class='tbl'>";
  h += "<tr><th>Status</th><td>" + String(WiFi.status() == WL_CONNECTED ? "Verbunden" : "Getrennt") + "</td></tr>";
  h += "<tr><th>SSID</th><td>" + WiFi.SSID() + "</td></tr>";
  h += "<tr><th>IP</th><td>" + WiFi.localIP().toString() + "</td></tr>";
  h += "<tr><th>Gateway</th><td>" + WiFi.gatewayIP().toString() + "</td></tr>";
  h += "<tr><th>RSSI</th><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
  h += "<tr><th>mDNS</th><td>http://multi-sensor.local/</td></tr>";
  h += "</table></div>";
  return h;
}

String cardSensor() {
  String h;
  h += "<div class='card'><h2>Sensor</h2><table class='tbl'>";

  if (!gLive) {
    h += "<tr><th>Status</th><td>Keine Daten</td></tr>";
  } else {
    h += "<tr><th>Temperatur</th><td>" + String(gLive->temperature_c, 1) + " °C</td></tr>";
    h += "<tr><th>Feuchte</th><td>" + String(gLive->humidity_rh, 1) + " %</td></tr>";
    h += "<tr><th>Druck</th><td>" + String(gLive->pressure_hpa, 1) + " hPa</td></tr>";
  }

  h += "</table></div>";
  return h;
}

String cardSpeicher() {
  String h;
  h += "<div class='card'><h2>Speicher</h2><table class='tbl'>";
  h += "<tr><th>Free Heap</th><td>" + String(ESP.getFreeHeap()) + " B</td></tr>";
  h += "<tr><th>Max Block</th><td>" + String(ESP.getMaxFreeBlockSize()) + " B</td></tr>";
  h += "</table></div>";
  return h;
}

String cardZeit() {
  String h;
  h += "<div class='card'><h2>Zeit</h2><table class='tbl'>";

  if (gCfg && ntpIsValid()) {
    h += "<tr><th>NTP gültig</th><td>Ja</td></tr>";
    h += "<tr><th>Lokal</th><td>" + ntpDateTimeString(*gCfg) + "</td></tr>";
  } else {
    h += "<tr><th>NTP gültig</th><td>Nein</td></tr>";
    h += "<tr><th>Lokal</th><td>—</td></tr>";
  }

  h += "</table></div>";
  return h;
}

String cardAktuelleEinstellungen() {
  String h;
  h += "<div class='card'><h2>Aktuelle Einstellungen</h2><table class='tbl'>";

  if (!gCfg) {
    h += "<tr><th>Status</th><td>cfg fehlt</td></tr>";
  } else {
    h += "<tr><th>Sensor-ID</th><td>" + gCfg->sensor_id + "</td></tr>";
    h += "<tr><th>Intervall</th><td>" + String(gCfg->send_interval_ms) + " ms</td></tr>";
    h += "<tr><th>Server IP</th><td>" + gCfg->server_udp_ip + "</td></tr>";
    h += "<tr><th>UDP Port</th><td>" + String(gCfg->server_udp_port) + "</td></tr>";
    h += "<tr><th>MQTT</th><td>" + String(gCfg->mqtt_enabled ? "An" : "Aus") + "</td></tr>";
  }

  h += "</table></div>";
  return h;
}
