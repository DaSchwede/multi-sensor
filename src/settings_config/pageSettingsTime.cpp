#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <math.h>
#include "pages.h"
#include "ntp_time.h"
#include "html_util.h"
#include "settings_config/settings_common.h"

void pageSettingsTime(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String msg = "";

  if (server.method() == HTTP_POST) {
    if (server.hasArg("ntp_server")) {
      String srv = server.arg("ntp_server");
      srv.trim();
      cfg->ntp_server = srv.length() ? srv : String("pool.ntp.org");
    }

    cfg->tz_auto_berlin = (server.arg("tz_mode") != "offset");

    // Stunden (auch halbe, z.B. Indien +5,5)
    if (server.hasArg("tz_base_hours")) {
      String h = server.arg("tz_base_hours");
      h.replace(",", ".");
      float v = h.toFloat();
      if (v < -12.0f) v = -12.0f;
      if (v > 14.0f)  v = 14.0f;
      cfg->tz_base_seconds = (int)lroundf(v * 2.0f) * 1800;   // auf 30 min runden
    }
    if (server.hasArg("dst_add_hours")) {
      int d = toIntSafe(server.arg("dst_add_hours"), cfg->dst_add_seconds / 3600);
      if (d < 0) d = 0;
      if (d > 2) d = 2;
      cfg->dst_add_seconds = d * 3600;
    }

    saveConfig(*cfg);
    if (WiFi.status() == WL_CONNECTED) ntpBegin(*cfg);   // Server/Zeitzone sofort übernehmen
    else ntpApplyTimezone(*cfg);
    msg = "Gespeichert und übernommen.";
  }

  const bool autoDst = cfg->tz_auto_berlin;
  const float baseH = cfg->tz_base_seconds / 3600.0f;
  const int dstH = cfg->dst_add_seconds / 3600;
  String baseStr = String(baseH, (cfg->tz_base_seconds % 3600) ? 1 : 0);

  String html = pagesHeaderAuth("Einstellungen – Zeit", "/settings/time");
  html += pagesTitle("Zeit / NTP", "Zeitserver und Zeitzone für Anzeige, Logger und Diagramme.");
  settingsSendOkBadge(html, msg);

  html += "<form method='POST'>";

  html += "<div class='card'><h2>NTP-Konfiguration</h2>";
  html += "<div class='form-row'><label for='ntp_server'>Zeitserver</label>"
          "<input id='ntp_server' name='ntp_server' maxlength='60' value='" + htmlEscape(cfg->ntp_server) + "' "
          "placeholder='pool.ntp.org' autocapitalize='none' autocorrect='off' spellcheck='false'>"
          "<div class='small'>Tipp: regional z.B. <b>de.pool.ntp.org</b>, die Fritz!Box (<b>fritz.box</b>) oder <b>ptbtime1.ptb.de</b>.</div></div>";

  html += "<div class='form-row'><label for='tz_mode'>Zeitzone</label>"
          "<select id='tz_mode' name='tz_mode'>"
          "<option value='auto'" + String(autoDst ? " selected" : "") + ">Automatische Sommerzeit (EU-Regel, z.B. Europe/Berlin)</option>"
          "<option value='offset'" + String(autoDst ? "" : " selected") + ">Fester Offset (ohne Sommerzeitwechsel)</option>"
          "</select></div>";

  html += "<div class='time-grid'>";
  html += "<div class='form-row'><label for='tz_base_hours'>Abstand zu UTC (Stunden)</label>"
          "<input id='tz_base_hours' name='tz_base_hours' type='number' min='-12' max='14' step='0.5' value='" + baseStr + "'>"
          "<div class='small' id='hint_base'></div></div>";
  html += "<div class='form-row'><label for='dst_add_hours'>Sommerzeit-Zuschlag (Stunden)</label>"
          "<select id='dst_add_hours' name='dst_add_hours'>"
          "<option value='0'" + String(dstH == 0 ? " selected" : "") + ">0</option>"
          "<option value='1'" + String(dstH == 1 ? " selected" : "") + ">+1</option>"
          "<option value='2'" + String(dstH == 2 ? " selected" : "") + ">+2</option>"
          "</select><div class='small' id='hint_dst'></div></div>";
  html += "</div>";

  html += "<div class='form-row'><label>Verwendete Zeitzonen-Regel</label>"
          "<input type='text' readonly value='" + htmlEscape(ntpPosixTz(*cfg)) + "'></div>";
  html += "<div class='actions'><button class='btn-primary' type='submit'>Speichern</button></div>";
  html += "</div>";
  html += "</form>";

  html += "<div class='card'><h2>Aktuelle Zeit</h2><table class='tbl'>";
  html += "<tr><th>Status</th><td id='t_state'>…</td></tr>";
  html += "<tr><th>Lokale Uhrzeit</th><td id='t_local'>…</td></tr>";
  html += "<tr><th>Sommerzeit</th><td id='t_dst'>…</td></tr>";
  html += "<tr><th>Letzte Synchronisation</th><td id='t_last'>…</td></tr>";
  html += "</table>";
  html += "<div class='actions'><button id='btn_time_sync' class='btn-secondary' type='button'>Zeit jetzt synchronisieren</button></div>";
  html += "<div class='small mt-8'>Ohne WLAN/Internet ist keine Synchronisation möglich.</div>";
  html += "</div>";

  // Hinweise je nach Modus
  html += R"JS(
<script>
(function(){
  const mode = document.getElementById('tz_mode');
  const hb = document.getElementById('hint_base'), hd = document.getElementById('hint_dst');
  function apply(){
    const auto = mode.value !== 'offset';
    hb.textContent = auto ? 'Winterzeit-Abstand. Deutschland: 1' : 'Fester Abstand zu UTC.';
    hd.textContent = auto ? 'Wird zwischen letztem Sonntag im März und Oktober addiert. Deutschland: +1'
                          : 'Wird dauerhaft addiert – meist 0.';
  }
  mode.addEventListener('change', apply);
  apply();
})();
</script>
)JS";
  html += pagesTimeLiveScript();

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
