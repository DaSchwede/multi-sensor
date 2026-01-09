#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "pages.h"
#include "auth.h"

static String cardLiveBox() {
  SensorData* live = pagesLive();

  String h;
  h += "<div class='card'><h2>Live</h2>";
  h += "<div class='badge-row'><span id='sbadge' class='badge badge-ok'>Sensor OK</span></div>";  h += "<table class='tbl'>";
  h += "<tr><th>Temperatur</th><td class='value' id='tval'>" + String(live ? live->temperature_c : 0, 2) + " °C</td></tr>";
  h += "<tr><th>Feuchte</th><td class='value' id='hval'>" + String(live ? live->humidity_rh   : 0, 2) + " %</td></tr>";
  h += "<tr><th>Druck</th><td class='value' id='pval'>" + String(live ? live->pressure_hpa  : 0, 2) + " hPa</td></tr>";
  h += "<tr><th>CO₂</th><td class='value' id='cval'>" + String(live ? live->co2_ppm : 0, 0) + " ppm</td></tr>";
  h += "</table>";
  h += "<div class='small mt-8'>Aktualisiert automatisch.</div>";
  h += "</div>";
  return h;
}

static String cardByIdRoot(const String &id) {
  // Root darf alles anzeigen (du kannst es später einschränken)
  if (id == "live")     return cardLiveBox();
  if (id == "system")   return cardSystem();
  if (id == "network")  return cardNetzwerk();
  if (id == "sensor")   return cardSensor();
  if (id == "memory")   return cardSpeicher();
  if (id == "time")     return cardZeit();
  if (id == "settings") return cardAktuelleEinstellungen();
  return "";
}

static void appendAutoRefreshJS(String &html) {
  html += R"JS(
<script>
function fmt(v, unit){
  if(v === null || v === undefined) return "—";
  return Number(v).toFixed(2) + unit;
}
function fmtInt(v, unit){
  if(v === null || v === undefined) return "—";
  return Math.round(Number(v)) + unit;
}
async function refreshLive(){
  try{
    const r = await fetch('/api/live', {cache:'no-store'});
    if(!r.ok) return;
    const d = await r.json();

    const t = document.getElementById('tval');
    const h = document.getElementById('hval');
    const p = document.getElementById('pval');
    const c = document.getElementById('cval');

    if(t) t.textContent = fmt(d.temperature_c, " °C");
    if(h) h.textContent = fmt(d.humidity_rh, " %");
    if(p) p.textContent = fmt(d.pressure_hpa, " hPa");
    if(c) c.textContent = fmtInt(d.co2_ppm, " ppm");

    const sb = document.getElementById('sbadge');
    if(sb){
      const ok =
        (d.temperature_c !== null && d.temperature_c !== undefined) &&
        (d.pressure_hpa  !== null && d.pressure_hpa  !== undefined) &&
        (d.co2_ppm       !== null && d.co2_ppm       !== undefined);

      sb.className = "badge " + (ok ? "badge-ok" : "badge-bad");
      sb.textContent = ok ? "Sensor OK" : "Sensor Fehler";
    }
  }catch(e){}
}
setInterval(refreshLive, 5000);
</script>
)JS";
}

void pageRoot(ESP8266WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  String html = pagesHeaderAuth("Startseite", "/");

  auto order = pagesSplitCsv(cfg->ui_root_order);

  String left, right;
  int idx = 0;
  int unknown = 0;

  for (auto &id : order) {
    String card = cardByIdRoot(id);
    if (card.length()) {
      (idx++ % 2 == 0 ? left : right) += card;
    } else {
      unknown++;
    }
  }

  // Fallback: Standard wenn leer / alles unknown
  if (idx == 0) {
    left  = cardLiveBox() + cardNetzwerk();
    right = cardZeit();
    unknown = 0;
  }

  html += "<div class='columns'>"
          "<div class='col'>" + left + "</div>"
          "<div class='col'>" + right + "</div>"
          "</div>";

  if (unknown > 0) {
    html += "<div class='card'><p class='small'>Hinweis: " + String(unknown) +
            " unbekannte ID(s) in ui_root_order wurden ignoriert.</p></div>";
  }

  appendAutoRefreshJS(html);

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
