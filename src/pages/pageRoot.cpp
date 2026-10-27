#include <Arduino.h>
#include <WebServer.h>
#include <math.h>
#include "pages.h"
#include "auth.h"

// Zahl mit Dezimalkomma, "—" wenn kein gültiger Wert
static String deNum(float v, int decimals) {
  if (isnan(v)) return String("—");
  String s = String(v, decimals);
  s.replace(".", ",");
  return s;
}

// Live-Karte: CO₂ mit Lüftungs-Skala als Blickfang, darunter das Raumklima.
// IDs (tval/hval/pval/cval, sbadge, rescanBtn) werden vom JS unten aktualisiert.
static String cardLiveBox() {
  SensorData* live = pagesLive();
  const float t   = live ? live->temperature_c : NAN;
  const float h   = live ? live->humidity_rh   : NAN;
  const float p   = live ? live->pressure_hpa  : NAN;
  const float co2 = live ? live->co2_ppm       : NAN;

  String s;
  s += "<section class='card air' id='air' data-level='none' aria-label='Raumluft'>";

  s += "<div class='air-top'>"
         "<div>"
           "<h2 class='air-title'>Raumluft</h2>"
           "<div class='air-co2'><span class='air-num' id='cval'>" + deNum(co2, 0) + "</span>"
           "<span class='air-unit'>ppm CO₂</span></div>"
           "<p class='air-verdict' id='cverdict'>Messwert wird geladen</p>"
         "</div>"
         "<div class='air-status'>"
           "<span id='sbadge' class='badge badge-ok'>Sensoren OK</span>"
           "<button id='rescanBtn' class='icon-btn' type='button' "
             "title='Sensoren neu erkennen' aria-label='Sensoren neu erkennen' "
             "onclick='rescanSensors()'>&#x21bb;</button>"
         "</div>"
       "</div>";

  // Skala 400 … 2000 ppm: gut < 1000, bald lüften < 1400, darüber jetzt lüften
  s += "<div class='air-scale' aria-hidden='true'>"
         "<div class='air-bar'>"
           "<span class='seg good'></span><span class='seg mid'></span><span class='seg bad'></span>"
           "<span class='air-mark' id='cmark'></span>"
         "</div>"
         "<div class='air-ticks'>"
           "<span style='left:0'>400</span>"
           "<span style='left:37.5%'>1000</span>"
           "<span style='left:62.5%'>1400</span>"
           "<span style='left:100%'>2000</span>"
         "</div>"
       "</div>";

  s += "<div class='air-climate'>"
         "<div class='clim'><span class='clim-lbl'>Temperatur</span>"
           "<span class='clim-val'><b id='tval'>" + deNum(t, 1) + "</b> °C</span>"
           "<span class='clim-note' id='tnote'></span></div>"
         "<div class='clim'><span class='clim-lbl'>Luftfeuchte</span>"
           "<span class='clim-val'><b id='hval'>" + deNum(h, 0) + "</b> %</span>"
           "<span class='clim-note' id='hnote'></span></div>"
         "<div class='clim'><span class='clim-lbl'>Luftdruck</span>"
           "<span class='clim-val'><b id='pval'>" + deNum(p, 0) + "</b> hPa</span>"
           "<span class='clim-note' id='pnote'></span></div>"
       "</div>";

  s += "<p class='air-foot small' id='astamp'>Aktualisiert sich alle 5 Sekunden.</p>";
  s += "</section>";
  return s;
}

static String cardByIdRoot(const String &id) {
  if (id == "system")   return cardSystem();
  if (id == "network")  return cardNetzwerk();
  if (id == "sensor")   return cardSensor();
  if (id == "memory")   return cardSpeicher();
  if (id == "time")     return cardZeit();
  if (id == "udp" || id == "settings") return cardUdpEinstellungen();
  if (id == "mqtt")     return cardMqtt();
  return "";
}

static void appendAutoRefreshJS(String &html) {
  html += R"JS(
<script>
(function(){
  const $ = id => document.getElementById(id);
  const has = v => v !== null && v !== undefined && !isNaN(v);
  function num(v, d){
    return has(v) ? Number(v).toLocaleString('de-DE',{minimumFractionDigits:d,maximumFractionDigits:d,useGrouping:false}) : '—';
  }
  function setNote(id, txt){ const e = $(id); if(e) e.textContent = txt || ''; }

  function render(d){
    const co2 = d.co2_ppm, t = d.temperature_c, h = d.humidity_rh, p = d.pressure_hpa;
    $('cval').textContent = has(co2) ? String(Math.round(co2)) : '—';
    $('tval').textContent = num(t, 1);
    $('hval').textContent = num(h, 0);
    $('pval').textContent = num(p, 0);

    // CO₂-Ampel
    let lvl = 'none', txt = 'Keine Messwerte vom CO₂-Sensor';
    if(has(co2)){
      if(co2 < 1000){ lvl = 'good'; txt = 'Luft ist gut'; }
      else if(co2 < 1400){ lvl = 'mid'; txt = 'Bald lüften'; }
      else { lvl = 'bad'; txt = 'Jetzt lüften'; }
    }
    $('air').dataset.level = lvl;
    $('cverdict').textContent = txt;
    const m = $('cmark');
    if(has(co2)){
      const pos = Math.max(0, Math.min(1, (co2 - 400) / 1600));
      m.style.left = (pos * 100).toFixed(1) + '%';
      m.hidden = false;
    } else m.hidden = true;

    // Raumklima in Worten
    setNote('tnote', !has(t) ? '' : t < 18 ? 'kühl' : t <= 24 ? 'angenehm' : 'warm');
    setNote('hnote', !has(h) ? '' : h < 35 ? 'trocken' : h <= 60 ? 'angenehm' : 'feucht');
    setNote('pnote', !has(p) ? '' : p < 1000 ? 'tief' : p <= 1025 ? 'normal' : 'hoch');

    // Sensorstatus
    const ok = has(t) && has(p) && has(co2);
    const sb = $('sbadge');
    sb.className = 'badge ' + (ok ? 'badge-ok' : 'badge-bad');
    sb.textContent = ok ? 'Sensoren OK' : (has(co2) ? 'BME280 fehlt' : (has(t) ? 'SCD40 fehlt' : 'Keine Sensoren'));

    $('astamp').textContent = 'Stand ' + new Date().toLocaleTimeString('de-DE') + ', aktualisiert sich alle 5 Sekunden.';
  }

  async function refreshLive(){
    try{
      const r = await fetch('/api/live', {cache:'no-store'});
      if(!r.ok) return;
      render(await r.json());
    }catch(e){}
  }

  window.rescanSensors = async function(){
    const b = $('rescanBtn');
    if(b){ b.disabled = true; b.classList.add('spin'); }
    try{
      await fetch('/action/rescan_sensors', {
        method:'POST',
        headers:{'Content-Type':'application/x-www-form-urlencoded'},
        body:'src=live'
      });
      await new Promise(res => setTimeout(res, 300));
      await refreshLive();
    }catch(e){
    }finally{
      if(b){ b.disabled = false; b.classList.remove('spin'); }
    }
  };

  refreshLive();
  setInterval(refreshLive, 5000);
})();
</script>
)JS";
}

void pageRoot(WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  String html = pagesHeaderAuth("Startseite", "/");

  auto order = pagesSplitCsv(cfg->ui_root_order);

  // "live" steht immer oben über die ganze Breite, die übrigen Karten in zwei Spalten
  bool showLive = false;
  String left, right;
  int idx = 0;
  int unknown = 0;

  for (auto &id : order) {
    if (id == "live") { showLive = true; continue; }
    String card = cardByIdRoot(id);
    if (card.length()) {
      (idx++ % 2 == 0 ? left : right) += card;
    } else {
      unknown++;
    }
  }

  // Fallback: Standard wenn leer / alles unbekannt
  if (!showLive && idx == 0) {
    showLive = true;
    left = cardNetzwerk();
    right = cardZeit();
    unknown = 0;
  }

  if (showLive) html += cardLiveBox();

  if (left.length() || right.length()) {
    html += "<div class='columns'>"
            "<div class='col'>" + left + "</div>"
            "<div class='col'>" + right + "</div>"
            "</div>";
  }

  if (unknown > 0) {
    html += "<div class='card'><p class='small'>Hinweis: " + String(unknown) +
            " unbekannte ID(s) in ui_root_order wurden ignoriert.</p></div>";
  }

  if (showLive) appendAutoRefreshJS(html);

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
