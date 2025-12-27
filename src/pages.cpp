#include "pages.h"
#include <LittleFS.h>
#include "auth.h"
#include "version.h"
#include "ntp_time.h"
#include "web_server.h"

static AppConfig* gCfg = nullptr;
static SensorData* gLive = nullptr;
static uint32_t* gLastReadMs = nullptr;
static uint32_t* gLastSendMs = nullptr;

void pagesInit(AppConfig &cfg,
               SensorData *liveData,
               uint32_t *lastReadMs,
               uint32_t *lastSendMs) {
  gCfg = &cfg;
  gLive = liveData;
  gLastReadMs = lastReadMs;
  gLastSendMs = lastSendMs;
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


static String navLink(const String& href, const String& label, const String& current) {
  String cls = (href == current) ? "active" : "";
  return "<a class='" + cls + "' href='" + href + "'>" + label + "</a>";
}

static String headerHtml(const String &title, const String &currentPath) {
  return String(
    "<!doctype html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<link rel='stylesheet' href='/style.css'>"
    "<title>") + title + "</title></head><body>"
    "<div class='topbar'>Multi-Sensor</div>"
    "<div class='menubar'>"
      + navLink("/", "Startseite", currentPath)
      + navLink("/info", "Info", currentPath)
      + navLink("/settings", "Einstellungen", currentPath)
      + navLink("/about", "Über", currentPath)
      + "<span class='right'><a href='/logout'>Abmelden</a></span>"
    "</div>"
    "<div class='content'>";
}

static String footerHtml() { return "</div></body></html>"; }

void pageRoot(ESP8266WebServer &server) {
  if (!requireAuth(server, *gCfg)) return;

  String html = headerHtml("Startseite", "/");

  html += "<div class='grid'>";

  // Sensordaten
  html += "<div class='card'><h2>Sensordaten (live)</h2>";
  html += "<div style='margin: 6px 0 10px;'>";
  html += bmeIsOk()
  ? "<span id='sbadge' class='badge badge-ok'>Sensor OK</span>"
  : "<span class='badge badge-bad'>Sensor Fehler</span>";
  html += "</div>";

  auto agoSec = [](uint32_t tMs)->String {
  if (tMs == 0) return "—";
  uint32_t diff = (millis() - tMs) / 1000;
  return String(diff) + " s";
};

  String lastReadAgo = (gLastReadMs && *gLastReadMs) ? agoSec(*gLastReadMs) : "—";
  String lastSendAgo = (gLastSendMs && *gLastSendMs) ? agoSec(*gLastSendMs) : "—";

  html += "<p class='small'>Letzte Messung: <b>vor " + lastReadAgo + "</b><br>"
        "Letzter UDP-Versand: <b>vor " + lastSendAgo + "</b></p>";
  html += "<p class='small'>Uhrzeit: <b>" + ntpDateTimeString(*gCfg) + "</b></p>";

  if (gLive) {
    html += "<table class='tbl'>";
    html += "<tr><th>Temperatur</th><td class='value' id='tval'>" + String(gLive->temperature_c, 2) + " °C</td></tr>";
    html += "<tr><th>Feuchte</th><td class='value' id='hval'>" + String(gLive->humidity_rh, 2) + " %</td></tr>";
    html += "<tr><th>Druck</th><td class='value' id='pval'>" + String(gLive->pressure_hpa, 2) + " hPa</td></tr>";
    html += "</table>";
  } else {
    html += "<p class='small'>Keine Sensordaten verfügbar.</p>";
  }
  html += "</div>";

  // WLAN
  html += "<div class='card'><h2>WLAN</h2>";
  html += "<table class='tbl'>";

bool wifiOk = (WiFi.status() == WL_CONNECTED);
  html += "<tr><th>Status</th><td>";
  html += wifiOk
  ? "<span class='badge badge-ok'>Verbunden</span>"
  : "<span class='badge badge-bad'>Offline</span>";
  html += "</td></tr>";

  html += "<tr><th>SSID</th><td class='value'>" + WiFi.SSID() + "</td></tr>";
  html += "<tr><th>IP</th><td class='value'>" + WiFi.localIP().toString() + "</td></tr>";
  html += "</table>";
  html += "</div>";

  html += "</div>"; // grid
  html += R"JS(
<script>
function fmt(v, unit){
  if(v === null || v === undefined) return "—";
  return Number(v).toFixed(2) + unit;
}

async function refreshLive(){
  try{
    const r = await fetch('/api/live', {cache:'no-store'});
    if(!r.ok) return;
    const d = await r.json();

    const t = document.getElementById('tval');
    const h = document.getElementById('hval');
    const p = document.getElementById('pval');
    if(t) t.textContent = fmt(d.temperature_c, " °C");
    if(h) h.textContent = fmt(d.humidity_rh, " %");
    if(p) p.textContent = fmt(d.pressure_hpa, " hPa");

    const sb = document.getElementById('sbadge');
    if(sb){
      const ok = (d.temperature_c !== null && d.pressure_hpa !== null);
      sb.className = "badge " + (ok ? "badge-ok" : "badge-bad");
      sb.textContent = ok ? "Sensor OK" : "Sensor Fehler";
    }

  } catch(e) {}
}

// alle 5s (stell auf 10000 für 10s)
setInterval(refreshLive, 5000);
</script>
)JS";
  
  html += footerHtml();
  server.send(200, "text/html", html);
}

void pageInfo(ESP8266WebServer &server) {
  if (!requireAuth(server, *gCfg)) return;

  String html = headerHtml("Info", "/info");

  html += "<div class='grid'>";

  // ===== System =====
  html += "<div class='card'><h2>System</h2><table class='tbl'>";
  html += "<tr><th>Firmware</th><td>" + String(FW_VERSION) + "</td></tr>";
  html += "<tr><th>Chip ID</th><td>" + String(ESP.getChipId(), HEX) + "</td></tr>";
  html += "<tr><th>CPU</th><td>" + String(ESP.getCpuFreqMHz()) + " MHz</td></tr>";
  html += "<tr><th>SDK</th><td>" + String(ESP.getSdkVersion()) + "</td></tr>";
  html += "<tr><th>Uptime</th><td>" + uptimeString() + "</td></tr>";
  html += "</table></div>";

  // ===== Speicher =====
  html += "<div class='card'><h2>Speicher</h2><table class='tbl'>";
  html += "<tr><th>Free Heap</th><td>" + String(ESP.getFreeHeap()) + " B</td></tr>";
  html += "<tr><th>Max Block</th><td>" + String(ESP.getMaxFreeBlockSize()) + " B</td></tr>";
  html += "<tr><th>Fragmentation</th><td>" + String(ESP.getHeapFragmentation()) + " %</td></tr>";
  html += "<tr><th>Flash</th><td>" + String(ESP.getFlashChipSize()/1024) + " KB</td></tr>";
  html += "</table></div>";

  // ===== Netzwerk =====
  html += "<div class='card'><h2>Netzwerk</h2><table class='tbl'>";
  html += "<tr><th>SSID</th><td>" + WiFi.SSID() + "</td></tr>";
  html += "<tr><th>IP</th><td>" + WiFi.localIP().toString() + "</td></tr>";
  html += "<tr><th>Gateway</th><td>" + WiFi.gatewayIP().toString() + "</td></tr>";
  html += "<tr><th>Subnet</th><td>" + WiFi.subnetMask().toString() + "</td></tr>";
  html += "<tr><th>RSSI</th><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
  html += "<tr><th>MAC</th><td>" + WiFi.macAddress() + "</td></tr>";
  html += "<tr><th>mDNS</th><td>http://multi-sensor.local/</td></tr>";
  html += "<tr><th>Hostname</th><td>multi-sensor</td></tr>";
  html += "</table></div>";

  // ===== Zeit =====
  html += "<div class='card'><h2>Zeit</h2><table class='tbl'>";
  html += "<tr><th>NTP gültig</th><td>" + String(ntpIsValid() ? "Ja" : "Nein") + "</td></tr>";
  html += "<tr><th>Lokale Zeit</th><td>" + ntpDateTimeString(*gCfg) + "</td></tr>";
  html += "</table></div>";

  // ===== Sensor =====
  html += "<div class='card'><h2>Sensor</h2><table class='tbl'>";
  html += "<tr><th>Status</th><td>" + String(bmeIsOk() ? "OK" : "Fehler") + "</td></tr>";
  html += "<tr><th>Typ</th><td>BME280</td></tr>";
  html += "<tr><th>I2C</th><td>0x76</td></tr>";
  html += "</table></div>";

  // Aktuelle Einstellungen
  html += "<div class='card'><h2>Aktuelle Einstellungen</h2>";
  html += "<table class='tbl'>";
  html += "<tr><th>Loxone</th><td class='value'>" + gCfg->loxone_ip + ":" + String(gCfg->loxone_udp_port) + "</td></tr>";
  html += "<tr><th>Intervall</th><td class='value'>" + String(gCfg->send_interval_ms) + " ms</td></tr>";
  html += "<tr><th>Sensor-ID</th><td class='value'>" + gCfg->sensor_id + "</td></tr>";
  html += "<tr><th>NTP</th><td class='value'>" + gCfg->ntp_server + "</td></tr>";
  html += "</table>";
  html += "</div>";

  html += "</div>"; // grid

  html += footerHtml();
  server.send(200, "text/html", html);
}

void pageSettings(ESP8266WebServer &server) {
  if (!requireAuth(server, *gCfg)) return;

  // POST: speichern
  if (server.method() == HTTP_POST) {
    gCfg->loxone_ip = server.arg("loxone_ip");
    gCfg->loxone_udp_port = server.arg("loxone_udp_port").toInt();
    gCfg->send_interval_ms = server.arg("send_interval_ms").toInt();
    gCfg->sensor_id = server.arg("sensor_id");
    gCfg->ntp_server = server.arg("ntp_server");
    gCfg->tz_auto_berlin = server.hasArg("tz_auto_berlin");

    if (!gCfg->tz_auto_berlin) {
    gCfg->tz_base_seconds = server.arg("tz_base_seconds").toInt();
    gCfg->dst_add_seconds = server.arg("dst_add_seconds").toInt();
}

// NTP sofort mit neuen Settings neu starten
ntpBegin(*gCfg);


    // optional: Admin Passwort ändern
    String np = server.arg("new_pass");
    if (np.length() >= 6) {
      gCfg->admin_pass_hash = sha1Hex(np);
      gCfg->force_pw_change = false;
    }

    saveConfig(*gCfg);
    server.sendHeader("Location", "/settings", true);
    server.send(302, "text/plain", "");
    return;
  }

  // GET: Formular
  String html = headerHtml("Einstellungen", "/settings");
  html += "<script>"
        "const cb=document.querySelector(\"input[name='tz_auto_berlin']\");"
        "const base=document.querySelector(\"input[name='tz_base_seconds']\");"
        "const dst=document.querySelector(\"input[name='dst_add_seconds']\");"
        "function upd(){ const dis=cb.checked; base.disabled=dis; dst.disabled=dis; }"
        "cb.addEventListener('change',upd); upd();"
        "</script>";

  html += "<form method='POST'>";

  html += "<div class='card'><h2>Loxone</h2>";
  html += "<div class='form-row'><label>Miniserver IP</label>"
          "<input name='loxone_ip' value='" + gCfg->loxone_ip + "'></div>";

  html += "<div class='form-row'><label>UDP Port</label>"
          "<input name='loxone_udp_port' value='" + String(gCfg->loxone_udp_port) + "'></div>";

  html += "<div class='form-row'><label>Sendeintervall (ms)</label>"
          "<input name='send_interval_ms' value='" + String(gCfg->send_interval_ms) + "'></div>";

  html += "<div class='form-row'><label>Sensor-ID</label>"
          "<input name='sensor_id' value='" + gCfg->sensor_id + "'></div>";
  html += "</div>";

  html += "<div class='card'><h2>NTP / Zeitzone</h2>";

  html += "<div class='form-row'><label>NTP Server</label>"
        "<input name='ntp_server' value='" + gCfg->ntp_server + "'></div>";

  html += "<div class='form-row'>"
        "<label><input type='checkbox' name='tz_auto_berlin' "
        + String(gCfg->tz_auto_berlin ? "checked" : "") +
        "> Automatische Zeitzone (Europe/Berlin)</label>"
        "<div class='small'>Sommerzeit wird automatisch berechnet.</div>"
        "</div>";

  html += "<div class='form-row'><label>Basis-Offset (Sekunden)</label>"
        "<input name='tz_base_seconds' value='" + String(gCfg->tz_base_seconds) + "'></div>";

  html += "<div class='form-row'><label>DST-Zuschlag (Sekunden)</label>"
        "<input name='dst_add_seconds' value='" + String(gCfg->dst_add_seconds) + "'></div>";

  html += "</div>";

  html += "<div class='card'><h2>Admin</h2>";
  html += "<div class='form-row'><label>Neues Passwort</label>"
          "<input type='password' name='new_pass' placeholder='(leer lassen = unverändert)'></div>";
  html += "</div>";

  html += "<div class='actions'>"
          "<button class='btn-primary' type='submit'>Speichern</button>"
          "<a class='btn btn-secondary' href='/backup'>Backup</a>"
          "<a class='btn btn-secondary' href='/restore'>Restore</a>"
          "<a class='btn btn-secondary' href='/factory_reset'>Factory Reset</a>"
          "</div>";

  html += "</form>";

  html += footerHtml();
  server.send(200, "text/html", html);
}


void pageAbout(ESP8266WebServer &server) {
  if (!requireAuth(server, *gCfg)) return;

  String html = headerHtml("Über", "/about");

  html += "<div class='card'>";
  html += "<h2>" + String(FW_NAME) + "</h2>";

  html += "<table class='tbl'>";
  html += "<tr><th>Firmware</th><td>" + String(FW_NAME) + "</td></tr>";
  html += "<tr><th>Version</th><td>" + String(FW_VERSION) + "</td></tr>";
  html += "<tr><th>Build-Datum</th><td>" + String(FW_DATE) + "</td></tr>";
  html += "<tr><th>Autor</th><td>" + String(FW_AUTHOR) + "</td></tr>";
  html += "<tr><th>Copyright</th><td>" + String(FW_COPYRIGHT) + "</td></tr>";
  html += "</table>";

  html += "<p class='small' style='margin-top:14px;'>"
          "ESP8266-basierter Multi-Sensor mit BME280, "
          "Weboberfläche, Login-System, NTP-Zeit, "
          "UDP-Ausgabe für Loxone und LittleFS."
          "</p>";

  html += "</div>";

  html += footerHtml();
  server.send(200, "text/html; charset=utf-8", html);
}


void pageBackup(ESP8266WebServer &server) {
  if (!requireAuth(server, *gCfg)) return;

  if (!LittleFS.exists("/config.json")) {
    server.send(404, "text/plain; charset=utf-8", "config.json nicht gefunden");
    return;
  }

  File f = LittleFS.open("/config.json", "r");
  server.sendHeader("Content-Disposition", "attachment; filename=\"config.json\"");
  server.streamFile(f, "application/json");
  f.close();
}

void pageRestoreForm(ESP8266WebServer &server) {
  if (!requireAuth(server, *gCfg)) return;

  String html = headerHtml("Restore", "/settings");
  html += "<div class='card'><h2>Restore config.json</h2>"
          "<form method='POST' action='/restore' enctype='multipart/form-data'>"
          "<div class='form-row'><label>config.json auswählen</label>"
          "<input type='file' name='cfg' accept='application/json' required></div>"
          "<div class='actions'><button class='btn-primary' type='submit'>Upload</button></div>"
          "</form></div>";
  html += footerHtml();
  server.send(200, "text/html; charset=utf-8", html);
}

static File gUploadFile;

void pageRestoreUpload(ESP8266WebServer &server) {
  if (!requireAuth(server, *gCfg)) return;

  HTTPUpload& up = server.upload();

  if (up.status == UPLOAD_FILE_START) {
    if (LittleFS.exists("/config.json")) LittleFS.remove("/config.json");
    gUploadFile = LittleFS.open("/config.json", "w");
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (gUploadFile) gUploadFile.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (gUploadFile) gUploadFile.close();

    // optional: sofort neu laden + NTP neu starten
    loadConfig(*gCfg);
    ntpBegin(*gCfg);

    server.sendHeader("Location", "/settings", true);
    server.send(302, "text/plain", "");
  }
}

void pageFactoryResetForm(ESP8266WebServer &server) {
  if (!requireAuth(server, *gCfg)) return;

  String html = headerHtml("Factory Reset", "/settings");
  html += "<div class='card'><h2>Werkseinstellungen</h2>"
          "<p class='small'>Das löscht die Konfiguration (config.json). Danach startet das Gerät neu.</p>"
          "<form method='POST' action='/factory_reset'>"
          "<div class='actions'>"
          "<button class='btn-primary' type='submit'>Ja, löschen & neu starten</button>"
          "<a class='btn btn-secondary' href='/settings'>Abbrechen</a>"
          "</div></form></div>";
  html += footerHtml();
  server.send(200, "text/html; charset=utf-8", html);
}

void pageFactoryResetDo(ESP8266WebServer &server) {
  if (!requireAuth(server, *gCfg)) return;

  if (LittleFS.exists("/config.json")) LittleFS.remove("/config.json");

  server.send(200, "text/plain; charset=utf-8", "OK, reboot...");
  delay(300);
  ESP.restart();
}

void apiLive(ESP8266WebServer &server) {
  if (!requireAuth(server, *gCfg)) return;

  // Werte absichern
  float t = (gLive ? gLive->temperature_c : NAN);
  float h = (gLive ? gLive->humidity_rh   : NAN);
  float p = (gLive ? gLive->pressure_hpa  : NAN);

  // Letzte Messung / Send (falls du Punkt 2 vorher schon eingebaut hast)
  uint32_t lr = (gLastReadMs ? *gLastReadMs : 0);
  uint32_t ls = (gLastSendMs ? *gLastSendMs : 0);

  auto jsNum = [](float v)->String { return isnan(v) ? "null" : String(v, 2); };

  String json = "{";
  json += "\"temperature_c\":" + jsNum(t) + ",";
  json += "\"humidity_rh\":"   + jsNum(h) + ",";
  json += "\"pressure_hpa\":"  + jsNum(p) + ",";
  json += "\"wifi_ok\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"last_read_ms\":" + String(lr) + ",";
  json += "\"last_send_ms\":" + String(ls);
  json += "}";

  server.send(200, "application/json; charset=utf-8", json);
}
