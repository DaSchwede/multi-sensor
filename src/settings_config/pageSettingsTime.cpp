#include <Arduino.h>
#include <WebServer.h>
#include "pages.h"
#include "settings_config/settings_common.h"
#include "ntp_time.h"

static int clampInt(int v, int lo, int hi){
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

void pageSettingsTime(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String msg = "";

  // -------- POST: speichern --------
  if (server.method() == HTTP_POST) {
    // NTP Server
    if (server.hasArg("ntp_server")) {
      cfg->ntp_server = server.arg("ntp_server");
      cfg->ntp_server.trim();
      if (cfg->ntp_server.length() == 0) cfg->ntp_server = "pool.ntp.org";
    }

    // Modus (berlin / offset)
    const String mode = server.hasArg("tz_mode") ? server.arg("tz_mode") : "";

    if (mode == "berlin") {
      cfg->tz_auto_berlin = true;
      // (optional) deine bisherigen Werte so lassen – werden vom ntp_time ggf. ignoriert
    } else {
      cfg->tz_auto_berlin = false;

      // Freundlich: Stunden statt Sekunden
      int baseH = server.hasArg("tz_base_hours") ? toIntSafe(server.arg("tz_base_hours"), cfg->tz_base_seconds / 3600) : (cfg->tz_base_seconds / 3600);
      int dstH  = server.hasArg("dst_add_hours") ? toIntSafe(server.arg("dst_add_hours"),  cfg->dst_add_seconds / 3600) : (cfg->dst_add_seconds / 3600);

      baseH = clampInt(baseH, -12, 14);
      dstH  = clampInt(dstH, 0, 2);

      cfg->tz_base_seconds = baseH * 3600;
      cfg->dst_add_seconds = dstH  * 3600;
    }

    saveConfig(*cfg);

    // NTP direkt „anstupsen“ (neu konfigurieren/neu syncen)
    ntpBegin(*cfg);

    msg = "Gespeichert.";
  }

  // -------- UI --------
  const bool berlin = cfg->tz_auto_berlin;
  int baseH = (int)lround((double)cfg->tz_base_seconds / 3600.0);
  int dstH  = (int)lround((double)cfg->dst_add_seconds / 3600.0);
  baseH = clampInt(baseH, -12, 14);
  dstH  = clampInt(dstH, 0, 2);

  String html = pagesHeaderAuth("Einstellungen – Zeit / NTP", "/settings/time");
  settingsSendOkBadge(html, msg);

  html += "<form method='POST'>";

  // ===== NTP-Konfiguration (OpenDTU-like) =====
  html += "<div class='card'>";
  html += "  <div class='card-head'>NTP-Konfiguration</div>";

  html += "  <div class='form-grid'>";

  html += "    <div class='form-row'>"
          "      <label for='ntp_server'>Zeitserver</label>"
          "      <input id='ntp_server' name='ntp_server' type='text' "
          "             value='" + cfg->ntp_server + "' "
          "             placeholder='pool.ntp.org' autocapitalize='none' autocorrect='off' spellcheck='false'>"
          "      <div class='small'>Tipp: regional geht z.B. <b>de.pool.ntp.org</b> oder <b>ptbtime1.ptb.de</b>.</div>"
          "    </div>";

  html += "    <div class='form-row'>"
          "      <label for='tz_mode'>Zeitzone</label>"
          "      <select id='tz_mode' name='tz_mode'>"
          "        <option value='berlin' " + String(berlin ? "selected" : "") + ">Europe/Berlin (autom. Sommerzeit)</option>"
          "        <option value='offset' " + String(!berlin ? "selected" : "") + ">Fester Offset (UTC±…)</option>"
          "      </select>"
          "      <div class='small'>Wenn du keinen DST-Wechsel willst: „Fester Offset“ wählen.</div>"
          "    </div>";

  html += "  </div>"; // form-grid

  // Offset-Details (werden per JS deaktiviert wenn Berlin aktiv)
  html += "  <div id='tz_offset_box' class='mt-14'>";

  html += "    <div class='form-grid'>";

  html += "      <div class='form-row'>"
          "        <label for='tz_base_hours'>Basis Offset (Stunden)</label>"
          "        <input id='tz_base_hours' name='tz_base_hours' type='number' min='-12' max='14' step='1' value='" + String(baseH) + "'>"
          "        <div class='small'>Beispiel: Deutschland Winterzeit ≈ +1</div>"
          "      </div>";

  html += "      <div class='form-row'>"
          "        <label for='dst_add_hours'>Sommerzeit Zuschlag (Stunden)</label>"
          "        <select id='dst_add_hours' name='dst_add_hours'>"
          "          <option value='0' " + String(dstH == 0 ? "selected" : "") + ">0</option>"
          "          <option value='1' " + String(dstH == 1 ? "selected" : "") + ">+1</option>"
          "          <option value='2' " + String(dstH == 2 ? "selected" : "") + ">+2</option>"
          "        </select>"
          "        <div class='small'>Bei „Fester Offset“ meist 0.</div>"
          "      </div>";

  html += "    </div>"; // form-grid
  html += "  </div>";   // tz_offset_box

  // Readonly „Konfig“-Hinweis
  html += "  <div class='form-row mt-14'>"
          "    <label>Zeitzonen-Konfiguration</label>"
          "    <input type='text' readonly value='" + String(berlin ? "CET-1CEST,M3.5.0,M10.5.0/3" : ("UTC" + String(baseH >= 0 ? "+" : "") + String(baseH) + (dstH ? (", DST +" + String(dstH)) : ""))) + "'>"
          "    <div class='small'>Nur Info/Anzeige – intern nutzt die Firmware deine gespeicherten Werte.</div>"
          "  </div>";

  html += "</div>"; // card

  // ===== Manuelle Zeitsynchronisation =====
  html += "<div class='card'>";
  html += "  <div class='card-head'>Manuelle Zeitsynchronisation</div>";

  html += "  <div class='form-grid'>"
          "    <div class='form-row'>"
          "      <label>Aktuelle Gerätezeit</label>"
          "      <input id='time_local' type='text' readonly value='…'>"
          "    </div>"
          "    <div class='form-row'>"
          "      <label>NTP Status</label>"
          "      <input id='time_ntp' type='text' readonly value='…'>"
          "    </div>"
          "  </div>";

  html += "  <div class='actions'>"
          "    <button id='btn_time_sync' class='btn-secondary' type='button'>Zeit synchronisieren</button>"
          "    <button class='btn-primary' type='submit'>Speichern</button>"
          "  </div>";

  html += "  <div class='small mt-8'>Hinweis: Ohne Internet/WLAN kann keine NTP-Synchronisation erfolgen.</div>";
  html += "</div>"; // card

  html += "</form>";

  // JS Hook
  html += "<script>if(window.setupTimeUi){setupTimeUi();}</script>";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
