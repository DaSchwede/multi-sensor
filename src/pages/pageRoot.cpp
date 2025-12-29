#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "pages.h"
#include "auth.h"

void pageRoot(ESP8266WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  SensorData* live = pagesLive();

  String html = pagesHeaderAuth("Startseite", "/");
  html += "<div class='columns'>";

  // Links: Live-Werte
  html += "<div class='col'>";
  html += "<div class='card'><h2>Live</h2>";
  html += "<div style='margin: 6px 0 10px;'><span id='sbadge' class='badge badge-ok'>Sensor OK</span></div>";

  html += "<table class='tbl'>";
  html += "<tr><th>Temperatur</th><td class='value' id='tval'>" + String(live ? live->temperature_c : 0, 2) + " °C</td></tr>";
  html += "<tr><th>Feuchte</th><td class='value' id='hval'>" + String(live ? live->humidity_rh   : 0, 2) + " %</td></tr>";
  html += "<tr><th>Druck</th><td class='value' id='pval'>" + String(live ? live->pressure_hpa  : 0, 2) + " hPa</td></tr>";
  html += "</table>";

  html += "<div class='small' style='margin-top:10px;'>Aktualisiert automatisch.</div>";
  html += "</div>"; // card
  html += "</div>"; // col

  // Rechts: kurze Info-Kacheln (optional – kannst du tauschen)
  html += "<div class='col'>";
  html += cardNetzwerk();
  html += cardZeit();
  html += "</div>"; // col

  html += "</div>"; // columns

  // Auto-Refresh (ohne Reload)
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
  }catch(e){}
}
setInterval(refreshLive, 5000);
</script>
)JS";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
