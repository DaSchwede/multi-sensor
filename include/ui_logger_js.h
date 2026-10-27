#pragma once
#include <Arduino.h>

// Verlaufsdiagramm (Info → Logger). Wird von der Firmware unter /ui-logger.js ausgeliefert,
// damit Änderungen per OTA kommen (ohne LittleFS-Upload). Ersetzt data/logger.js.
static const char UI_LOGGER_JS[] PROGMEM = R"JS(
(function(){
  const META = {
    temp:  { c:'#1f77b4', l:'Temperatur',  u:'°C',  d:1 },
    hum:   { c:'#2ca02c', l:'Luftfeuchte', u:'%',   d:0 },
    press: { c:'#ff7f0e', l:'Luftdruck',   u:'hPa', d:0 },
    co2:   { c:'#d62728', l:'CO₂',         u:'ppm', d:0 }
  };
  const PAD = { l:48, r:12, t:10, b:24 };
  const $ = id => document.getElementById(id);
  const wrap = $('charts'), err = $('err');
  let lastData = null;

  function selMetrics(){ return Array.from(document.querySelectorAll('.m:checked')).map(x => x.value); }
  function showErr(m){ err.hidden = false; err.textContent = m; }
  function hideErr(){ err.hidden = true; err.textContent = ''; }
  function fmt(v, d){
    if (v === null || v === undefined || Number.isNaN(v)) return '—';
    return Number(v).toLocaleString('de-DE', { minimumFractionDigits:d, maximumFractionDigits:d });
  }
  function esc(s){ return String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c])); }

  function stats(arr){
    let mn = Infinity, mx = -Infinity, s = 0, n = 0, last = null;
    for (const v of arr){ if (v === null || v === undefined) continue; mn = Math.min(mn, v); mx = Math.max(mx, v); s += v; n++; last = v; }
    return n ? { mn, mx, avg: s / n, last, n } : null;
  }
  function yRange(st){
    let mn = st.mn, mx = st.mx;
    if (mn === mx){ const p = Math.max(Math.abs(mn) * 0.01, 1); mn -= p; mx += p; }
    const pad = (mx - mn) * 0.1;
    return [mn - pad, mx + pad];
  }

  function setupCanvas(cv){
    const dpr = window.devicePixelRatio || 1;
    const w = cv.clientWidth, h = cv.clientHeight;
    if (cv.width !== Math.round(w * dpr) || cv.height !== Math.round(h * dpr)){
      cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr);
    }
    const ctx = cv.getContext('2d');
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    return { ctx, w, h };
  }
  function xAt(i, n, mode, pw){
    if (mode === 'bar'){ const gw = pw / n; return PAD.l + gw * i + gw / 2; }
    return PAD.l + (n <= 1 ? pw / 2 : pw * i / (n - 1));
  }
  function indexAt(x, n, mode, pw){
    let i = mode === 'bar' ? Math.floor((x - PAD.l) / (pw / n))
                           : Math.round((x - PAD.l) / pw * (n - 1));
    return Math.max(0, Math.min(n - 1, i));
  }

  function draw(cv, labels, arr, m, mode, st, hi){
    const { ctx, w, h } = setupCanvas(cv);
    ctx.clearRect(0, 0, w, h);
    const pw = w - PAD.l - PAD.r, ph = h - PAD.t - PAD.b, n = labels.length;
    ctx.font = '11px system-ui, -apple-system, Segoe UI, Roboto, sans-serif';
    if (!st){ ctx.fillStyle = '#6b7280'; ctx.fillText('Keine Werte im Zeitraum', PAD.l, PAD.t + 20); return; }

    const [y0, y1] = yRange(st);
    const yAt = v => PAD.t + (1 - (v - y0) / (y1 - y0)) * ph;

    // Raster + Y-Beschriftung
    ctx.lineWidth = 1;
    for (let t = 0; t <= 3; t++){
      const v = y0 + (y1 - y0) * t / 3, y = Math.round(yAt(v)) + 0.5;
      ctx.strokeStyle = '#eef0f3'; ctx.beginPath(); ctx.moveTo(PAD.l, y); ctx.lineTo(w - PAD.r, y); ctx.stroke();
      ctx.fillStyle = '#6b7280'; ctx.textAlign = 'right';
      ctx.textBaseline = t === 0 ? 'bottom' : (t === 3 ? 'top' : 'middle');   // nicht mit X-Achse kollidieren
      ctx.fillText(fmt(v, m.d), PAD.l - 6, y);
    }
    // X-Beschriftung (so viele wie Platz ist)
    const maxL = Math.max(2, Math.floor(pw / 64)), step = Math.max(1, Math.ceil(n / maxL));
    ctx.textAlign = 'center'; ctx.textBaseline = 'top'; ctx.fillStyle = '#6b7280';
    for (let i = 0; i < n; i += step){
      ctx.textAlign = (i === 0 && mode !== 'bar') ? 'left' : 'center';
      ctx.fillText(labels[i], xAt(i, n, mode, pw), h - PAD.b + 6);
    }

    if (mode === 'bar'){
      const gw = pw / n, bw = Math.max(1, gw * 0.7), base = PAD.t + ph;
      for (let i = 0; i < n; i++){
        const v = arr[i]; if (v === null || v === undefined) continue;
        ctx.globalAlpha = (hi < 0 || hi === i) ? 1 : 0.5;
        ctx.fillStyle = m.c;
        const y = yAt(v);
        ctx.fillRect(xAt(i, n, mode, pw) - bw / 2, y, bw, base - y);
      }
      ctx.globalAlpha = 1;
    } else {
      ctx.strokeStyle = m.c; ctx.lineWidth = 2; ctx.lineJoin = 'round'; ctx.lineCap = 'round';
      ctx.beginPath();
      let pen = false;
      for (let i = 0; i < n; i++){
        const v = arr[i];
        if (v === null || v === undefined){ pen = false; continue; }   // Lücke statt Verbindungslinie
        const x = xAt(i, n, mode, pw), y = yAt(v);
        if (pen) ctx.lineTo(x, y); else ctx.moveTo(x, y);
        pen = true;
      }
      ctx.stroke();
      if (n <= 72){   // bei wenigen Punkten die Messpunkte zeigen
        ctx.fillStyle = m.c;
        for (let i = 0; i < n; i++){
          const v = arr[i]; if (v === null || v === undefined) continue;
          ctx.beginPath(); ctx.arc(xAt(i, n, mode, pw), yAt(v), 2.2, 0, Math.PI * 2); ctx.fill();
        }
      }
    }

    // Markierung beim Drüberfahren
    if (hi >= 0 && hi < n){
      const x = Math.round(xAt(hi, n, mode, pw)) + 0.5;
      ctx.strokeStyle = '#9ca3af'; ctx.lineWidth = 1; ctx.setLineDash([3, 3]);
      ctx.beginPath(); ctx.moveTo(x, PAD.t); ctx.lineTo(x, PAD.t + ph); ctx.stroke(); ctx.setLineDash([]);
      const v = arr[hi];
      if (mode !== 'bar' && v !== null && v !== undefined){
        ctx.fillStyle = '#fff'; ctx.strokeStyle = m.c; ctx.lineWidth = 2;
        ctx.beginPath(); ctx.arc(x, yAt(v), 4, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
      }
    }
  }

  function render(data){
    lastData = data;
    wrap.innerHTML = '';
    const labels = data.labels || [], series = data.series || {}, mode = data.mode || 'line';
    if (!labels.length){
      wrap.innerHTML = "<div class='hint'>Keine Daten im gewählten Zeitraum.</div>";
      return;
    }
    Object.keys(series).forEach(key => {
      const m = META[key]; if (!m) return;
      const arr = series[key] || [], st = stats(arr);
      const box = document.createElement('div');
      box.className = 'chart-box';
      box.innerHTML =
        "<div class='chart-head'>" +
          "<span class='chart-name'><span class='swatch' style='background:" + m.c + "'></span>" + m.l + "</span>" +
          "<span class='chart-now'>" + (st ? fmt(st.last, m.d) + ' ' + m.u : '—') + "</span>" +
          "<span class='chart-stats'>" + (st ? 'min ' + fmt(st.mn, m.d) + ' · Ø ' + fmt(st.avg, m.d) + ' · max ' + fmt(st.mx, m.d) + ' ' + m.u : 'keine Werte') + "</span>" +
        "</div>" +
        "<div class='chart-canvas-wrap'><canvas class='chart-canvas'></canvas><div class='chart-tip' hidden></div></div>";
      wrap.appendChild(box);

      const cv = box.querySelector('canvas'), tip = box.querySelector('.chart-tip');
      const redraw = hi => draw(cv, labels, arr, m, mode, st, hi);
      redraw(-1);

      const pick = ev => {
        const r = cv.getBoundingClientRect(), x = ev.clientX - r.left;
        const pw = r.width - PAD.l - PAD.r;
        if (x < PAD.l - 4 || x > r.width - PAD.r + 4){ redraw(-1); tip.hidden = true; return; }
        const i = indexAt(x, labels.length, mode, pw);
        redraw(i);
        tip.textContent = labels[i] + '  ' + fmt(arr[i], m.d) + ' ' + m.u;
        tip.hidden = false;
        const tx = xAt(i, labels.length, mode, pw);
        const tw = tip.offsetWidth;
        tip.style.left = Math.max(0, Math.min(r.width - tw, tx - tw / 2)) + 'px';
      };
      cv.addEventListener('pointermove', pick);
      cv.addEventListener('pointerdown', pick);
      cv.addEventListener('pointerleave', () => { redraw(-1); tip.hidden = true; });
    });
  }

  async function load(){
    hideErr();
    const metrics = selMetrics();
    if (!metrics.length){ wrap.innerHTML = ''; showErr('Bitte mindestens einen Wert auswählen.'); return; }
    const q = new URLSearchParams({ range: $('range').value, chart: $('chart').value, metrics: metrics.join(',') });
    try {
      const r = await fetch('/api/history?' + q.toString(), { cache:'no-store' });
      if (r.status === 503) throw new Error('Keine SD-Karte erkannt.');
      if (r.status === 409) throw new Error('Uhrzeit ist noch nicht synchronisiert (NTP).');
      if (!r.ok) throw new Error('HTTP ' + r.status);
      render(await r.json());
    } catch(e){
      wrap.innerHTML = '';
      showErr('Konnte Daten nicht laden: ' + e.message);
    }
  }

  function fmtSize(b){ return b < 1024 ? b + ' B' : (b / 1024).toLocaleString('de-DE', { maximumFractionDigits:1 }) + ' KB'; }
  function fmtDay(n){ const p = n.slice(0, 10).split('-'); return p[2] + '.' + p[1] + '.' + p[0]; }

  async function loadFiles(){
    const box = $('files');
    if (!box) return;
    try {
      const r = await fetch('/api/logs', { cache:'no-store' });
      if (r.status === 503){ box.innerHTML = "<div class='hint warn'>Keine SD-Karte erkannt.</div>"; return; }
      if (!r.ok) throw new Error('HTTP ' + r.status);
      const j = await r.json();
      const files = j.files || [];
      if (!files.length){ box.innerHTML = "<div class='hint'>Noch keine Log-Dateien vorhanden.</div>"; return; }
      let h = "<div class='files-scroll'><table class='tbl'><tr><th>Tag</th><th>Größe</th><th>Download</th></tr>";
      files.forEach(f => {
        const n = encodeURIComponent(f.name);
        h += '<tr><td>' + esc(fmtDay(f.name)) + '</td><td>' + fmtSize(f.size) + '</td><td>' +
             "<a href='/api/logs/download?f=" + n + "&fmt=excel'>Excel</a> · " +
             "<a href='/api/logs/download?f=" + n + "'>CSV</a></td></tr>";
      });
      h += '</table></div>';
      box.innerHTML = h;
    } catch(e){
      box.innerHTML = "<div class='hint danger'>Dateiliste konnte nicht geladen werden: " + esc(e.message) + "</div>";
    }
  }

  $('range').addEventListener('change', load);
  $('chart').addEventListener('change', load);
  document.querySelectorAll('.m').forEach(x => x.addEventListener('change', load));

  let rt = null;
  window.addEventListener('resize', () => { clearTimeout(rt); rt = setTimeout(() => { if (lastData) render(lastData); }, 150); });

  // alle 60 s aktualisieren, solange die Seite sichtbar ist
  setInterval(() => { if (!document.hidden) load(); }, 60000);

  load();
  loadFiles();
})();
)JS";
