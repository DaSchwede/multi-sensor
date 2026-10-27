#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>

#include "pages.h"
#include "ntp_time.h"
#include "html_util.h"
#include "settings_config/settings_common.h"

// Gemeinsames Status-JSON für Info → Zeit / NTP und Einstellungen → Zeit
static String fmtLocalTime(time_t t) {
  struct tm lt {};
  localtime_r(&t, &lt);
  char buf[32];
  strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", &lt);
  return String(buf);
}

void apiTimeStatus(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  const bool valid = ntpIsValid();
  const time_t now = time(nullptr);
  const unsigned long last = ntpLastSyncUtc();

  String j = "{";
  j += "\"valid\":" + String(valid ? "true" : "false");
  j += ",\"local\":\"" + (valid ? fmtLocalTime(now) : String("—")) + "\"";
  j += ",\"utc\":" + String(valid ? (unsigned long)now : 0UL);
  j += ",\"dst\":" + String(valid && ntpDstActive() ? "true" : "false");
  j += ",\"server\":\"" + jsonEscape(String(ntpServer())) + "\"";
  j += ",\"last_sync\":\"" + (last ? fmtLocalTime((time_t)last) : String("noch nie")) + "\"";
  j += ",\"last_sync_ago_s\":" + String((last && valid) ? (long)(now - (time_t)last) : -1L);
  j += "}";
  server.send(200, "application/json; charset=utf-8", j);
}

void apiTimeSync(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;
  const bool ok = ntpSyncNow();
  server.send(ok ? 200 : 503, "application/json; charset=utf-8",
              ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"kein WLAN oder NTP nicht gestartet\"}");
}

// Live-Anzeige + Sync-Knopf (von beiden Zeitseiten genutzt)
String pagesTimeLiveScript() {
  return R"JS(
<script>
(function(){
  function ago(s){ if(s<0) return ''; if(s<90) return ' (vor '+s+' s)'; if(s<5400) return ' (vor '+Math.round(s/60)+' min)'; return ' (vor '+Math.round(s/3600)+' h)'; }
  async function refresh(){
    try{
      const r = await fetch('/api/time/status', {cache:'no-store'});
      if(!r.ok) return;
      const j = await r.json();
      const set = (id, v) => { const e = document.getElementById(id); if(e) e.textContent = v; };
      set('t_local', j.local);
      set('t_state', j.valid ? 'synchronisiert' : 'nicht synchronisiert');
      set('t_dst', j.valid ? (j.dst ? 'Ja (Sommerzeit)' : 'Nein (Winterzeit)') : '—');
      set('t_last', j.last_sync + ago(j.last_sync_ago_s));
      set('t_utc', j.utc ? String(j.utc) : '—');
    }catch(e){}
  }
  const btn = document.getElementById('btn_time_sync');
  if(btn) btn.addEventListener('click', async () => {
    btn.disabled = true; const old = btn.textContent; btn.textContent = 'Synchronisiere…';
    try{ await fetch('/api/time/sync', {method:'POST'}); }catch(e){}
    setTimeout(async () => { await refresh(); btn.textContent = old; btn.disabled = false; }, 2500);
  });
  refresh();
  setInterval(refresh, 5000);
})();
</script>
)JS";
}

void pageNTP(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String html = pagesHeaderAuth("Info – Zeit / NTP", "/info/time");
  html += pagesTitle("Zeit / NTP", "Zeitserver, Zeitzone und Synchronisation.",
                     "<button class='icon-btn' type='button' title='Aktualisieren' onclick='location.reload()'>&#x21bb;</button>");

  html += "<div class='card'><h2>Konfiguration</h2><table class='tbl'>";
  html += "<tr><th>Zeitserver</th><td>" + htmlEscape(cfg->ntp_server) + "</td></tr>";
  html += "<tr><th>Zeitzone</th><td>" +
          String(cfg->tz_auto_berlin ? "Automatische Sommerzeit (EU-Regel)" : "Fester Offset") +
          "</td></tr>";
  html += "<tr><th>TZ-Regel</th><td><code>" + htmlEscape(ntpPosixTz(*cfg)) + "</code></td></tr>";
  html += "</table>";
  html += "<div class='actions'><a class='btn btn-secondary' href='/settings/time'>Einstellungen ändern</a></div>";
  html += "</div>";

  html += "<div class='card'><h2>Aktuelle Zeit</h2><table class='tbl'>";
  html += "<tr><th>Status</th><td id='t_state'>…</td></tr>";
  html += "<tr><th>Lokale Uhrzeit</th><td id='t_local'>…</td></tr>";
  html += "<tr><th>Sommerzeit</th><td id='t_dst'>…</td></tr>";
  html += "<tr><th>Letzte Synchronisation</th><td id='t_last'>…</td></tr>";
  html += "<tr><th>UTC (Unix-Zeit)</th><td id='t_utc'>…</td></tr>";
  html += "</table>";
  html += "<div class='actions'><button id='btn_time_sync' class='btn-primary' type='button'>Jetzt synchronisieren</button></div>";
  html += "<div class='small mt-8'>Die Zeit wird automatisch etwa stündlich mit dem Server abgeglichen. Ohne WLAN/Internet ist keine Synchronisation möglich.</div>";
  html += "</div>";

  html += pagesTimeLiveScript();
  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
