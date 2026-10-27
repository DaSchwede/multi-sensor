#pragma once
#include <Arduino.h>

// Ergänzende Styles, die von der Firmware selbst unter /ui.css ausgeliefert werden.
// Vorteil: Änderungen kommen mit dem normalen Firmware-Update (OTA),
// ohne das LittleFS-Image neu hochzuladen (das würde config.json überschreiben).
static const char UI_EXTRA_CSS[] PROGMEM = R"CSS(
.hint{background:#f9fafb;border:1px solid var(--border);border-radius:10px;padding:10px 12px;color:var(--muted);font-size:13px;line-height:1.45;margin:12px 0 0;}
.hint.warn{background:#fffbeb;border-color:#fcd34d;color:#92400e;}
.hint.danger{background:#fef2f2;border-color:#fecaca;color:#991b1b;}
.hint b{color:inherit;}
a.btn,.btn{display:inline-block;text-decoration:none;text-align:center;}
.btn-danger{background:#dc2626;color:#fff;}
.btn-danger:hover{background:#b91c1c;}
.page-title{margin:10px 0 2px;font-size:26px;font-weight:800;}
.page-head{display:flex;align-items:flex-end;justify-content:space-between;gap:12px;margin:4px 0 4px;}
.page-sub{margin:0 0 6px;color:var(--muted);font-size:14px;}
.sec{padding:16px;}
.sec-head{margin:0 0 12px;font-size:18px;font-weight:700;color:var(--text);}
.sec-head.danger{color:#b91c1c;}
.sec-body{padding:0;}
.tool-row{display:flex;gap:12px;align-items:center;flex-wrap:wrap;}
.tool-row .grow{flex:1 1 240px;}
.tool-row .lbl{flex:0 0 220px;color:var(--text);font-weight:600;font-size:14px;}
.tool-row > button,.tool-row > .btn{flex:0 0 auto;min-width:150px;}
input[type="file"]{width:100%;padding:8px;border:1px solid var(--border);border-radius:10px;background:#fff;font-size:14px;}
.pw-grid{display:grid;grid-template-columns:220px 1fr;gap:10px 12px;align-items:center;}
.pw-grid label{color:var(--text);font-weight:600;font-size:14px;}
.right-actions{display:flex;justify-content:flex-end;gap:10px;margin-top:14px;flex-wrap:wrap;}
/* Verlauf / Diagramme */
.chart-controls{display:grid;grid-template-columns:1fr 1fr;gap:0 14px;}
select{width:100%;padding:10px 12px;border:1px solid var(--border);border-radius:10px;background:#fff;font-size:15px;}
.metric-picks{display:flex;flex-wrap:wrap;gap:8px 18px;margin-top:6px;}
.metric-picks label{display:flex;align-items:center;gap:6px;color:var(--text);font-weight:600;font-size:14px;cursor:pointer;}
.chart-box{padding:10px 0 14px;border-bottom:1px solid var(--border);}
.chart-box:last-child{border-bottom:none;padding-bottom:0;}
.chart-head{display:flex;flex-wrap:wrap;align-items:baseline;gap:4px 14px;margin-bottom:6px;}
.chart-name{display:flex;align-items:center;gap:6px;font-weight:700;}
.chart-now{font-size:18px;font-weight:800;}
.chart-stats{color:var(--muted);font-size:12px;margin-left:auto;}
.chart-canvas-wrap{position:relative;}
.chart-canvas{display:block;width:100%;height:170px;touch-action:pan-y;cursor:crosshair;}
.chart-tip{position:absolute;top:0;background:#111827;color:#fff;font-size:12px;font-weight:600;padding:4px 8px;border-radius:6px;pointer-events:none;white-space:nowrap;}
.files-scroll{max-height:320px;overflow:auto;}
/* Untermenüs: per Klick (Touch) öffnen, Hover nur mit Maus */
.navdrop.open .navdrop-menu{display:block;}
@media (hover:none){ .navdrop:hover .navdrop-menu{display:none;} .navdrop.open .navdrop-menu{display:block;} }
/* Changelog (Über) */
.changelog h3{margin:16px 0 6px;font-size:15px;}
.changelog h3:first-child{margin-top:0;}
.changelog ul{margin:0 0 10px 18px;padding:0;}
.changelog li{margin:4px 0;font-size:14px;}
/* Zeit-Einstellungen */
.time-grid{display:grid;grid-template-columns:1fr 1fr;gap:0 14px;}
@media (max-width:640px){ .time-grid{grid-template-columns:1fr;} }
/* Menü: am Handy hinter einem Menü-Knopf */
.nav-toggle{display:none;}
@media (max-width:700px){
  .menubar.has-toggle{flex-wrap:wrap;gap:2px;}
  .nav-toggle{display:flex;align-items:center;gap:8px;background:transparent;color:#fff;padding:8px 12px;border-radius:999px;font-size:15px;font-weight:700;}
  .nav-toggle:hover{background:rgba(255,255,255,.12);}
  .menubar.has-toggle > a,.menubar.has-toggle > .navdrop,.menubar.has-toggle > .right{display:none;}
  .menubar.has-toggle.open > a,.menubar.has-toggle.open > .navdrop,.menubar.has-toggle.open > .right{display:block;width:100%;}
  .menubar.has-toggle .right{margin-left:0;}
  .menubar.has-toggle .navdrop-btn{display:block;}
  .menubar.has-toggle .navdrop .caret{display:none;}
  .menubar.has-toggle .navdrop-menu{display:block;position:static;border:none;background:transparent;padding:0 0 4px 16px;min-width:0;}
  .menubar.has-toggle .navdrop-menu a{padding:8px 12px;margin:0;font-weight:500;}
}
@media (max-width:640px){
  .chart-controls{grid-template-columns:1fr;}
  .chart-stats{margin-left:0;flex-basis:100%;}
  .tool-row .lbl{flex-basis:100%;}
  .pw-grid{grid-template-columns:1fr;}
  .tool-row > button,.tool-row > .btn{width:100%;}
}
/* ===== Design 1.5: ruhiger Rahmen, Raumluft als Blickfang ===== */
:root{
  --bg:#edf0ed;--card:#fff;--text:#1d2420;--muted:#5f6a63;--border:#dbe1dc;
  --brand:#32cd32;--brand-ink:#1f8a2a;--dark:#262b28;
  --good:#2f9e48;--mid:#d99a1c;--bad:#d2453a;
  --shadow:0 1px 2px rgba(22,40,28,.06);--radius:12px;
  --head:Bahnschrift,"DIN Alternate","DIN Next","Segoe UI Variable Display","Segoe UI",system-ui,sans-serif;
}
body{background:var(--bg);color:var(--text);font-family:"Segoe UI Variable Text","Segoe UI",system-ui,-apple-system,Roboto,Arial,sans-serif;font-size:15px;line-height:1.45;-webkit-font-smoothing:antialiased;}
.topbar{height:58px;padding:0 20px;background:var(--brand);}
.topbar .logo{height:42px;min-height:0;max-height:none;}
.menubar{background:var(--dark);gap:2px;padding:0 12px;align-items:stretch;}
.menubar a,.menubar .navdrop-btn{font-family:var(--head);font-weight:500;font-size:15px;letter-spacing:.01em;border-radius:0;padding:13px 14px 11px;opacity:.82;border-bottom:3px solid transparent;}
.menubar a:hover,.menubar .navdrop-btn:hover{background:rgba(255,255,255,.06);opacity:1;}
.menubar > a.active,.navdrop.active > .navdrop-btn{background:transparent;opacity:1;border-bottom-color:var(--brand);}
.menubar .right{display:flex;align-items:stretch;}
.navdrop{display:flex;align-items:stretch;}
.navdrop-menu{background:var(--dark);border:none;border-radius:0 0 10px 10px;padding:6px;box-shadow:0 10px 24px rgba(0,0,0,.18);}
.navdrop-menu a{border:none;border-radius:8px;padding:10px 12px;margin:1px 0;font-family:inherit;font-size:14px;}
.navdrop-menu a.active{background:transparent;color:var(--brand);font-weight:600;}
.content{max-width:1080px;padding:26px 18px 48px;}
.page-title{font-family:var(--head);font-weight:600;font-size:30px;letter-spacing:-.01em;margin:6px 0 2px;}
.page-sub{font-size:14px;}
.card{border:1px solid var(--border);border-radius:var(--radius);box-shadow:var(--shadow);padding:18px 20px;margin:16px 0;}
.card h2{font-family:var(--head);font-weight:600;font-size:17px;letter-spacing:.01em;margin:0 0 10px;}
.columns{gap:16px;}
.col{gap:0;}
.col > .card:first-child{margin-top:0;}
.columns .card{margin:0 0 16px;}
.tbl{border:none;border-radius:0;}
.tbl th,.tbl td{padding:9px 0;border-bottom:1px solid var(--border);}
.tbl th{background:transparent;color:var(--muted);font-weight:500;}
.tbl th + td:last-child{text-align:right;font-weight:600;font-variant-numeric:tabular-nums;padding-left:12px;overflow-wrap:anywhere;}
.badge{font-weight:600;font-size:12px;padding:3px 10px;}
.btn-primary{background:var(--brand-ink);}
.btn-primary:hover{background:#18731f;}
.btn-secondary{background:#e6ebe7;color:var(--text);}
button,.btn{font-weight:600;border-radius:10px;}
input:not([type="checkbox"]):not([type="radio"]):not([type="file"]),select,textarea{border-color:#cfd6d0;border-radius:8px;}
input:focus,select:focus,textarea:focus{outline:2px solid var(--brand);outline-offset:1px;border-color:var(--brand);}
a{color:var(--brand-ink);}
.menubar a,.navdrop-menu a{color:#fff;}
:focus-visible{outline:2px solid var(--brand);outline-offset:2px;}
/* Raumluft-Karte */
.air{padding:22px 24px 16px;}
.air-top{display:flex;justify-content:space-between;align-items:flex-start;gap:16px;}
.air-title{font-size:15px !important;color:var(--muted);font-weight:500 !important;margin:0 0 2px !important;}
.air-co2{display:flex;align-items:baseline;gap:10px;font-family:var(--head);}
.air-num{font-size:68px;font-weight:600;line-height:1;letter-spacing:-.02em;font-variant-numeric:tabular-nums;}
.air-unit{font-size:18px;color:var(--muted);white-space:nowrap;}
.air-verdict{margin:8px 0 0;font-family:var(--head);font-size:22px;font-weight:600;color:var(--muted);}
.air[data-level=good] .air-verdict{color:var(--good);}
.air[data-level=mid] .air-verdict{color:#b57a07;}
.air[data-level=bad] .air-verdict{color:var(--bad);}
.air-status{display:flex;align-items:center;gap:8px;flex-shrink:0;}
.air-status .badge{white-space:nowrap;}
.air-scale{margin:20px 0 6px;}
.air-bar{position:relative;display:flex;height:10px;border-radius:5px;}
.air-bar .seg{display:block;height:100%;}
.air-bar .good{flex:0 0 37.5%;background:var(--good);border-radius:5px 0 0 5px;}
.air-bar .mid{flex:0 0 25%;background:var(--mid);}
.air-bar .bad{flex:1;background:var(--bad);border-radius:0 5px 5px 0;}
.air[data-level=none] .air-bar .seg{opacity:.35;}
.air-mark{position:absolute;top:50%;left:0;width:22px;height:22px;margin:-11px 0 0 -11px;border-radius:50%;background:#fff;border:4px solid var(--dark);box-shadow:0 1px 4px rgba(0,0,0,.25);transition:left .6s ease;}
.air-ticks{position:relative;height:18px;margin-top:8px;font-size:12px;color:var(--muted);font-variant-numeric:tabular-nums;}
.air-ticks span{position:absolute;transform:translateX(-50%);}
.air-ticks span:first-child{transform:none;}
.air-ticks span:last-child{transform:translateX(-100%);}
.air-climate{display:grid;grid-template-columns:repeat(3,1fr);margin-top:16px;border-top:1px solid var(--border);}
.clim{display:flex;flex-direction:column;padding:14px 16px 4px;border-left:1px solid var(--border);}
.clim:first-child{border-left:none;padding-left:0;}
.clim-lbl{font-size:13px;color:var(--muted);}
.clim-val{font-family:var(--head);font-size:18px;color:var(--muted);font-variant-numeric:tabular-nums;}
.clim-val b{font-size:30px;font-weight:600;color:var(--text);}
.clim-note{font-size:13px;color:var(--muted);min-height:1.2em;}
.air-foot{margin:12px 0 0;}
@media (prefers-reduced-motion:reduce){.air-mark{transition:none;}.icon-btn.spin{animation:none;}}
@media (max-width:700px){
  .topbar{height:52px;padding:0 14px;}
  .topbar .logo{height:36px;}
  .menubar{padding:0 6px;}
  .menubar.has-toggle .nav-toggle{border-radius:8px;margin:6px 0;}
  .menubar.has-toggle.open > a,.menubar.has-toggle.open > .navdrop{display:block;}
  .menubar.has-toggle.open > .right{display:block;}
  .menubar.has-toggle a,.menubar.has-toggle .navdrop-btn{border-bottom:none;border-left:3px solid transparent;padding:10px 12px;}
  .menubar.has-toggle > a.active,.menubar.has-toggle .navdrop.active > .navdrop-btn{border-left-color:var(--brand);}
  .menubar.has-toggle .navdrop-menu{box-shadow:none;}
  .content{padding:18px 12px 40px;}
  .page-title{font-size:25px;}
  .card{padding:16px;}
  .air{padding:18px 16px 14px;}
  .air-num{font-size:56px;}
  .air-verdict{font-size:20px;}
  .air-climate{grid-template-columns:1fr;}
  .columns{gap:0;}
  .tbl th{width:auto;white-space:nowrap;padding-right:12px;}
  .air-top{flex-wrap:wrap-reverse;}
  .clim{display:grid;grid-template-columns:1fr auto;grid-template-areas:"lbl val" "note val";align-items:center;border-left:none;border-top:1px solid var(--border);padding:10px 0;}
  .clim:first-child{border-top:none;}
  .clim-lbl{grid-area:lbl;font-size:14px;color:var(--text);}
  .clim-note{grid-area:note;}
  .clim-val{grid-area:val;}
  .clim-val b{font-size:26px;}
}
)CSS";
