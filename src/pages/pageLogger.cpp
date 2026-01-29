#include <Arduino.h>
#include <WebServer.h>
#include "pages.h"
#include "settings_config/settings_common.h"

void pageLogger(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String html = pagesHeaderAuth("Logger – Verlauf", "/logger");

  html += "<div class='card'><h2>Verlauf</h2>";

  html += "<div class='form-row'><label>Zeitraum</label>"
          "<select id='range'>"
          "<option value='24h'>Letzte 24 Stunden</option>"
          "<option value='7d'>Letzte 7 Tage</option>"
          "<option value='month'>Dieser Monat</option>"
          "</select></div>";

  html += "<div class='hint'>Werte auswählen und Zeitraum ändern – Diagramm lädt automatisch.</div>";

  // Checkboxen (IDs = metric keys)
  html += "<div class='pick-row'><label><input type='checkbox' class='m' value='temp' checked> Temperatur (°C)</label></div>";
  html += "<div class='pick-row'><label><input type='checkbox' class='m' value='hum'  checked> Luftfeuchte (%)</label></div>";
  html += "<div class='pick-row'><label><input type='checkbox' class='m' value='press' checked> Luftdruck (hPa)</label></div>";
  html += "<div class='pick-row'><label><input type='checkbox' class='m' value='co2'  checked> CO₂ (ppm)</label></div>";

  html += "<div class='hint warn logger-err' id='err' hidden></div>";
  html += "<canvas id='c' width='900' height='320' class='logger-canvas'></canvas>";

  html += "</div>"; // card

  // Minimaler Canvas-Renderer (Linie + Achsen + Legend)
  html += R"HTML(
<script>
(function(){
  const canvas = document.getElementById('c');
  const ctx = canvas.getContext('2d');
  const err = document.getElementById('err');

  function selMetrics(){
    return Array.from(document.querySelectorAll('.m:checked')).map(x => x.value);
  }

  function qs(obj){
    return Object.keys(obj).map(k => k + '=' + encodeURIComponent(obj[k])).join('&');
  }

  function showErr(msg){
    err.hidden = false;
    err.textContent = msg;
  }
  function hideErr(){
    err.hidden = true;
    err.textContent = '';
  }

  function clear(){
    ctx.clearRect(0,0,canvas.width,canvas.height);
  }

  // einfache Farben
  const colors = {
    temp:  '#1f77b4',
    hum:   '#2ca02c',
    press: '#ff7f0e',
    co2:   '#d62728'
  };

  function drawChart(payload){
    clear();
    const W = canvas.width, H = canvas.height;
    const padL=48, padR=16, padT=14, padB=34;
    const plotW = W - padL - padR;
    const plotH = H - padT - padB;

    const labels = payload.labels || [];
    const series = payload.series || {};

    // min/max über alle gewählten Serien
    let ymin = Infinity, ymax = -Infinity;
    let any = false;

    Object.keys(series).forEach(k=>{
      (series[k]||[]).forEach(v=>{
        if (v === null || Number.isNaN(v)) return;
        any = true;
        ymin = Math.min(ymin, v);
        ymax = Math.max(ymax, v);
      });
    });

    if (!any || labels.length < 2){
      ctx.fillText('Keine Daten', 20, 30);
      return;
    }

    if (ymin === ymax){ ymin -= 1; ymax += 1; }

    const xAt = (i)=> padL + (i/(labels.length-1))*plotW;
    const yAt = (v)=> padT + (1 - (v - ymin)/(ymax - ymin))*plotH;

    // Achsen
    ctx.strokeStyle = '#cfd3d7';
    ctx.beginPath();
    ctx.moveTo(padL, padT);
    ctx.lineTo(padL, padT+plotH);
    ctx.lineTo(padL+plotW, padT+plotH);
    ctx.stroke();

    // y labels (3 ticks)
    ctx.fillStyle = '#333';
    ctx.font = '12px system-ui, sans-serif';
    for(let t=0;t<=2;t++){
      const v = ymin + (t/2)*(ymax-ymin);
      const y = yAt(v);
      ctx.fillText(v.toFixed(1), 6, y+4);
      ctx.strokeStyle = '#eee';
      ctx.beginPath();
      ctx.moveTo(padL, y);
      ctx.lineTo(padL+plotW, y);
      ctx.stroke();
    }

    // x label start/end
    ctx.fillStyle = '#333';
    const l0 = labels[0] || '';
    const l1 = labels[labels.length-1] || '';
    ctx.fillText(l0, padL, H-10);
    const tw = ctx.measureText(l1).width;
    ctx.fillText(l1, W-padR-tw, H-10);

    // Serien zeichnen
    Object.keys(series).forEach(key=>{
      const arr = series[key] || [];
      ctx.strokeStyle = colors[key] || '#000';
      ctx.lineWidth = 2;
      ctx.beginPath();
      let started=false;
      for(let i=0;i<arr.length;i++){
        const v = arr[i];
        if (v === null || Number.isNaN(v)) continue;
        const x = xAt(i);
        const y = yAt(v);
        if (!started){ ctx.moveTo(x,y); started=true; }
        else ctx.lineTo(x,y);
      }
      ctx.stroke();
    });

    // Legende
    let lx = padL, ly = padT+12;
    Object.keys(series).forEach(key=>{
      ctx.fillStyle = colors[key] || '#000';
      ctx.fillRect(lx, ly-8, 10, 3);
      ctx.fillStyle = '#333';
      ctx.fillText(key, lx+14, ly-2);
      lx += 70;
    });
  }

  async function load(){
    hideErr();
    const range = document.getElementById('range').value;
    const metrics = selMetrics();
    if (metrics.length === 0){
      showErr('Bitte mindestens einen Wert auswählen.');
      clear();
      return;
    }
    try{
      const r = await fetch('/api/history?' + qs({range, metrics: metrics.join(',')}));
      if (!r.ok) throw new Error('HTTP ' + r.status);
      const data = await r.json();
      drawChart(data);
    }catch(e){
      showErr('Konnte Daten nicht laden: ' + e.message);
      clear();
    }
  }

  document.getElementById('range').addEventListener('change', load);
  document.querySelectorAll('.m').forEach(x => x.addEventListener('change', load));
  load();
})();
</script>
)HTML";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
