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
)CSS";
