#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include "pages.h"
#include "ntp_time.h"
#include "html_util.h"
#include "settings_config/settings_common.h"

void pageSettingsTime(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String msg = "";

  if (server.method() == HTTP_POST) {
    cfg->tz_auto_berlin = server.hasArg("tz_auto_berlin");
    if (server.hasArg("tz_base_seconds")) {
      int v = toIntSafe(server.arg("tz_base_seconds"), cfg->tz_base_seconds);
      if (v >= -12 * 3600 && v <= 14 * 3600) cfg->tz_base_seconds = v;
    }
    if (server.hasArg("dst_add_seconds")) {
      int v = toIntSafe(server.arg("dst_add_seconds"), cfg->dst_add_seconds);
      if (v >= 0 && v <= 2 * 3600) cfg->dst_add_seconds = v;
    }
    if (server.hasArg("ntp_server")) {
      String srv = server.arg("ntp_server");
      srv.trim();
      cfg->ntp_server = srv.length() ? srv : String("pool.ntp.org");
    }

    saveConfig(*cfg);

    // sofort übernehmen
    if (WiFi.status() == WL_CONNECTED) ntpBegin(*cfg);
    else ntpApplyTimezone(*cfg);

    msg = "Gespeichert und übernommen.";
  }

  String html = pagesHeaderAuth("Einstellungen – Zeit", "/settings/time");
  html += pagesTitle("Zeit / NTP", "Zeitserver und Zeitzone für Anzeige, Logger und Diagramme.");
  settingsSendOkBadge(html, msg);

  html += "<form method='POST'>";

  html += "<div class='card'><h2>Zeit</h2>";
  html += "<div class='form-row'><label>NTP-Server</label>"
          "<input name='ntp_server' maxlength='60' value='" + htmlEscape(cfg->ntp_server) + "'></div>";
  html += "<div class='form-row'>"
          "<label><input type='checkbox' name='tz_auto_berlin' " + String(cfg->tz_auto_berlin ? "checked" : "") +
          "> Automatische Sommerzeit (EU-Regel, Standard: Europe/Berlin)</label>"
          "</div>";
  html += "<div class='form-row'><label>Basis Offset (Sek.)</label>"
          "<input name='tz_base_seconds' type='number' step='900' value='" + String(cfg->tz_base_seconds) + "'></div>";
  html += "<div class='form-row'><label>DST Add (Sek.)</label>"
          "<input name='dst_add_seconds' type='number' step='900' min='0' max='7200' value='" + String(cfg->dst_add_seconds) + "'></div>";
  html += "<div class='hint'>Basis = Abstand zu UTC im Winter (Deutschland: 3600). "
          "DST Add wirkt nur bei automatischer Sommerzeit (Deutschland: 3600).</div>";
  html += "</div>";

  html += "<div class='card'><h2>Aktuell</h2><table class='tbl'>";
  html += "<tr><th>Zeit gültig</th><td>" + String(ntpIsValid() ? "Ja" : "Nein (noch keine NTP-Antwort)") + "</td></tr>";
  html += "<tr><th>Lokale Zeit</th><td>" + ntpDateTimeString(*cfg) + "</td></tr>";
  html += "<tr><th>Sommerzeit aktiv</th><td>" + String(ntpDstActive() ? "Ja" : "Nein") + "</td></tr>";
  html += "<tr><th>TZ-Regel</th><td><code>" + htmlEscape(ntpPosixTz(*cfg)) + "</code></td></tr>";
  html += "</table></div>";

  html += "<div class='card'><div class='actions'>"
          "<button class='btn-primary' type='submit'>Speichern</button>"
          "</div></div>";

  html += "</form>";
  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
