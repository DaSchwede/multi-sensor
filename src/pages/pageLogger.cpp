#include <Arduino.h>
#include <WebServer.h>
#include "pages.h"
#include "version.h"
#include "settings_config/settings_common.h"

void pageLogger(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String html = pagesHeaderAuth("Logger – Verlauf", "/logger");
  html += pagesTitle("Verlauf", "Gespeicherte Messwerte von der SD-Karte als Diagramm.");

  html += "<div class='card'><h2>Auswahl</h2>";
  html += "<div class='chart-controls'>";
  html += "<div class='form-row'><label>Zeitraum</label>"
          "<select id='range'>"
          "<option value='1h'>Letzte Stunde</option>"
          "<option value='12h'>Letzte 12 Stunden</option>"
          "<option value='24h' selected>Letzte 24 Stunden</option>"
          "<option value='7d'>Letzte 7 Tage (Tagesmittel)</option>"
          "<option value='month'>Dieser Monat (Tagesmittel)</option>"
          "</select></div>";
  html += "<div class='form-row'><label>Darstellung</label>"
          "<select id='chart'>"
          "<option value='line' selected>Linie</option>"
          "<option value='bar'>Balken</option>"
          "</select></div>";
  html += "</div>";

  html += "<div class='metric-picks'>"
          "<label><input type='checkbox' class='m' value='temp' checked><span class='swatch temp'></span>Temperatur</label>"
          "<label><input type='checkbox' class='m' value='hum' checked><span class='swatch hum'></span>Luftfeuchte</label>"
          "<label><input type='checkbox' class='m' value='press' checked><span class='swatch press'></span>Luftdruck</label>"
          "<label><input type='checkbox' class='m' value='co2' checked><span class='swatch co2'></span>CO₂</label>"
          "</div>";
  html += "<div class='hint danger' id='err' hidden></div>";
  html += "</div>";

  html += "<div class='card'><h2>Diagramme</h2>"
          "<div id='charts'><div class='small'>Lade…</div></div>"
          "<div class='small mt-8'>Mit der Maus (oder dem Finger) über ein Diagramm fahren, um einzelne Werte zu sehen. "
          "Aktualisiert sich jede Minute.</div>"
          "</div>";

  html += "<div class='card'><h2>Log-Dateien</h2>"
          "<div id='files'><div class='small'>Lade…</div></div>"
          "<div class='hint'><b>Excel:</b> lesbare Uhrzeit, Semikolon und Dezimalkomma – öffnet sich direkt richtig in Excel. "
          "<b>CSV:</b> Originaldatei mit Unix-Zeitstempel, z.B. für eigene Auswertungen.</div>"
          "</div>";

  html += "<script src='/ui-logger.js?v=" FW_VERSION "' defer></script>";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
