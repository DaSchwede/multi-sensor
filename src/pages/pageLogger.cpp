#include <Arduino.h>
#include <WebServer.h>
#include "pages.h"
#include "settings_config/settings_common.h"

void pageLogger(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String html = pagesHeaderAuth("Logger – Verlauf", "/logger");

  html += "<div class='card'><h2>Verlauf</h2>";

  html += "<div class='form-row'><label>Zeitraum</label>"
          "<select id='range'>"
          "<option value='1h'>Letzte 1 Stunde</option>"
          "<option value='12h'>Letzte 12 Stunden</option>"
          "<option value='24h'>Letzte 24 Stunden</option>"
          "<option value='7d'>Letzte 7 Tage</option>"
          "<option value='month'>Dieser Monat</option>"
          "</select></div>";

  html += "<div class='form-row'><label>Darstellung</label>"
          "<select id='chart'>"
          "<option value='bar'>Balkendiagramm</option>"
          "<option value='line'>Streifendiagramm</option>"
          "</select></div>";

  html += "<div class='hint'>Werte auswählen und Zeitraum ändern – Diagramm lädt automatisch.</div>";

  // Checkboxen (IDs = metric keys)
  html += "<div class='pick-row'><label><input type='checkbox' class='m' value='temp' checked> Temperatur (°C)</label></div>";
  html += "<div class='pick-row'><label><input type='checkbox' class='m' value='hum'  checked> Luftfeuchte (%)</label></div>";
  html += "<div class='pick-row'><label><input type='checkbox' class='m' value='press' checked> Luftdruck (hPa)</label></div>";
  html += "<div class='pick-row'><label><input type='checkbox' class='m' value='co2'  checked> CO₂ (ppm)</label></div>";

  html += "<div class='hint warn logger-err' id='err' hidden></div>";
  html += "<canvas id='c' width='900' height='320' class='logger-canvas'></canvas>";

  html += "</div>"; // card

  // Minimaler Canvas-Renderer (Linie + Achsen + Legend)
  html += "<script src='/logger.js' defer></script>";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
