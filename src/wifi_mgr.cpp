#include "wifi_mgr.h"
#include <WiFi.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <esp_system.h>
#include "device_id.h"
#include "html_util.h"
#include "version.h"

static Preferences prefs;
static Preferences apPrefs;
static WebServer* srv = nullptr;

// Captive Portal: im Setup-WLAN zeigen alle DNS-Namen auf den ESP,
// damit Handy/Laptop die Einrichtungsseite automatisch öffnen.
static DNSServer dnsServer;
static bool dnsRunning = false;

static void startCaptiveDns() {
  if (dnsRunning) dnsServer.stop();
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsRunning = dnsServer.start(53, "*", WiFi.softAPIP());
  Serial.printf("Captive-DNS %s\n", dnsRunning ? "aktiv" : "konnte nicht starten");
}

static void stopCaptiveDns() {
  if (!dnsRunning) return;
  dnsServer.stop();
  dnsRunning = false;
}

// ===== Config =====
static constexpr uint32_t BOOT_GRACE_MS     = 15000;  // nach Boot erstmal Ruhe geben
static constexpr uint32_t RETRY_INTERVAL_MS = 10000;  // Reconnect alle 10s
static constexpr uint8_t  MAX_FAILS         = 3;
static constexpr uint32_t PORTAL_RETRY_MS   = 30000;  // STA-Retry bei aktivem Portal nur alle 30s

// ===== State =====
static String storedSSID;
static String storedPass;

static bool   portalActive = false;
static String apSsid;
static String apPass;          // WPA2-Passwort für den Setup-AP (zufällig, im NVS gespeichert)

static uint32_t bootMs = 0;
static uint32_t lastTryMs = 0;
static uint8_t  fails = 0;
static uint32_t lastBeginMs = 0;
static uint32_t lastPortalRetryMs = 0;

// /wifi-Zeitfenster im Heimnetz (0 = geschlossen)
static uint32_t wifiUiUntilMs = 0;

// Deferred actions (aus HTTP-Handlern / Settings)
static bool reqForget = false;
static bool reqStartPortalKeepSta = false;
static bool reqConnectNew = false;

// ---------- Helpers ----------
static String makeApName(const String &prefix) {
  return prefix + "-" + deviceShortId();
}

static String generateApPass() {
  // ohne verwechselbare Zeichen (0/O, 1/l/I)
  static const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
  const size_t n = sizeof(alphabet) - 1;
  String s;
  for (int i = 0; i < 10; i++) s += alphabet[esp_random() % n];
  return s;
}

static bool uiWindowActive() {
  return wifiUiUntilMs != 0 && (int32_t)(wifiUiUntilMs - millis()) > 0;
}

static bool wifiUiAllowed() {
  return portalActive || (storedSSID.length() == 0) || uiWindowActive();
}

// Erstinbetriebnahme (noch keine WLAN-Daten): offener AP, damit man ohne
// serielle Konsole einrichten kann. Sobald WLAN-Daten existieren, ist der
// AP (z.B. Fallback nach Verbindungsverlust) immer WPA2-geschützt.
static const char* currentApPass() {
  return storedSSID.length() ? apPass.c_str() : nullptr;
}

static void startApOnlyPortal() {
  // AP-only = iPhone freundlicher, stabilere Beacons
  WiFi.setSleep(false);
  WiFi.disconnect(true, true);
  delay(100);

  WiFi.mode(WIFI_AP);
  delay(50);

  IPAddress ip(192,168,4,1), gw(192,168,4,1), sn(255,255,255,0);
  WiFi.softAPConfig(ip, gw, sn);

  const char* pw = currentApPass();
  bool ok = WiFi.softAP(apSsid.c_str(), pw, 1);
  delay(100);

  portalActive = ok;
  if (ok) startCaptiveDns();
  Serial.printf("softAP(AP-only,'%s',%s) ok=%d AP-IP=%s\n",
                apSsid.c_str(), pw ? "WPA2" : "offen", ok ? 1 : 0,
                WiFi.softAPIP().toString().c_str());
}

static void startApKeepStaPortal() {
  WiFi.setSleep(false);
  WiFi.mode(WIFI_AP_STA);
  delay(50);

  IPAddress ip(192,168,4,1), gw(192,168,4,1), sn(255,255,255,0);
  WiFi.softAPConfig(ip, gw, sn);

  const char* pw = currentApPass();
  bool ok = WiFi.softAP(apSsid.c_str(), pw, 1);
  delay(100);

  portalActive = ok;
  if (ok) startCaptiveDns();
  Serial.printf("softAP(AP+STA,'%s',%s) ok=%d AP-IP=%s\n",
                apSsid.c_str(), pw ? "WPA2" : "offen", ok ? 1 : 0,
                WiFi.softAPIP().toString().c_str());
}

static void stopPortal() {
  if (!portalActive) return;
  stopCaptiveDns();
  WiFi.softAPdisconnect(true);
  portalActive = false;
}

static void staBegin() {
  if (storedSSID.length() == 0) return;

  const wl_status_t st = WiFi.status();
  const uint32_t now = millis();

  if (st == WL_CONNECTED) return;

  // Auf ESP32 ist WL_IDLE_STATUS häufig "connecting" -> nicht anfassen
  if (st == WL_IDLE_STATUS) {
    Serial.println("STA: connecting... skip begin/reconnect");
    return;
  }

  WiFi.setSleep(false);

  // Mode nur setzen, wenn nötig (Mode-Wechsel kann STA resetten)
  wifi_mode_t want = portalActive ? WIFI_AP_STA : WIFI_STA;
  if (WiFi.getMode() != want) {
    WiFi.mode(want);
    delay(30);
  }

  const bool hardFail =
      (st == WL_NO_SSID_AVAIL) ||
      (st == WL_CONNECT_FAILED) ||
      (st == WL_CONNECTION_LOST) ||
      (st == WL_DISCONNECTED);

  if (lastBeginMs == 0 || hardFail || (now - lastBeginMs) > 30000) {
    lastBeginMs = now;
    WiFi.begin(storedSSID.c_str(), storedPass.c_str());
    Serial.println(String("STA begin -> ") + storedSSID + " (status=" + String((int)st) + ")");
    return;
  }

  WiFi.reconnect();
  Serial.println(String("STA reconnect -> ") + storedSSID + " (status=" + String((int)st) + ")");
}

// Neue Zugangsdaten anwenden: bestehende STA-Verbindung trennen und neu verbinden
static void connectWithNewCredentials() {
  wifi_mode_t m = WiFi.getMode();
  if (m == WIFI_MODE_AP) WiFi.mode(WIFI_AP_STA);
  else if (m == WIFI_MODE_NULL) WiFi.mode(WIFI_STA);
  delay(30);

  WiFi.disconnect(false, false);
  delay(100);
  lastBeginMs = millis();
  WiFi.begin(storedSSID.c_str(), storedPass.c_str());
  Serial.println("STA begin (neue Zugangsdaten) -> " + storedSSID);
}

static bool guardWifiApi() {
  if (wifiUiAllowed()) return true;
  srv->send(403, "text/plain", "Forbidden");
  return false;
}

// ---------- HTTP ----------
static void handleWifiPage() {
  if (!guardWifiApi()) return;

  String html;
  html.reserve(3200);

  html += "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<link rel='stylesheet' href='/style.css'>";
  html += "<link rel='stylesheet' href='/ui.css?v=" FW_VERSION "'>";
  html += "<link rel='icon' type='image/svg+xml' href='/favicon.svg'>";
  html += "<title>Multi Sensors - WLAN Setup</title>";
  html += "</head><body>";

  html += "<div class='topbar'><img src='/logo_name_weiss.svg' alt='Multi Sensors' class='logo'></div>";
  html += "<div class='menubar'>";
  html += "<a class='active' href='/wifi'>WLAN Setup</a>";
  html += "</div>";
  html += "<div class='content'>";

  html += "<div class='card'><h2>WLAN Setup</h2>";
  html += "<p class='small'>Wähle ein WLAN aus, gib das Passwort ein und speichere. Sobald verbunden, kannst du zur Startseite wechseln.</p>";
  html += "<div class='actions'>";
  html += "<button class='btn-primary' type='button' onclick='wifiScan()'>WLAN scannen</button>";
  html += "</div>";
  html += "<div id='msg' class='small mt-12'></div>";
  html += "<div id='list' class='mt-12'></div>";
  html += "</div>";

  html += "<div class='card'><h2>Status</h2><table class='tbl'>";
  html += "<tr><th>Setup-AP</th><td>" + htmlEscape(apSsid) + "</td></tr>";
  html += "<tr><th>Setup-IP</th><td>192.168.4.1</td></tr>";
  html += "<tr><th>Verbunden</th><td id='st_conn'>—</td></tr>";
  html += "<tr><th>SSID</th><td id='st_ssid'>—</td></tr>";
  html += "<tr><th>IP</th><td id='st_ip'>—</td></tr>";
  html += "</table>";

  html += "<div id='go' class='actions' style='display:none'>";
  html += "<a id='go_ip' class='btn btn-secondary' href='/'>Startseite (IP)</a>";
  html += "<a id='go_mdns' class='btn btn-secondary' href='/'>Startseite (mDNS)</a>";
  html += "</div>";

  html += "</div>"; // status card
  html += "</div>"; // content

  html += "<script src='/wifi.js'></script>";

  html += "</body></html>";
  srv->send(200, "text/html; charset=utf-8", html);
}

static void handleScan() {
  if (!guardWifiApi()) return;

  // Scannen braucht STA. Wenn Portal aktiv ist, lassen wir AP+STA dauerhaft.
  wifi_mode_t m = WiFi.getMode();
  if (m == WIFI_MODE_AP) {
    WiFi.mode(WIFI_AP_STA);
    delay(50);
  } else if (m == WIFI_MODE_NULL) {
    WiFi.mode(WIFI_STA);
    delay(50);
  }

  int n = WiFi.scanNetworks(false, true);

  struct Item { String ssid; int rssi; };
  Item best[25]; int mcnt = 0;

  for (int i = 0; i < n && mcnt < 25; i++) {
    String s = WiFi.SSID(i);
    int r = WiFi.RSSI(i);
    if (!s.length()) continue;

    int found = -1;
    for (int k = 0; k < mcnt; k++) if (best[k].ssid == s) { found = k; break; }
    if (found >= 0) { if (r > best[found].rssi) best[found].rssi = r; }
    else { best[mcnt++] = {s, r}; }
  }

  String json = "[";
  for (int i = 0; i < mcnt; i++) {
    if (i) json += ",";
    json += "{\"ssid\":\"" + jsonEscape(best[i].ssid) + "\",\"rssi\":" + String(best[i].rssi) + "}";
  }
  json += "]";
  srv->send(200, "application/json", json);

  WiFi.scanDelete();
}

static void handleSave() {
  if (!guardWifiApi()) return;

  String ssid = srv->arg("ssid");
  String pass = srv->arg("pass");
  if (!ssid.length() || ssid.length() > 32) { srv->send(400, "text/plain", "SSID fehlt oder ungültig"); return; }
  if (pass.length() > 0 && (pass.length() < 8 || pass.length() > 63)) {
    srv->send(400, "text/plain", "WLAN-Passwort muss 8–63 Zeichen haben (oder leer für offenes WLAN).");
    return;
  }

  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  storedSSID = ssid;
  storedPass = pass;

  fails = 0;
  lastTryMs = millis();

  // Erst antworten, dann (im Loop) neu verbinden – sonst geht die Antwort verloren,
  // wenn der Browser über die bestehende STA-Verbindung kommt.
  reqConnectNew = true;

  srv->send(200, "text/plain", "Gespeichert. Verbinde... (AP schließt bei Erfolg)");
}

static void handleStatus() {
  if (!guardWifiApi()) return;

  String json = "{";
  json += "\"portal\":" + String(portalActive ? "true" : "false");
  bool really = (WiFi.status() == WL_CONNECTED) && (WiFi.localIP() != IPAddress(0,0,0,0));
  json += ",\"connected\":" + String(really ? "true" : "false");
  json += ",\"ssid\":\"" + jsonEscape(WiFi.SSID()) + "\"";
  json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"host\":\"" + jsonEscape(String(WiFi.getHostname())) + "\"";
  json += "}";
  srv->send(200, "application/json", json);
}

static void handleNotFound() {
  // Geräte im Setup-WLAN prüfen beim Verbinden Adressen wie /generate_204 (Android),
  // /hotspot-detect.html (Apple) oder /connecttest.txt (Windows). Eine Umleitung mit
  // vollständiger Adresse lässt sie die Anmeldeseite automatisch öffnen.
  if (portalActive && srv->client().localIP() == WiFi.softAPIP()) {
    srv->sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/wifi", true);
    srv->send(302, "text/plain", "Redirect");
    return;
  }
  srv->send(404, "text/plain", String("Not found: ") + srv->uri());
}

// ---------- Public API ----------
void wifiMgrBegin(WebServer &server, const String &apNamePrefix) {
  srv = &server;
  prefs.begin("wifi", false);

  storedSSID = prefs.getString("ssid", "");
  storedPass = prefs.getString("pass", "");

  // AP-Passwort in eigenem Namespace (bleibt bei "WLAN vergessen" erhalten)
  apPrefs.begin("wifiap", false);
  apPass = apPrefs.getString("pass", "");
  if (apPass.length() < 8) {
    apPass = generateApPass();
    apPrefs.putString("pass", apPass);
  }

  apSsid = makeApName(apNamePrefix);
  Serial.println("Setup-AP: " + apSsid + "  Passwort (wenn WLAN-Daten vorhanden): " + apPass);

  srv->on("/wifi", handleWifiPage);
  srv->on("/api/wifi/scan", HTTP_GET, handleScan);
  srv->on("/api/wifi/save", HTTP_POST, handleSave);
  srv->on("/api/wifi/status", handleStatus);
  srv->onNotFound(handleNotFound);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      Serial.printf("WiFi STA DISCONNECTED, reason=%d\n", info.wifi_sta_disconnected.reason);
    }
  });

  bootMs = millis();
  lastTryMs = 0;
  fails = 0;

  if (storedSSID.length() > 0) {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.begin(storedSSID.c_str(), storedPass.c_str());
    Serial.println("Boot STA begin -> " + storedSSID);
  } else {
    startApOnlyPortal();
  }
}

void wifiMgrLoop() {
  if (dnsRunning) dnsServer.processNextRequest();

  // Deferred actions
  if (reqForget) {
    reqForget = false;
    prefs.clear();
    storedSSID = "";
    storedPass = "";
    fails = 0;
    lastTryMs = 0;
    stopPortal();
    startApOnlyPortal();
    return;
  }

  if (reqStartPortalKeepSta) {
    reqStartPortalKeepSta = false;
    startApKeepStaPortal();
    return;
  }

  if (reqConnectNew) {
    reqConnectNew = false;
    connectWithNewCredentials();
    return;
  }

  // Connected -> Portal aus (außer der Nutzer hat das /wifi-Fenster bewusst geöffnet)
  if (WiFi.status() == WL_CONNECTED) {
    fails = 0;
    lastBeginMs = 0;
    lastPortalRetryMs = 0;
    if (portalActive && !uiWindowActive()) stopPortal();
    return;
  }

  // Keine Creds -> Portal
  if (storedSSID.length() == 0) {
    if (!portalActive) startApOnlyPortal();
    return;
  }

  // Boot-Grace (damit der erste Versuch nicht kaputt getaktet wird)
  if (!portalActive && (millis() - bootMs) < BOOT_GRACE_MS) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastTryMs < RETRY_INTERVAL_MS) return;
  lastTryMs = now;

  if (!portalActive) {
    wl_status_t st = WiFi.status();

    bool hardFail =
      (st == WL_NO_SSID_AVAIL) ||
      (st == WL_CONNECT_FAILED) ||
      (st == WL_CONNECTION_LOST) ||
      (st == WL_DISCONNECTED);

    if (hardFail) {
      fails++;
      Serial.println("WLAN getrennt, reconnect Versuch " + String(fails) + " (status=" + String((int)st) + ")");
    } else {
      Serial.println("WLAN reconnect: warte (status=" + String((int)st) + ")");
    }

    WiFi.reconnect();

    if (fails >= MAX_FAILS) {
      Serial.println("Reconnect mehrfach fehlgeschlagen -> Setup AP aktivieren (AP+STA, WPA2)");
      startApKeepStaPortal();
    }
    return;
  }

  // Portal aktiv: nur selten STA retry (sonst killt man den Connect-Prozess)
  const wl_status_t st = WiFi.status();
  if (st == WL_IDLE_STATUS) return;

  const uint32_t now2 = millis();
  if (now2 - lastPortalRetryMs < PORTAL_RETRY_MS) return;
  lastPortalRetryMs = now2;

  if (st == WL_DISCONNECTED || st == WL_CONNECT_FAILED || st == WL_CONNECTION_LOST || st == WL_NO_SSID_AVAIL) {
    Serial.println("Portal aktiv -> STA retry zu " + storedSSID + " (status=" + String((int)st) + ")");
    staBegin();
  }
}

bool wifiMgrIsConnected() { return WiFi.status() == WL_CONNECTED; }
bool wifiMgrPortalActive() { return portalActive; }
bool wifiMgrHasCredentials() { return storedSSID.length() > 0; }
String wifiMgrApSsid() { return apSsid; }
String wifiMgrApPass() { return apPass; }

void wifiMgrRequestWifiUi(uint32_t seconds) {
  wifiUiUntilMs = millis() + seconds * 1000UL;
  if (wifiUiUntilMs == 0) wifiUiUntilMs = 1;
}
void wifiMgrRequestStartPortalKeepSta() {
  reqStartPortalKeepSta = true;
}
void wifiMgrRequestForget() {
  reqForget = true;
}
