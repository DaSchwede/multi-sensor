#include "pages.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <vector>

#include "auth.h"
#include "version.h"
#include "ntp_time.h"
#include <esp_heap_caps.h>
#include "device_id.h"
#include "html_util.h"
#include "mqtt_client.h"
#include "changelog.h"


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

static bool pathStartsWith(const String& path, const String& prefix) {
  return path.length() >= prefix.length() && path.startsWith(prefix);
}

static String navDrop(const String& label,
                      const String& basePath,
                      const String& currentPath,
                      const std::vector<std::pair<String,String>>& items) {
  // aktiv, wenn wir "in" diesem Bereich sind
  String activeCls = pathStartsWith(currentPath, basePath) ? "active" : "";

  String h;
  h += "<div class='navdrop " + activeCls + "'>";
  // Menü öffnet per Klick (Touch) und per Maus-Hover; basePath bleibt als Fallback ohne JavaScript
  h +=   "<a class='navdrop-btn' href='" + basePath + "' aria-haspopup='true' aria-expanded='false'>" + label + " <span class='caret'>▾</span></a>";
  h +=   "<div class='navdrop-menu'>";

  for (const auto& it : items) {
    const String& href  = it.first;
    const String& title = it.second;
    String cls = (href == currentPath) ? "active" : "";
    h += "<a class='" + cls + "' href='" + href + "'>" + title + "</a>";
  }

  h +=   "</div>";
  h += "</div>";
  return h;
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
  // Dropdown-Items (Einstellungen)
std::vector<std::pair<String,String>> settingsItems = {
  { "/settings/sensors", "Sensoren" },
  { "/settings/udp",   "UDP" },
  { "/settings/time",  "Zeit / NTP" },
  { "/settings/mqtt",  "MQTT" },
  { "/settings/logger", "Logger" },
  { "/settings/ui",    "Darstellung" },
  { "/settings/wifi", "WLAN" },
  { "/settings/tools", "Tools" },
};
  // Dropdown-Items (Info)
std::vector<std::pair<String,String>> infoItems = {
    { "/info",        "Übersicht" },
    { "/info/system", "Systeminfo" },
    { "/logger", "Logger" },
    { "/info/time",   "Zeit / NTP" },
        // später erweiterbar:
    // { "/info/network", "Netzwerk" },
    // { "/info/sensor",  "Sensor" },
  };


  return String(
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<link rel='stylesheet' href='/style.css'>"
    "<link rel='stylesheet' href='/ui.css?v=" FW_VERSION "'>"
    "<script defer src='/script.js'></script>"
    "<script>"
    "document.addEventListener('DOMContentLoaded',function(){"
    "var drops=[].slice.call(document.querySelectorAll('.navdrop'));"
    "function closeAll(ex){drops.forEach(function(d){if(d!==ex){d.classList.remove('open');"
    "var b=d.querySelector('.navdrop-btn');if(b)b.setAttribute('aria-expanded','false');}});}"
    "drops.forEach(function(d){var b=d.querySelector('.navdrop-btn');if(!b)return;"
    "b.addEventListener('click',function(e){e.preventDefault();e.stopImmediatePropagation();"
    "var o=!d.classList.contains('open');closeAll(d);d.classList.toggle('open',o);"
    "b.setAttribute('aria-expanded',o?'true':'false');},true);});"
    "document.addEventListener('click',function(){closeAll(null);});"
    "});"
    "</script>"
    "<link rel='icon' type='image/svg+xml' href='/favicon.svg'>"
    "<title>") + String(FW_NAME) + " - " + title + "</title>"
    "</head><body>"
    "<div class='topbar'><img src='/logo_name_weiss.svg' alt='Multi Sensors' class='logo'></div>"
    "<div class='menubar has-toggle'>"
      "<button type='button' class='nav-toggle' aria-label='Menü öffnen' "
      "onclick=\"this.parentNode.classList.toggle('open')\">&#9776; Menü</button>"
      + navLink("/", "Startseite", currentPath)
      + navDrop("Info", "/info", currentPath, infoItems)
      + navDrop("Einstellungen", "/settings/udp", currentPath, settingsItems)
      + navLink("/about", "Über", currentPath)
      + "<span class='right'><a href='/logout'>Abmelden</a></span>"
    "</div>"
    "<div class='content'>";
}

static String headerHtmlPublic(WebServer &server, const String &title, const String &currentPath) {
  bool authed = isAuthenticated(server);

  std::vector<std::pair<String,String>> infoItems = {
  { "/info",           "Übersicht" },
  { "/info/system",    "Systeminfo" },
  { "/logger",         "Logger" },
  { "/info/time",      "Zeit / NTP" },
  //{ "/info/network",   "Netzwerk" },
  //{ "/info/sensor",    "Sensor" },
  //{ "/info/memory",    "Speicher" },
  //{ "/info/settings",  "Einstellungen" },
};

  std::vector<std::pair<String,String>> settingsItems = {
  { "/settings/sensors", "Sensoren" },
  { "/settings/udp",   "UDP" },
  { "/settings/time",  "Zeit / NTP" },
  { "/settings/mqtt",  "MQTT" },
  { "/settings/logger", "Logger" },
  { "/settings/ui",    "Darstellung" },
  { "/settings/wifi", "WLAN" },
  { "/settings/tools", "Tools" },
};

  String menu;
  if (authed) {
    menu =
      navLink("/", "Startseite", currentPath) +
      navDrop("Info", "/info", currentPath, infoItems) +
      navDrop("Einstellungen", "/settings/udp", currentPath, settingsItems) +
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
    "<link rel='stylesheet' href='/ui.css?v=" FW_VERSION "'>"
    "<script defer src='/script.js'></script>"
    "<script>"
    "document.addEventListener('DOMContentLoaded',function(){"
    "var drops=[].slice.call(document.querySelectorAll('.navdrop'));"
    "function closeAll(ex){drops.forEach(function(d){if(d!==ex){d.classList.remove('open');"
    "var b=d.querySelector('.navdrop-btn');if(b)b.setAttribute('aria-expanded','false');}});}"
    "drops.forEach(function(d){var b=d.querySelector('.navdrop-btn');if(!b)return;"
    "b.addEventListener('click',function(e){e.preventDefault();e.stopImmediatePropagation();"
    "var o=!d.classList.contains('open');closeAll(d);d.classList.toggle('open',o);"
    "b.setAttribute('aria-expanded',o?'true':'false');},true);});"
    "document.addEventListener('click',function(){closeAll(null);});"
    "});"
    "</script>"
    "<link rel='icon' type='image/svg+xml' href='/favicon.svg'>"
    "<title>") + String(FW_NAME) + " - " + title + "</title>"
    "</head><body>"
    "<div class='topbar'>" + String(FW_NAME) + "</div>"
    "<div class='menubar has-toggle'>"
      "<button type='button' class='nav-toggle' aria-label='Menü öffnen' "
      "onclick=\"this.parentNode.classList.toggle('open')\">&#9776; Menü</button>" + menu + "</div>"
    "<div class='content'>";
}

static String footerHtml() {
  return "</div></body></html>";
}

// Exported wrappers
String pagesHeaderAuth(const String &title, const String &currentPath) { return headerHtmlAuth(title, currentPath); }
String pagesHeaderPublic(WebServer &server, const String &title, const String &currentPath) { return headerHtmlPublic(server, title, currentPath); }
String pagesFooter() { return footerHtml(); }

String pagesTitle(const String &title, const String &subtitle, const String &rightHtml) {
  String h = "<div class='page-head'><div>";
  h += "<h1 class='page-title'>" + title + "</h1>";
  if (subtitle.length()) h += "<p class='page-sub'>" + subtitle + "</p>";
  h += "</div>";
  if (rightHtml.length()) h += "<div class='page-head-right'>" + rightHtml + "</div>";
  h += "</div>";
  return h;
}

// ============================================================================
// Card builders (werden von pageInfo/pageRoot genutzt)
// ============================================================================
String cardSystem() {
  String h;
  h += "<div class='card'><h2>System</h2><table class='tbl'>";
  h += "<tr><th>Firmware</th><td>" + String(FW_NAME) + "</td></tr>";
  h += "<tr><th>Version</th><td>" + String(FW_VERSION) + "</td></tr>";
  h += "<tr><th>Build</th><td>" + String(FW_DATE) + "</td></tr>";
  h += "<tr><th>MAC</th><td>" + deviceMacHex() + "</td></tr>";
  h += "<tr><th>CPU</th><td>" + String(ESP.getCpuFreqMHz()) + " MHz</td></tr>";
  h += "<tr><th>Uptime</th><td>" + uptimeString() + "</td></tr>";
  h += "</table></div>";
  return h;
}

String cardNetzwerk() {
  String h;
  h += "<div class='card'><h2>Netzwerk</h2><table class='tbl'>";
  h += "<tr><th>Status</th><td>" + String(WiFi.status() == WL_CONNECTED ? "Verbunden" : "Getrennt") + "</td></tr>";
  h += "<tr><th>SSID</th><td>" + htmlEscape(WiFi.SSID()) + "</td></tr>";
  h += "<tr><th>IP</th><td>" + WiFi.localIP().toString() + "</td></tr>";
  h += "<tr><th>Gateway</th><td>" + WiFi.gatewayIP().toString() + "</td></tr>";
  h += "<tr><th>RSSI</th><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
  h += "<tr><th>mDNS</th><td>http://" + String(WiFi.getHostname()) + ".local/</td></tr>";
  h += "</table></div>";
  return h;
}

String cardSensor() {
  String h;
  h += "<div class='card'><h2>Sensor</h2><table class='tbl'>";

  if (!gLive) {
    h += "<tr><th>Status</th><td>Keine Daten</td></tr>";
  } else {
    auto fmt1 = [](float v, const char* unit)->String {
      if (isnan(v)) return String("—");
      return String(v, 1) + unit;
    };
    auto fmt0 = [](float v, const char* unit)->String {
      if (isnan(v)) return String("—");
      return String((int)lroundf(v)) + unit;
    };

    h += "<tr><th>Temperatur</th><td>" + fmt1(gLive->temperature_c, " °C")  + "</td></tr>";
    h += "<tr><th>Feuchte</th><td>"    + fmt1(gLive->humidity_rh,   " %")   + "</td></tr>";
    h += "<tr><th>Druck</th><td>"      + fmt1(gLive->pressure_hpa,  " hPa") + "</td></tr>";
    h += "<tr><th>CO₂</th><td>"        + fmt0(gLive->co2_ppm,       " ppm") + "</td></tr>";
  }

  /* --- Rescan Button --- */
  h += "<tr><td colspan='2' style='padding-top:12px;'>"
       "<form method='POST' action='/action/rescan_sensors' "
       "onsubmit='return confirm(\"Sensoren neu erkennen?\");'>"
       "<button class='btn-primary' type='submit'>"
       "Sensoren neu erkennen"
       "</button>"
       "</form>"
       "</td></tr>";

  h += "</table></div>";
  return h;
}

String cardSpeicher() {
  String h;
  h += "<div class='card'><h2>Speicher</h2><table class='tbl'>";
  h += "<tr><th>Free Heap</th><td>" + String(ESP.getFreeHeap()) + " B</td></tr>";
  h += "<tr><th>Max Block</th><td>" + String(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)) + " B</td></tr>";
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

String cardUdpEinstellungen() {
  String h;
  h += "<div class='card'><h2>UDP Einstellungen</h2><table class='tbl'>";

  if (!gCfg) {
    h += "<tr><th>Status</th><td>cfg fehlt</td></tr>";
  } else {
    h += "<tr><th>Sensor-ID</th><td>" + htmlEscape(gCfg->sensor_id) + "</td></tr>";
    h += "<tr><th>Intervall</th><td>" + String(gCfg->send_interval_ms) + " ms</td></tr>";
    h += "<tr><th>Server IP</th><td>" + htmlEscape(gCfg->server_udp_ip) + "</td></tr>";
    h += "<tr><th>UDP Port</th><td>" + String(gCfg->server_udp_port) + "</td></tr>";
    h += "<tr><th>Format</th><td>" + String(gCfg->udp_format == 1 ? "JSON" : "Text (CSV)") + "</td></tr>";
    h += "<tr><th>Aktiv</th><td>" + String(gCfg->udp_enabled ? "Ja" : "Nein") + "</td></tr>";
  }

  h += "</table></div>";
  return h;
}

String cardMqtt() {
  String h;
  h += "<div class='card'><h2>MQTT</h2><table class='tbl'>";
  if (!gCfg) {
    h += "<tr><th>Status</th><td>cfg fehlt</td></tr>";
  } else if (!gCfg->mqtt_enabled) {
    h += "<tr><th>Status</th><td>ausgeschaltet</td></tr>";
  } else {
    const bool conn = mqttIsConnected();
    h += "<tr><th>Status</th><td><span class='badge " + String(conn ? "badge-ok" : "badge-bad") + "'>" +
         String(mqttStateStr()) + "</span></td></tr>";
    h += "<tr><th>Broker</th><td>" + (gCfg->mqtt_host.length() ? htmlEscape(gCfg->mqtt_host) : String("—")) +
         ":" + String(gCfg->mqtt_port) + (gCfg->mqtt_tls_enabled ? " (TLS)" : "") + "</td></tr>";
    h += "<tr><th>Topic</th><td><code>" + htmlEscape(mqttTopicFor(*gCfg, "…")) + "</code></td></tr>";
    h += "<tr><th>Home Assistant</th><td>" + String(gCfg->mqtt_ha_discovery ? "Discovery an" : "aus") + "</td></tr>";
    h += "<tr><th>Last Will</th><td>" + String(gCfg->mqtt_lwt_enabled ? "an" : "aus") + "</td></tr>";
  }
  h += "</table></div>";
  return h;
}

// Changelog aus include/changelog.h als Liste
String cardChangelog() {
  const String txt = String(FPSTR(CHANGELOG_TXT));
  String h = "<div class='card'><h2>Änderungen</h2><div class='changelog'>";
  bool inList = false;
  int start = 0;
  while (start <= (int)txt.length()) {
    int nl = txt.indexOf('\n', start);
    if (nl < 0) nl = txt.length();
    String line = txt.substring(start, nl);
    start = nl + 1;
    line.trim();
    if (!line.length()) {
      if (inList) { h += "</ul>"; inList = false; }
      if (nl >= (int)txt.length()) break;
      continue;
    }
    if (line.startsWith("-")) {
      if (!inList) { h += "<ul>"; inList = true; }
      line.remove(0, 1);
      line.trim();
      h += "<li>" + htmlEscape(line) + "</li>";
    } else {
      if (inList) { h += "</ul>"; inList = false; }
      h += "<h3>" + htmlEscape(line) + "</h3>";
    }
  }
  if (inList) h += "</ul>";
  h += "</div></div>";
  return h;
}
