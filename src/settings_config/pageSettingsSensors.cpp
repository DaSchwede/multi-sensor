#include <Arduino.h>
#include <WebServer.h>
#include <math.h>
#include "pages.h"
#include "html_util.h"
#include "sensors_ctrl.h"
#include "scd40_sensor.h"
#include "bme280_sensor.h"
#include "settings_config/settings_common.h"

static String fmtAge(int32_t ageMs) {
  if (ageMs < 0) return "noch keine Messung";
  const int32_t s = ageMs / 1000;
  if (s < 2) return "gerade eben";
  if (s < 120) return "vor " + String(s) + " s";
  return "vor " + String(s / 60) + " min";
}

static String fmtDuration(uint32_t ms) {
  const uint32_t s = ms / 1000;
  if (s < 60) return String(s) + " s";
  if (s < 3600) return String(s / 60) + " min " + String(s % 60) + " s";
  return String(s / 3600) + " h " + String((s % 3600) / 60) + " min";
}

static String stateBadge(bool found, int32_t ageMs) {
  if (!found) return "<span class='badge badge-bad'>nicht gefunden</span>";
  if (ageMs < 0 || ageMs > (int32_t)SENSOR_STALE_MS) return "<span class='badge badge-warn'>keine aktuellen Werte</span>";
  return "<span class='badge badge-ok'>OK</span>";
}

static String fmtVal(float v, int dec, const char *unit) {
  if (isnan(v)) return "—";
  return String(v, dec) + unit;
}

void pageSettingsSensors(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String okMsg, errMsg;

  if (server.method() == HTTP_POST) {
    const String action = server.arg("action");

    if (action == "save") {
      // Temperatur-Offset (Komma oder Punkt erlaubt)
      String off = server.arg("bme_temp_offset_c");
      off.trim();
      off.replace(",", ".");
      const float v = off.length() ? off.toFloat() : 0.0f;
      if (isnan(v) || v < -10.0f || v > 10.0f) {
        errMsg = "Temperatur-Offset muss zwischen -10 und +10 °C liegen.";
      } else {
        cfg->bme_temp_offset_c = roundf(v * 10.0f) / 10.0f;
        const bool asc = server.hasArg("scd_asc_enabled");
        const bool ascChanged = (asc != cfg->scd_asc_enabled);
        cfg->scd_asc_enabled = asc;
        saveConfig(*cfg);
        if (ascChanged && !scdSetAsc(asc)) {
          errMsg = "Gespeichert, aber der SCD40 hat die ASC-Einstellung nicht übernommen.";
        } else {
          okMsg = "Gespeichert.";
        }
      }
    }
    else if (action == "frc") {
      const long target = server.arg("frc_target").toInt();
      if (target < 400 || target > 2000) {
        errMsg = "Referenzwert muss zwischen 400 und 2000 ppm liegen.";
      } else {
        int corr = 0;
        const int rc = scdForcedRecalibration((uint16_t)target, corr);
        if (rc == 0) {
          okMsg = "Kalibrierung erfolgreich: Referenz " + String(target) + " ppm, Korrektur " +
                  (corr >= 0 ? "+" : "") + String(corr) + " ppm.";
        } else if (rc == -1) {
          errMsg = "SCD40 nicht gefunden.";
        } else if (rc == -2) {
          errMsg = "Der Sensor muss vorher mindestens 3 Minuten ununterbrochen messen.";
        } else {
          errMsg = "Der SCD40 hat die Kalibrierung abgelehnt. Bitte später erneut versuchen.";
        }
      }
    }
  }

  const SensorStatus st = sensorsGetStatus();
  SensorData* live = pagesLive();

  String html = pagesHeaderAuth("Einstellungen – Sensoren", "/settings/sensors");
  html += pagesTitle("Sensoren", "Status, Temperatur-Korrektur und CO₂-Kalibrierung.");

  if (okMsg.length())  html += "<div class='hint'><b>✔ " + htmlEscape(okMsg) + "</b></div>";
  if (errMsg.length()) html += "<div class='hint danger'><b>" + htmlEscape(errMsg) + "</b></div>";

  // ===== Status =====
  html += "<div class='card'><h2>Status</h2><table class='tbl'>";
  html += "<tr><th>BME280 (Temp./Feuchte/Druck)</th><td>" + stateBadge(st.bmeFound, st.bmeAgeMs) +
          " <span class='small'>" + fmtAge(st.bmeAgeMs) + "</span></td></tr>";
  html += "<tr><th>SCD40 (CO₂)</th><td>" + stateBadge(st.scdFound, st.scdAgeMs) +
          " <span class='small'>" + fmtAge(st.scdAgeMs) + "</span></td></tr>";
  html += "<tr><th>SCD40 misst seit</th><td>" + (st.scdFound ? fmtDuration(st.scdRunningMs) : String("—")) + "</td></tr>";
  html += "<tr><th>Luftdruck-Kompensation</th><td>" +
          (isnan(st.scdPressureHpa) ? String("inaktiv (kein Luftdruck vom BME280)")
                                    : "aktiv, " + String(st.scdPressureHpa, 0) + " hPa") + "</td></tr>";
  html += "</table>";
  html += "<div class='hint'>Liefert ein Sensor länger als " + String(SENSOR_STALE_MS / 1000) +
          " s keine gültigen Werte, werden seine Werte als „keine Daten“ behandelt (kein Senden per UDP/MQTT, "
          "leer im Logger). Ein ausgefallener Sensor wird automatisch jede Minute neu gesucht.</div>";
  html += "<form method='POST' action='/action/rescan_sensors'><div class='actions'>"
          "<button class='btn-secondary' type='submit'>Sensoren neu erkennen</button></div></form>";
  html += "</div>";

  // ===== Einstellungen =====
  html += "<form method='POST' action='/settings/sensors'>";
  html += "<input type='hidden' name='action' value='save'>";

  html += "<div class='card'><h2>BME280 – Temperatur-Korrektur</h2>";
  html += "<div class='form-row'><label>Temperatur-Offset (°C)</label>"
          "<input name='bme_temp_offset_c' type='number' step='0.1' min='-10' max='10' value='" +
          String(cfg->bme_temp_offset_c, 1) + "'></div>";
  html += "<table class='tbl'>";
  html += "<tr><th>Gemessen (roh)</th><td>" + fmtVal(st.bmeRawTempC, 1, " °C") + " / " + fmtVal(st.bmeRawHum, 0, " %") + "</td></tr>";
  html += "<tr><th>Angezeigt (korrigiert)</th><td>" +
          (live ? fmtVal(live->temperature_c, 1, " °C") + " / " + fmtVal(live->humidity_rh, 0, " %") : String("—")) + "</td></tr>";
  html += "</table>";
  html += "<div class='hint'>Durch die Abwärme des ESP32 misst der BME280 meist 1–3 °C zu warm. "
          "Mit einem Referenzthermometer vergleichen und die Differenz eintragen, z.B. <b>-1.8</b>. "
          "Die Luftfeuchte wird automatisch passend umgerechnet.</div>";
  html += "</div>";

  html += "<div class='card'><h2>SCD40 – CO₂</h2>";
  html += "<div class='form-row'><label>Automatische Selbstkalibrierung (ASC)</label>"
          "<label class='switch'>"
          "<input type='checkbox' name='scd_asc_enabled' " + String(cfg->scd_asc_enabled ? "checked" : "") + ">"
          "<span class='slider'></span>"
          "</label></div>";
  html += "<div class='hint'>Die ASC geht davon aus, dass der Raum <b>mindestens einmal pro Woche</b> gut gelüftet "
          "wird (≈ 400 ppm), und gleicht die Drift des Sensors darüber automatisch aus. "
          "In Räumen, die nie richtig gelüftet werden, ausschalten und stattdessen gelegentlich manuell kalibrieren.</div>";
  html += "</div>";

  html += "<div class='card'><div class='actions'>"
          "<button class='btn-primary' type='submit'>Speichern</button>"
          "</div></div>";
  html += "</form>";

  // ===== Kalibrierung =====
  const bool frcReady = st.scdFound && st.scdRunningMs >= 180000UL;
  html += "<div class='card'><h2>SCD40 – Kalibrierung an Frischluft</h2>";
  html += "<form method='POST' action='/settings/sensors' "
          "onsubmit=\"return confirm('Steht der Sensor seit mindestens 3 Minuten an frischer Luft? Jetzt kalibrieren?');\">"
          "<input type='hidden' name='action' value='frc'>"
          "<div class='form-row'><label>Referenzwert (ppm)</label>"
          "<input name='frc_target' type='number' min='400' max='2000' step='1' value='420'></div>"
          "<div class='actions'>"
          "<button class='btn-primary' type='submit'" + String(frcReady ? "" : " disabled") + ">Jetzt kalibrieren</button>"
          "</div></form>";
  if (!st.scdFound) {
    html += "<div class='hint danger'>SCD40 nicht gefunden.</div>";
  } else if (!frcReady) {
    html += "<div class='hint warn'>Möglich in " + fmtDuration(180000UL - st.scdRunningMs) +
            " – der Sensor muss vorher 3 Minuten ununterbrochen messen. Seite dann neu laden.</div>";
  }
  html += "<div class='hint warn'><b>So geht's:</b> Sensor nach draußen oder ans weit geöffnete Fenster stellen "
          "(nicht anhauchen, Abstand zu Personen), mindestens 3 Minuten warten, dann kalibrieren. "
          "Außenluft hat derzeit etwa <b>420 ppm</b>. Das Ergebnis speichert der Sensor dauerhaft.</div>";
  html += "</div>";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
