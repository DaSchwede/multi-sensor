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

  // wichtig: STA-Init „clean“
  WiFi.setSleep(false);
  WiFi.mode(portalActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(storedSSID.c_str(), storedPass.c_str());
  Serial.println(String("STA begin -> ") + storedSSID);
}

// ---------- HTTP ----------
static void handleWifiPage() {
  if (!wifiUiAllowed()) {
    srv->send(403, "text/plain", "Forbidden");
    return;
  }

  String html;
  html.reserve(6500);

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

  // Setup card
  html += "<div class='card'><h2>WLAN Setup</h2>";
  html += "<p class='small'>Wähle ein WLAN aus, gib das Passwort ein und speichere. Sobald verbunden, kannst du zur Startseite wechseln.</p>";

  html += "<div class='actions'>";
  html += "<button class='btn-primary' type='button' onclick='scan()'>WLAN scannen</button>";
  html += "</div>";

  html += "<div id='msg' class='small mt-12'></div>";

  // Connect form (hidden until SSID chosen)
  html += "<div id='conn' class='mt-12' style='display:none;'>";
  html += "  <div class='card'><h2>Verbinden</h2>";
  html += "    <div class='form-row'><label>SSID</label>"
          "      <input id='ssid' readonly></div>";
  html += "    <div class='form-row'><label>Passwort</label>"
          "      <input id='pass' type='password' placeholder='Passwort (leer lassen für offen)'></div>";
  html += "    <div class='actions'>"
          "      <button class='btn-primary' type='button' onclick='saveConnect()'>Speichern & Verbinden</button>"
          "      <button class='btn-secondary' type='button' onclick='cancelConn()'>Abbrechen</button>"
          "    </div>";
  html += "  </div>";
  html += "</div>";

  // Networks list
  html += "<div id='list' class='mt-12'></div>";

  // Status card
  html += "<div class='card'><h2>Status</h2><table class='tbl'>";
  html += "<tr><th>Setup-AP</th><td>" + htmlEscape(apSsid) + "</td></tr>";
  html += "<tr><th>Setup-IP</th><td>192.168.4.1</td></tr>";
  html += "<tr><th>Verbunden</th><td id='st_conn'>—</td></tr>";
  html += "<tr><th>SSID</th><td id='st_ssid'>—</td></tr>";
  html += "<tr><th>IP</th><td id='st_ip'>—</td></tr>";
  html += "</table>";

  // Go buttons (shown when connected)
  html += "<div id='go' class='actions' style='display:none'>";
  html += "<a id='go_ip' class='btn btn-secondary' href='/'>Startseite (IP)</a>";
  html += "<a id='go_mdns' class='btn btn-secondary' href='/'>Startseite (mDNS)</a>";
  html += "</div>";

  html += "</div>"; // status card
  html += "</div>"; // content

  // JS
  html += R"JS(
<script>
let gSelected = null;
let pollTimer = null;
window._afterSave = false;   // <-- wichtig: default KEIN redirect

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

  // ab jetzt darf redirect passieren
  window._afterSave = true;

  if(!pollTimer){
    pollTimer = setInterval(pollStatus, 1200);
  }
}

async function pollStatus(){
  const r = await fetch('/api/wifi/status', { cache:'no-store' });
  const s = await r.json();

  // Buttons updaten (optional)
  const goIp   = document.getElementById('go_ip');
  const goMdns = document.getElementById('go_mdns');
  const goWrap = document.getElementById('go');

  if(goWrap) goWrap.style.display = (s.connected ? 'flex' : 'none');
  if(goIp && s.ip) goIp.href = 'http://' + s.ip + '/';
  if(goMdns) goMdns.href = 'http://' + (s.host || 'multi-sensor') + '.local/';

  // ✅ Redirect NUR wenn wir wirklich gerade gespeichert haben
  if(window._afterSave && s.connected && s.ip && s.ip !== '0.0.0.0'){
    clearInterval(pollTimer); pollTimer = null;
    window._afterSave = false;
    setTimeout(()=>{ window.location.href = 'http://' + s.ip + '/'; }, 700);
  }
}
async function scan(){
  const msg  = document.getElementById('msg');
  const list = document.getElementById('list');
  msg.textContent = 'Scanne...';
  list.innerHTML = '';

  try {
    const r = await fetch('/api/wifi/scan', { cache:'no-store' });

    // wenn Server z.B. HTML/Redirect liefert, sieht man es hier
    const ct = (r.headers.get('content-type') || '').toLowerCase();
    if(!r.ok){
      const t = await r.text();
      msg.textContent = `Scan fehlgeschlagen (${r.status}): ${t.substring(0,120)}`;
      return;
    }
    if(!ct.includes('application/json')){
      const t = await r.text();
      msg.textContent = `Scan: unerwartete Antwort (${ct}): ${t.substring(0,120)}`;
      return;
    }

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

  } catch(e){
    msg.textContent = 'JS Fehler beim Scan: ' + (e && e.message ? e.message : e);
  }
}


pollStatus();
setInterval(pollStatus, 3000);
</script>
)JS";

  html += "</body></html>";
  srv->send(200, "text/html; charset=utf-8", html);
}
//)JS";

static void handleScan() {
  // fürs Scannen brauchen wir STA kurz aktiv
  wifi_mode_t m = WiFi.getMode();
  bool needSta = (m == WIFI_MODE_AP);

  if (needSta) { WiFi.mode(WIFI_AP_STA); delay(50); }

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
  if (needSta) { WiFi.mode(WIFI_AP); delay(50); }
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
  json += ",\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
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
    fails++;
    Serial.println("WLAN getrennt, reconnect Versuch " + String(fails));
    WiFi.reconnect();

    if (fails >= MAX_FAILS) {
      Serial.println("Reconnect fehlgeschlagen -> Setup AP aktivieren");
      startApOnlyPortal();
    }
    return;
  }

  // Portal aktiv: trotzdem versuchen, wieder ins WLAN zu kommen (ohne Spam)
  Serial.println("Portal aktiv -> STA retry zu " + storedSSID);
  staBegin();
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
