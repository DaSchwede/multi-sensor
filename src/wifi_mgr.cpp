#include "wifi_mgr.h"
#include <WiFi.h>
#include <Preferences.h>

static Preferences prefs;
static WebServer* srv = nullptr;

// ===== Config =====
static constexpr uint32_t BOOT_GRACE_MS     = 15000;  // nach Boot erstmal Ruhe geben
static constexpr uint32_t RETRY_INTERVAL_MS = 10000;  // Reconnect alle 10s
static constexpr uint8_t  MAX_FAILS         = 3;      // <- deine Vorgabe

// ===== State =====
static String storedSSID;
static String storedPass;

static bool   portalActive = false;
static String apSsid;

static uint32_t bootMs = 0;
static uint32_t lastTryMs = 0;
static uint8_t  fails = 0;
static uint32_t lastBeginMs = 0;
static uint32_t lastPortalRetryMs = 0;
static constexpr uint32_t PORTAL_RETRY_MS = 30000;  // ✅ nur alle 30s

// UI-Window (closed by default)
static uint32_t wifiUiUntilMs = 0;

// Deferred actions (aus HTTP-Handlern / Settings)
static bool reqForget = false;
static bool reqStartPortalKeepSta = false;

// ---------- Helpers ----------
static String htmlEscape(const String &s) {
  String o; o.reserve(s.length() + 8);
  for (size_t i=0;i<s.length();i++){
    char c=s[i];
    switch(c){
      case '&': o+=F("&amp;"); break;
      case '<': o+=F("&lt;"); break;
      case '>': o+=F("&gt;"); break;
      case '"': o+=F("&quot;"); break;
      case '\'': o+=F("&#39;"); break;
      default: o+=c; break;
    }
  }
  return o;
}

static String makeApName(const String &prefix) {
  uint64_t mac = ESP.getEfuseMac();
  uint32_t low = (uint32_t)(mac & 0xFFFFFFFF);
  String chip = String(low, HEX);
  chip.toUpperCase();
  while (chip.length() < 8) chip = "0" + chip;
  String shortId = chip.substring(chip.length() - 4);
  return prefix + "-" + shortId;
}

static bool wifiUiAllowed() {
  return portalActive || (storedSSID.length() == 0) || (millis() < wifiUiUntilMs);
}

static void startApOnlyPortal() {
  // AP-only = iPhone freundlicher, stabilere Beacons
  apSsid = apSsid.length() ? apSsid : makeApName("Multi-Sensor");

  WiFi.setSleep(false);
  WiFi.disconnect(true, true);
  delay(100);

  WiFi.mode(WIFI_AP);
  delay(50);

  IPAddress ip(192,168,4,1), gw(192,168,4,1), sn(255,255,255,0);
  WiFi.softAPConfig(ip, gw, sn);

  bool ok = WiFi.softAP(apSsid.c_str(), nullptr, 1); // offen, Kanal 1 (iPhone-safe)
  delay(100);

  portalActive = ok;
  Serial.printf("softAP(AP-only,'%s') ok=%d AP-IP=%s mode=%d\n",
                apSsid.c_str(), ok?1:0, WiFi.softAPIP().toString().c_str(), (int)WiFi.getMode());
}

static void startApKeepStaPortal() {
  apSsid = apSsid.length() ? apSsid : makeApName("Multi-Sensor");

  WiFi.setSleep(false);
  WiFi.mode(WIFI_AP_STA);
  delay(50);

  IPAddress ip(192,168,4,1), gw(192,168,4,1), sn(255,255,255,0);
  WiFi.softAPConfig(ip, gw, sn);

  bool ok = WiFi.softAP(apSsid.c_str(), nullptr, 1);
  delay(100);

  portalActive = ok;
  Serial.printf("softAP(AP+STA,'%s') ok=%d AP-IP=%s mode=%d\n",
                apSsid.c_str(), ok?1:0, WiFi.softAPIP().toString().c_str(), (int)WiFi.getMode());
}

static void stopPortal() {
  if (!portalActive) return;
  WiFi.softAPdisconnect(true);
  portalActive = false;
}

static void staBegin() {
  if (storedSSID.length() == 0) return;

  const wl_status_t st = WiFi.status();
  const uint32_t now = millis();

  // ✅ Wenn verbunden: fertig
  if (st == WL_CONNECTED) return;

  // ✅ Wenn gerade am Verbinden (CONNECTING): NICHT anfassen!
  // Auf ESP32 ist WL_IDLE_STATUS häufig "connecting"
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

  // "harte" Fehler
  const bool hardFail =
      (st == WL_NO_SSID_AVAIL) ||
      (st == WL_CONNECT_FAILED) ||
      (st == WL_CONNECTION_LOST) ||
      (st == WL_DISCONNECTED);

  // ✅ begin maximal alle 30s (oder bei hardFail)
  if (lastBeginMs == 0 || hardFail || (now - lastBeginMs) > 30000) {
    lastBeginMs = now;
    WiFi.begin(storedSSID.c_str(), storedPass.c_str());
    Serial.println(String("STA begin -> ") + storedSSID + " (status=" + String((int)st) + ")");
    return;
  }

  // ✅ reconnect maximal alle 10s (aber nicht wenn connecting)
  WiFi.reconnect();
  Serial.println(String("STA reconnect -> ") + storedSSID + " (status=" + String((int)st) + ")");
}

// ---------- HTTP ----------
static void handleWifiPage() {
  if (!wifiUiAllowed()) {
    srv->send(403, "text/plain", "Forbidden");
    return;
  }

  String html;
  html.reserve(3200);

  html += "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<link rel='stylesheet' href='/style.css'>";
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

  // externer JS (klein & portal-spezifisch)
  html += "<script src='/wifi.js'></script>";

  html += "</body></html>";
  srv->send(200, "text/html; charset=utf-8", html);
}

static void handleScan() {
  // Scannen braucht STA. Wenn Portal aktiv ist, lassen wir AP+STA dauerhaft.
  wifi_mode_t m = WiFi.getMode();

  if (m == WIFI_MODE_AP) {
    // Statt nachher zurück auf AP zu flippen -> AP_STA beibehalten
    WiFi.mode(WIFI_AP_STA);
    delay(50);
  } else if (m == WIFI_MODE_NULL) {
    WiFi.mode(WIFI_STA);
    delay(50);
  }

  int n = WiFi.scanNetworks(false, true);

  struct Item { String ssid; int rssi; };
  Item best[25]; int mcnt = 0;

  for (int i=0;i<n && mcnt<25;i++){
    String s = WiFi.SSID(i);
    int r = WiFi.RSSI(i);
    if (!s.length()) continue;

    int found=-1;
    for(int k=0;k<mcnt;k++) if(best[k].ssid==s){found=k;break;}
    if(found>=0){ if(r>best[found].rssi) best[found].rssi=r; }
    else { best[mcnt++] = {s,r}; }
  }

  String json="[";
  for(int i=0;i<mcnt;i++){
    if(i) json+=",";
    json += "{\"ssid\":\""+htmlEscape(best[i].ssid)+"\",\"rssi\":"+String(best[i].rssi)+"}";
  }
  json+="]";
  srv->send(200,"application/json",json);

  WiFi.scanDelete();
}

static void handleSave() {
  String ssid = srv->arg("ssid");
  String pass = srv->arg("pass");
  if (!ssid.length()) { srv->send(400,"text/plain","SSID fehlt"); return; }

  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  storedSSID = ssid;
  storedPass = pass;

  fails = 0;
  lastTryMs = millis();

  // sofort versuchen
  staBegin();

  srv->send(200,"text/plain","Gespeichert. Verbinde... (AP schließt bei Erfolg)");
}

static void handleStatus() {
  String json = "{";
  json += "\"portal\":" + String(portalActive ? "true" : "false");
  bool really = (WiFi.status() == WL_CONNECTED) && (WiFi.localIP() != IPAddress(0,0,0,0));
  json += ",\"connected\":" + String(really ? "true" : "false");
  json += ",\"ssid\":\"" + htmlEscape(WiFi.SSID()) + "\"";
  json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"host\":\"" + htmlEscape(String(WiFi.getHostname())) + "\"";
  json += "}";
  srv->send(200, "application/json", json);
}

static void handleNotFound() {
  if (portalActive) {
    srv->sendHeader("Location","/wifi",true);
    srv->send(302,"text/plain","Redirect");
    return;
  }
  srv->send(404,"text/plain", String("Not found: ") + srv->uri());
}

// ---------- Public API ----------
void wifiMgrBegin(WebServer &server, const String &apNamePrefix) {
  srv = &server;
  prefs.begin("wifi", false);

  storedSSID = prefs.getString("ssid", "");
  storedPass = prefs.getString("pass", "");

  apSsid = makeApName(apNamePrefix);

  srv->on("/wifi", handleWifiPage);
  srv->on("/api/wifi/scan", HTTP_GET, handleScan);
  srv->on("/api/wifi/save", HTTP_POST, handleSave);
  srv->on("/api/wifi/status", handleStatus);
  srv->onNotFound(handleNotFound);
  
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
  if(event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED){
    Serial.printf("WiFi STA DISCONNECTED, reason=%d\n", info.wifi_sta_disconnected.reason);
  }
  });


  bootMs = millis();
  lastTryMs = 0;
  fails = 0;

  if (storedSSID.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
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
    // /wifi window soll i.d.R. schon vorher gesetzt sein (Settings)
    startApKeepStaPortal();
    return;
  }

  // Connected -> Portal aus
  if (WiFi.status() == WL_CONNECTED) {
    fails = 0;
    lastBeginMs = 0;
    lastPortalRetryMs = 0;
  if (portalActive) stopPortal();
  return;
  }


  // Kein Creds -> Portal
  if (storedSSID.length() == 0) {
    if (!portalActive) startApOnlyPortal();
    return;
  }

  // Boot-Grace (damit der erste Versuch nicht kaputt getaktet wird)
  if (!portalActive && (millis() - bootMs) < BOOT_GRACE_MS) {
    return;
  }

  // Reconnect-Takt
  const uint32_t now = millis();
  if (now - lastTryMs < RETRY_INTERVAL_MS) return;
  lastTryMs = now;

  // Wenn Portal nicht aktiv -> zählen wir Fehlschläge und öffnen nach 3 den AP
    if (!portalActive) {
    // Status-basiertes Fail-Counting (nicht einfach pro Timer-Tick hochzählen)
    wl_status_t st = WiFi.status();

    // Nur "harte" Fehler zählen
    bool hardFail =
      (st == WL_NO_SSID_AVAIL) ||
      (st == WL_CONNECT_FAILED) ||
      (st == WL_CONNECTION_LOST) ||
      (st == WL_DISCONNECTED);

    if (hardFail) {
      fails++;
      Serial.println("WLAN getrennt, reconnect Versuch " + String(fails) + " (status=" + String((int)st) + ")");
    } else {
      // z.B. WL_IDLE_STATUS -> nicht zählen, erstmal warten
      Serial.println("WLAN reconnect: warte (status=" + String((int)st) + ")");
    }

    WiFi.reconnect();

    if (fails >= MAX_FAILS) {
      Serial.println("Reconnect mehrfach fehlgeschlagen -> Setup AP aktivieren (AP+STA)");
      startApKeepStaPortal();   // <- wichtig: nicht STA hart abwürgen
    }
    return;
  }

  // Portal aktiv: trotzdem versuchen, wieder ins WLAN zu kommen (ohne Spam)
    // Portal aktiv: nur selten STA retry (sonst killt man den Connect-Prozess)
  const wl_status_t st = WiFi.status();

  // wenn connecting -> einfach warten
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

void wifiMgrRequestWifiUi(uint32_t seconds) {
  wifiUiUntilMs = millis() + seconds * 1000UL;
}
void wifiMgrRequestStartPortalKeepSta() {
  reqStartPortalKeepSta = true;
}
void wifiMgrRequestForget() {
  reqForget = true;
}
