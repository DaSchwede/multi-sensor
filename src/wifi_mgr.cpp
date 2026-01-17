#include "wifi_mgr.h"
#include <WiFi.h>
#include <Preferences.h>

static bool reqStartPortal = false;
static bool reqForget = false;

static uint32_t wifiUiUntilMs = 0;  // Zeitfenster, in dem /wifi erlaubt ist
static bool reqStartPortalKeepSta = false;

static Preferences prefs;
static WebServer *srv = nullptr;

static bool portalActive = false;
static String apSsid;

static String storedSSID;
static String storedPass;

// Reconnect-Logik
static uint32_t lastReconnectAttemptMs = 0;
static uint8_t reconnectFails = 0;

// ---------- Helpers ----------
static String htmlEscape(const String &s) {
  String o;
  o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&': o += F("&amp;"); break;
      case '<': o += F("&lt;"); break;
      case '>': o += F("&gt;"); break;
      case '"': o += F("&quot;"); break;
      case '\'': o += F("&#39;"); break;
      default: o += c; break;
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

static void startPortal();
static void stopPortal();
static void tryStaConnect();

// ---------- HTTP Handlers ----------
static void handleRoot() {
  // Im Portal-Modus immer auf /wifi umleiten
  if (portalActive) {
    srv->sendHeader("Location", "/wifi", true);
    srv->send(302, "text/plain", "Redirect");
    return;
  }
  srv->send(200, "text/plain", "OK");
}

static void handleWifiPage() {
  // im Normalbetrieb sperren
  if (!wifiUiAllowed()) {
  srv->send(403, "text/plain", "Forbidden");
  return;
  }


  String html;
  html.reserve(4500);

  html += "<!doctype html><html><head><meta charset='utf-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<link rel='stylesheet' href='/style.css'>";
  html += "<link rel='icon' type='image/svg+xml' href='/favicon.svg'>";
  html += "<title>Multi Sensors - WLAN Setup</title>";
  html += "</head><body>";

  // Topbar + Logo (wie bei dir)
  html += "<div class='topbar'><img src='/logo_name_weiss.svg' alt='Multi Sensors' class='logo'></div>";

  // Minimal-Menü (kein Login nötig im Portal)
  html += "<div class='menubar'>";
  html += "<a class='active' href='/wifi'>WLAN Setup</a>";
  html += "</div>";

  html += "<div class='content'>";

  // Card: Setup
  html += "<div class='card'><h2>WLAN Setup</h2>";
  html += "<p class='small'>Wähle ein WLAN aus und speichere das Passwort. Der Setup-AP schließt nach erfolgreicher Verbindung automatisch.</p>";
  html += "<div class='actions'>";
  html += "<button class='btn-primary' type='button' onclick='scan()'>WLAN scannen</button>";
  html += "</div>";
  html += "<div id='msg' class='small mt-12'></div>";
  html += "<div id='list' class='mt-12'></div>";
  html += "</div>";

  // Card: Status (mit deiner .tbl)
  html += "<div class='card'><h2>Status</h2><table class='tbl'>";
  html += "<tr><th>Setup-AP</th><td>" + htmlEscape(apSsid) + "</td></tr>";
  html += "<tr><th>Setup-IP</th><td>192.168.4.1</td></tr>";
  html += "</table></div>";

  html += "</div>"; // content

  // JS: Netzwerke als Tabelle in .tbl
  html += R"JS(
<script>
async function scan(){
  const msg  = document.getElementById('msg');
  const list = document.getElementById('list');
  msg.textContent = 'Scanne...';
  list.innerHTML = '';

  const r = await fetch('/api/wifi/scan');
  const j = await r.json();

  msg.textContent = 'Gefunden: ' + j.length;

  if(!j.length){
    list.innerHTML = '<div class="card"><p class="small">Keine Netzwerke gefunden.</p></div>';
    return;
  }

  let h = '<div class="card"><h2>Netzwerke</h2><table class="tbl">';
  h += '<tr><th>SSID</th><th>RSSI</th><th>Aktion</th></tr>';

  j.forEach(n=>{
    const ssidEsc = (n.ssid || '').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
    const ssidJs  = (n.ssid || '').replace(/'/g,"\\'");
    h += `<tr>
      <td><b>${ssidEsc}</b></td>
      <td>${n.rssi} dBm</td>
      <td><button class="btn-primary" type="button" onclick="connectTo('${ssidJs}')">Verbinden</button></td>
    </tr>`;
  });

  h += '</table></div>';
  list.innerHTML = h;
}

async function connectTo(ssid){
  const msg = document.getElementById('msg');
  let pass = prompt('Passwort für ' + ssid + ' (leer lassen für offen)');
  if(pass === null) return;

  const body = new URLSearchParams();
  body.set('ssid', ssid);
  body.set('pass', pass);

  msg.textContent = 'Speichere & verbinde...';
  const r = await fetch('/api/wifi/save', {
    method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body
  });
  msg.textContent = await r.text();
}
</script>
)JS";

  html += "</body></html>";

  srv->send(200, "text/html; charset=utf-8", html);
}

static void handleScan() {
  int n = WiFi.scanNetworks(false, true);

  // SSID -> best RSSI (simple dedupe)
  struct Item { String ssid; int rssi; };
  Item best[25];
  int m = 0;

  for (int i = 0; i < n && m < 25; i++) {
    String s = WiFi.SSID(i);
    int r = WiFi.RSSI(i);

    if (s.length() == 0) continue;

    int found = -1;
    for (int k = 0; k < m; k++) {
      if (best[k].ssid == s) { found = k; break; }
    }

    if (found >= 0) {
      if (r > best[found].rssi) best[found].rssi = r; // besser = weniger negativ
    } else {
      best[m++] = { s, r };
    }
  }

  // JSON
  String json = "[";
  for (int i = 0; i < m; i++) {
    if (i) json += ",";
    json += "{\"ssid\":\"" + htmlEscape(best[i].ssid) + "\",\"rssi\":" + String(best[i].rssi) + "}";
  }
  json += "]";
  srv->send(200, "application/json", json);

  WiFi.scanDelete();
}


static void handleSave() {
  String ssid = srv->arg("ssid");
  String pass = srv->arg("pass");

  if (ssid.length() == 0) {
    srv->send(400, "text/plain", "SSID fehlt");
    return;
  }

  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);

  storedSSID = ssid;
  storedPass = pass;

  srv->send(200, "text/plain", "Gespeichert. Verbinde... (AP schliesst bei Erfolg)");
  reconnectFails = 0;
  lastReconnectAttemptMs = 0;

  // Wechsel zu STA (AP bleibt kurz, bis verbunden -> dann stopPortal)
  tryStaConnect();
}

static void handleStatus() {
  String json = "{";
  json += "\"portal\":" + String(portalActive ? "true" : "false");
  json += ",\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
  json += ",\"ssid\":\"" + htmlEscape(WiFi.SSID()) + "\"";
  json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";
  srv->send(200, "application/json", json);
}

// ---------- Core WiFi ----------
static void tryStaConnect() {
  if (storedSSID.length() == 0) return;

  // AP + STA erlaubt Scans/Portal weiterhin, bis wirklich connected
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(storedSSID.c_str(), storedPass.c_str());
  Serial.println("STA begin -> " + storedSSID);

}

static void startPortal() {
  if (portalActive) return;

  // AP Name
  apSsid = apSsid.length() ? apSsid : makeApName("Multi-Sensor");
  WiFi.mode(WIFI_AP_STA); // wichtig: Scan + AP + später STA connect möglich
  WiFi.softAP(apSsid.c_str(), nullptr, 1); // Kanal 1

  portalActive = true;

  Serial.println("WiFi Setup AP gestartet: " + apSsid);
  Serial.println("IP: 192.168.4.1");
}

static void stopPortal() {
  if (!portalActive) return;
  WiFi.softAPdisconnect(true);
  portalActive = false;
}

static void handleNotFound() {
  if (portalActive) {
    // Alles auf das Setup leiten (Captive Portal Verhalten)
    srv->sendHeader("Location", "/wifi", true);
    srv->send(302, "text/plain", "Redirect");
    return;
  }

  // Normalbetrieb: echte 404
  String msg = "Not found: ";
  msg += srv->uri();
  srv->send(404, "text/plain", msg);
}

void wifiMgrBegin(WebServer &server, const String &apNamePrefix) {
  srv = &server;

  prefs.begin("wifi", false);
  storedSSID = prefs.getString("ssid", "");
  storedPass = prefs.getString("pass", "");

  apSsid = makeApName(apNamePrefix);

  // Routen IMMER registrieren (ein Server, kein Port-Konflikt!)
  //srv->on("/", handleRoot);
  srv->on("/wifi", handleWifiPage);
  srv->on("/api/wifi/scan", handleScan);
  srv->on("/api/wifi/save", HTTP_POST, handleSave);
  srv->on("/api/wifi/status", handleStatus);

  srv->onNotFound(handleNotFound);

  // Start: wenn Daten da -> STA versuchen, sonst Portal
  if (storedSSID.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(storedSSID.c_str(), storedPass.c_str());
  } else {
    startPortal();
  }
}

void wifiMgrLoop() {
  // Deferred actions (nie im HTTP-Handler WiFi umschalten)
if (reqForget) {
  reqForget = false;
  prefs.clear();
  storedSSID = "";
  storedPass = "";
  WiFi.disconnect(true);
  startPortal();
}

if (reqStartPortalKeepSta) {
  reqStartPortalKeepSta = false;
  // STA behalten, AP zuschalten
  WiFi.mode(WIFI_AP_STA);
  startPortal();          // startPortal setzt portalActive + softAP
}

if (reqStartPortal) {
  reqStartPortal = false;
  WiFi.disconnect(true);
  startPortal();
}

  // Wenn verbunden -> Portal aus
  if (WiFi.status() == WL_CONNECTED) {
    reconnectFails = 0;
    if (portalActive) stopPortal();
    return;
  }

  // Nicht verbunden:
  // Wenn noch kein Portal aktiv ist -> reconnect Versuche, dann Portal
  if (!portalActive) {
    uint32_t now = millis();
    if (now - lastReconnectAttemptMs >= 5000) {
      lastReconnectAttemptMs = now;
      reconnectFails++;

      if (storedSSID.length() > 0) {
        Serial.println("WLAN getrennt, reconnect Versuch " + String(reconnectFails));
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        WiFi.begin(storedSSID.c_str(), storedPass.c_str());
      }

      if (reconnectFails >= 5) {
        Serial.println("Reconnect fehlgeschlagen -> Setup AP aktivieren");
        startPortal();
      }
    }
  }
}

bool wifiMgrIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

bool wifiMgrPortalActive() {
  return portalActive;
}

void wifiMgrStartPortal() {
  startPortal();
}

void wifiMgrForget() {
  reqForget = true;
  prefs.clear();
  storedSSID = "";
  storedPass = "";
  WiFi.disconnect(true);
  startPortal();
}

bool wifiMgrHasCredentials() {
  return storedSSID.length() > 0;
}

String wifiMgrApSsid() {
  return apSsid;
}

void wifiMgrStartPortalManual() {
  reqStartPortal = true;
  WiFi.disconnect(true);
  startPortal();
}

void wifiMgrRequestStartPortalManual() { reqStartPortal = true; }
void wifiMgrRequestForget() { reqForget = true; }

void wifiMgrRequestWifiUi(uint32_t seconds) {
  wifiUiUntilMs = millis() + (seconds * 1000UL);
}

void wifiMgrRequestStartPortalKeepSta() {
  reqStartPortalKeepSta = true;
}
