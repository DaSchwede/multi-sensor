#include <Arduino.h>
#include <WebServer.h>
#include "pages.h"
#include "settings_config/settings_common.h"
#include "logger.h"
#include <math.h>
#include "log_bits.h"

// helpers wie bei UDP
static bool isAvailFloat(float v) { return !isnan(v); }

static String fmt2(float v, const char* unit) {
  if (isnan(v)) return String("—");
  return String(v, 2) + unit;
}
static String fmt0(float v, const char* unit) {
  if (isnan(v)) return String("—");
  return String((int)lroundf(v)) + unit;
}

static String badge(bool ok) {
  return ok ? "<span class='badge ok'>verfügbar</span>"
            : "<span class='badge warn'>keine Daten</span>";
}

static String fmtGB(uint64_t bytes) {
  double gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
  return String(gb, 2) + " GB";
}

void pageSettingsLogger(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  SensorData* live = pagesLive();   // <-- nach oben, damit du es im POST nutzen kannst
  String msg = "";

  if (server.method() == HTTP_POST) {
    cfg->log_enabled = server.hasArg("log_enabled");

    if (server.hasArg("log_interval_min")) {
      cfg->log_interval_min = (uint16_t)toIntSafe(server.arg("log_interval_min"),
                                                  (int)cfg->log_interval_min);
      if (cfg->log_interval_min < 1) cfg->log_interval_min = 30;
    }

    if (server.hasArg("log_retention_days")) {
      int v = toIntSafe(server.arg("log_retention_days"), (int)cfg->log_retention_days);
      if (v < 0) v = 0;
      cfg->log_retention_days = (uint16_t)v;
    }

    uint32_t mask = 0;
    if (server.hasArg("m_temp"))  mask |= LOG_TEMP;
    if (server.hasArg("m_hum"))   mask |= LOG_HUM;
    if (server.hasArg("m_press")) mask |= LOG_PRESS;
    if (server.hasArg("m_co2"))   mask |= LOG_CO2;
    if (server.hasArg("m_lux"))   mask |= LOG_LUX; // später
    cfg->log_metric_mask = mask;

    saveConfig(*cfg);
    msg = "Gespeichert.";

    // ✅ nur jetzt: sofort einen Eintrag schreiben (wenn möglich)
    if (cfg->log_enabled && live) {
      loggerForceOnce(*cfg, *live);
    }
  }

  const bool aTemp  = live ? isAvailFloat(live->temperature_c) : true;
  const bool aHum   = live ? isAvailFloat(live->humidity_rh)   : true;
  const bool aPress = live ? isAvailFloat(live->pressure_hpa)  : true;
  const bool aCo2   = live ? isAvailFloat(live->co2_ppm)       : true;

  // Lux: erst später verfügbar -> derzeit "keine Daten" (oder true, wenn du Feld schon hast)
  const bool aLux   = false;

  // SD Status aus Logger
  LoggerSdInfo sd = loggerGetSdInfo();
  uint16_t logDays = sd.ok ? loggerCountLogDays() : 0;
  int usedPct = (sd.ok && sd.total) ? (int)lround((double)sd.used * 100.0 / (double)sd.total) : 0;

  String html = pagesHeaderAuth("Einstellungen – Logger", "/settings/logger");
  settingsSendOkBadge(html, msg);

  html += "<form method='POST'>";

  // Card: Grundsätzlich
  html += "<div class='card'><h2>Logger</h2>";

  html += "<div class='form-row'><label>Datenspeicherung aktiv</label>"
          "<label class='switch'>"
          "<input type='checkbox' name='log_enabled' " + String(cfg->log_enabled ? "checked" : "") + ">"
          "<span class='slider'></span>"
          "</label></div>";

  // Intervall Dropdown
  html += "<div class='form-row'><label>Intervall</label><select name='log_interval_min'>";
  auto optMin = [&](int v, const char* txt){
    html += "<option value='" + String(v) + "' " + String((int)cfg->log_interval_min == v ? "selected" : "") + ">"
         + txt + "</option>";
  };
  optMin(1,   "1 Minute");  // nur zum Testen
  optMin(5,   "5 Minuten");  // nur zum Testen
  optMin(30,  "30 Minuten");
  optMin(60,  "60 Minuten");
  optMin(120, "2 Stunden");
  optMin(240, "4 Stunden");
  html += "</select></div>";

  // Retention Dropdown inkl. nie löschen
  html += "<div class='form-row'><label>Aufbewahrung</label><select name='log_retention_days'>";
  auto optRet = [&](int v, const char* txt){
    html += "<option value='" + String(v) + "' " + String((int)cfg->log_retention_days == v ? "selected" : "") + ">"
         + txt + "</option>";
  };
  optRet(0,   "Nie loeschen");
  optRet(1,   "1 Tag");
  optRet(7,   "7 Tage");
  optRet(14,  "14 Tage");
  optRet(30,  "30 Tage");
  optRet(90,  "90 Tage");
  optRet(365, "1 Jahr");
  html += "</select></div>";

  // SD Hinweis
  if (!sd.ok) {
    html += "<div class='hint warn'>Keine SD-Karte erkannt. Logging funktioniert nur mit SD.</div>";
  } else {
    html += "<div class='hint'>SD: Gesamt <b>" + fmtGB(sd.total) + "</b>, Belegt <b>" + fmtGB(sd.used) +
            "</b> (" + String(usedPct) + "%), Frei <b>" + fmtGB(sd.free) + "</b>, Log-Tage <b>" + String(logDays) + "</b></div>";
    html += "<form method='POST' action='/action/logger_rescan' style='margin-top:10px;'>"
        "<button class='btn-secondary' type='submit'>SD aktualisieren</button>"
        "</form>";
  }

  html += "</div>";

  // Card: Welche Werte speichern
  html += "<div class='card'><h2>Zu speichernde Werte</h2>";
  html += "<div class='hint'>Häkchen bestimmen, was gespeichert wird. Status basiert auf aktuellen Live-Daten.</div>";

  const uint32_t m = cfg->log_metric_mask;

  html += "<div class='pick-row'>"
          "<label><input type='checkbox' name='m_temp'  " + String((m & LOG_TEMP)  ? "checked" : "") + "> Temperatur (°C)</label>"
          + badge(aTemp) + "<span class='val'>" + (live ? fmt2(live->temperature_c, " °C") : "—") + "</span>"
          "</div>";

  html += "<div class='pick-row'>"
          "<label><input type='checkbox' name='m_hum'   " + String((m & LOG_HUM)   ? "checked" : "") + "> Luftfeuchte (%)</label>"
          + badge(aHum) + "<span class='val'>" + (live ? fmt2(live->humidity_rh, " %") : "—") + "</span>"
          "</div>";

  html += "<div class='pick-row'>"
          "<label><input type='checkbox' name='m_press' " + String((m & LOG_PRESS) ? "checked" : "") + "> Luftdruck (hPa)</label>"
          + badge(aPress) + "<span class='val'>" + (live ? fmt2(live->pressure_hpa, " hPa") : "—") + "</span>"
          "</div>";

  html += "<div class='pick-row'>"
          "<label><input type='checkbox' name='m_co2'   " + String((m & LOG_CO2)   ? "checked" : "") + "> CO₂ (ppm)</label>"
          + badge(aCo2) + "<span class='val'>" + (live ? fmt0(live->co2_ppm, " ppm") : "—") + "</span>"
          "</div>";

  // Lux placeholder (VEML7700)
  html += "<div class='pick-row'>"
          "<label><input type='checkbox' name='m_lux'   " + String((m & LOG_LUX)   ? "checked" : "") + "> Licht (lux)</label>"
          + badge(aLux) + "<span class='val'>—</span>"
          "</div>";

  html += "<div class='hint'>Wenn alles abgewählt ist, wird absichtlich nichts gespeichert.</div>";
  html += "</div>";

  html += "<div class='card'><div class='actions'>"
          "<button class='btn-primary' type='submit'>Speichern</button>"
          "</div></div>";

  html += "</form>";
  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
