#include "wifi_mgr.h"
#include <WiFi.h>
#include <Preferences.h>
#include "pages.h"


// ===== Requests =====
static bool reqStartPortal = false;
static bool reqStartPortalKeepSta = false;
static bool reqForget = false;
static uint32_t wifiUiUntilMs = 0;  // Zeitfenster für /wifi im Heimnetz

// ===== Storage =====
static Preferences prefs;
static WebServer *srv = nullptr;

// ===== AP/STA =====
static bool portalActive = false;

static String apSsid;
static String apPass;

static String storedSSID;
static String storedPass;

// ===== Fallback (Defaults) =====
static uint8_t  cfgMaxFails = 5;
static uint32_t cfgOfflineGraceMs = 180000;   // 3 Minuten
static uint32_t cfgRetryIntervalMs = 10000;   // 10 Sekunden

// ===== Runtime =====
static uint8_t  reconnectFails = 0;
static uint32_t lastReconnectAttemptMs = 0;
static uint32_t lastConnectedMs = 0;

// -------------------------------------------------
// Helpers
// -------------------------------------------------
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

static String last4LowerFromEfuse() {
  uint64_t mac = ESP.getEfuseMac();
  uint32_t low = (uint32_t)(mac & 0xFFFFFFFF);
  String hex = String(low, HEX);
  hex.toLowerCase();
  while (hex.length() < 8) hex = "0" + hex;
  return hex.substring(hex.length() - 4);
}

static void buildApCredentials(const String &prefix) {
  const String suffix = last4LowerFromEfuse();      // z.B. "cd43"
  apSsid = prefix + "-" + suffix;                   // "Multi-Sensor-cd43"
  apPass = apSsid;                                  // Passwort exakt gleich
}

static bool wifiUiAllowed() {
  return portalActive || !wifiMgrHasCredentials() || (millis() < wifiUiUntilMs);
}

// -------------------------------------------------
// Core WiFi actions
// -------------------------------------------------
static void startPortal(bool keepSta) {
  if (portalActive) return;

  // AP+STA damit Scan geht und STA parallel verbinden darf
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);

  // keepSta: optional NICHT disconnecten – wir lassen WiFi.begin wie es ist
  if (!keepSta) {
    // STA nicht hart trennen, sonst kann Scan/Portal manchmal "zappeln"
    // aber wir setzen gleich WiFi.begin bei Bedarf
  }

  const bool ok = WiFi.softAP(apSsid.c_str(), apPass.c_str(), 1); // WPA2, Channel 1
  delay(100);

  portalActive = true;

  Serial.println("[WiFiMgr] Setup AP gestartet");
  Serial.println("  SSID: " + apSsid);
  Serial.println("  PASS: " + apPass);
  Serial.println("  IP  : " + WiFi.softAPIP().toString());
  if (!ok) Serial.println("[WiFiMgr] WARN: softAP() returned false");
}

static void stopPortal() {
  if (!portalActive) return;
  WiFi.softAPdisconnect(true);
  portalActive = false;
}

static void tryStaConnect() {
  if (!wifiMgrHasCredentials()) return;

  WiFi.mode(portalActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.setSleep(false);

  WiFi.begin(storedSSID.c_str(), storedPass.c_str());
}

static void handleWifiPage() {
  // optional: /wifi im Heimnetz nur erlauben, wenn freigeschaltet
  // wenn du /wifi immer erlauben willst, diese if weg
  if (!wifiUiAllowed()) { srv->send(403, "text/plain", "Forbidden"); return;}

  String html = pagesHeaderPublic(*srv, "WLAN Setup", "/wifi");

  // WLAN Setup Card
  html += "<div class='card'><h2>WLAN Setup</h2>";
  html += "<div class='hint'>Wähle ein WLAN aus und speichere das Passwort. "
          "Der Setup-AP schließt nach erfolgreicher Verbindung automatisch.</div>";
  html += "<div class='actions'>"
          "<button class='btn-primary' type='button' onclick='scan()'>WLAN scannen</button>"
          "</div>";
  html += "<div id='msg' class='small mt-12'></div>";
  html += "<div id='list' class='mt-12'></div>";
  html += "</div>";

  // Status Card
  html += "<div class='card'><h2>Status</h2><table class='tbl'>";
  html += "<tr><th>Portal aktiv</th><td>" + String(portalActive ? "ja" : "nein") + "</td></tr>";
  html += "<tr><th>Setup-SSID</th><td>" + htmlEscape(apSsid) + "</td></tr>";
  html += "<tr><th>Setup-Passwort</th><td>" + htmlEscape(apPass) + "</td></tr>";
  html += "<tr><th>Setup-IP</th><td>" + WiFi.softAPIP().toString() + "</td></tr>";
  html += "<tr><th>STA Status</th><td>" + String(WiFi.status()) + "</td></tr>";
  html += "<tr><th>STA SSID</th><td>" + htmlEscape(WiFi.SSID()) + "</td></tr>";
  html += "<tr><th>STA IP</th><td>" + WiFi.localIP().toString() + "</td></tr>";
  html += "</table></div>";

  // JS Scan/Connect (wie vorher, nur in Cards)
  html += R"JS(
<script>
async function scan(){
  const msg  = document.getElementById('msg');
  const list = document.getElementById('list');
  msg.textContent = 'Scanne...';
  list.innerHTML = '';

  const r = await fetch('/api/wifi/scan');
  if(!r.ok){ msg.textContent = 'Scan fehlgeschlagen'; return; }
  const j = await r.json();

  msg.textContent = 'Gefunden: ' + j.length;

  if(!j.length){
    list.innerHTML = '<div class="card"><p class="small">Keine Netzwerke gefunden.</p></div>';
    return;
  }

  let h = '<div class="card"><h2>Netzwerke</h2><table class="tbl">';
  h += '<tr><th>SSID</th><th>RSSI</th><th>Aktion</th></tr>';

  j.sort((a,b)=> (b.rssi||-999) - (a.rssi||-999));

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

  html += pagesFooter();
  srv->send(200, "text/html; charset=utf-8", html);
}


static void handleScan() {
  int n = WiFi.scanNetworks(false, true);

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
      if (r > best[found].rssi) best[found].rssi = r;
    } else {
      best[m++] = { s, r };
    }
  }

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

  // Save
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);

  storedSSID = ssid;
  storedPass = pass;

  reconnectFails = 0;
  lastReconnectAttemptMs = 0;

  // Connect (AP bleibt bis WL_CONNECTED, dann stopPortal in loop)
  tryStaConnect();

  srv->send(200, "text/plain", "Gespeichert. Verbinde...");
}

// -------------------------------------------------
// Public API
// -------------------------------------------------
void wifiMgrBegin(WebServer &server, const String &apNamePrefix) {
  srv = &server;

  prefs.begin("wifi", false);
  storedSSID = prefs.getString("ssid", "");
  storedPass = prefs.getString("pass", "");

  buildApCredentials(apNamePrefix);

  // Routen IMMER registrieren (ein WebServer!)
  srv->on("/wifi", HTTP_GET, handleWifiPage);
  srv->on("/api/wifi/scan", HTTP_GET, handleScan);
  srv->on("/api/wifi/save", HTTP_POST, handleSave);

  // Boot: STA versuchen, sonst Portal starten
  WiFi.setSleep(false);

  if (wifiMgrHasCredentials()) {
    tryStaConnect();
  } else {
    startPortal(false);
  }
}

void wifiMgrLoop() {
  // Requests abarbeiten
  if (reqForget) {
    reqForget = false;
    prefs.clear();
    storedSSID = "";
    storedPass = "";
    reconnectFails = 0;
    lastReconnectAttemptMs = 0;
    WiFi.disconnect(true);
    startPortal(false);
  }

  if (reqStartPortal || reqStartPortalKeepSta) {
    bool keepSta = reqStartPortalKeepSta;
    reqStartPortal = false;
    reqStartPortalKeepSta = false;

    startPortal(keepSta);

    // Wenn wir Credentials haben, parallel STA verbinden (AP+STA)
    if (wifiMgrHasCredentials()) {
      tryStaConnect();
    }
  }

  // Connected?
  if (WiFi.status() == WL_CONNECTED) {
    reconnectFails = 0;
    lastConnectedMs = millis();
    if (portalActive) stopPortal();
    return;
  }

  const uint32_t now = millis();
  const bool offlineTooLong = (lastConnectedMs > 0) && ((now - lastConnectedMs) > cfgOfflineGraceMs);

  // Nicht verbunden: Reconnect / Fallback
  if (!portalActive) {
    if (offlineTooLong || reconnectFails >= cfgMaxFails) {
      startPortal(false);
      return;
    }

    if (wifiMgrHasCredentials() && (now - lastReconnectAttemptMs >= cfgRetryIntervalMs)) {
      lastReconnectAttemptMs = now;
      reconnectFails++;
      tryStaConnect();
    }
  }
}

bool wifiMgrIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

bool wifiMgrPortalActive() {
  return portalActive;
}

bool wifiMgrHasCredentials() {
  return storedSSID.length() > 0;
}

String wifiMgrApSsid() {
  return apSsid;
}

String wifiMgrApPass() {
  return apPass;
}

// Requests
void wifiMgrRequestStartPortal() {
  reqStartPortal = true;
}

void wifiMgrRequestStartPortalKeepSta() {
  reqStartPortalKeepSta = true;
}

void wifiMgrRequestForget() {
  reqForget = true;
}

void wifiMgrRequestWifiUi(uint32_t seconds) {
  wifiUiUntilMs = millis() + (seconds * 1000UL);
}

void wifiMgrConfigureFallback(uint8_t maxFails,
                              uint32_t offlineGraceMs,
                              uint32_t retryIntervalMs) {
  cfgMaxFails = maxFails;
  cfgOfflineGraceMs = offlineGraceMs;
  cfgRetryIntervalMs = retryIntervalMs;
}
