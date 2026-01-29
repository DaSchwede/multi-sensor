#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <time.h>
#include "logger.h"

#include "settings_config/settings_common.h"

// CSV Spaltennamen aus Logger
static const char* COL_EPOCH = "epoch";
static const char* COL_TEMP  = "temp_c";
static const char* COL_HUM   = "hum_rh";
static const char* COL_PRESS = "press_hpa";
static const char* COL_CO2   = "co2_ppm";

static const char* metricToCol(const String& metricKey);
static bool parseFloatField(const String& f, float& out);
static void splitCsv(const String& s, String out[], int maxParts, int& n);
static int  findColIndex(const String cols[], int n, const char* name);
static bool looksLikeHeader(const String& line);
static int  metricToIndexNoHeader(const String& metricKey);

static bool timeIsValid() {
  time_t now = time(nullptr);
  return (now > 1672531200);
}

static bool looksLikeHeader(const String& line){
  return line.startsWith("epoch");
}

// wenn KEIN Header: feste Spaltenpositionen
static int metricToIndexNoHeader(const String& metricKey){
  // 0=epoch, 1=temp, 2=hum, 3=press, 4=co2
  if (metricKey == "temp")  return 1;
  if (metricKey == "hum")   return 2;
  if (metricKey == "press") return 3;
  if (metricKey == "co2")   return 4;
  return -1;
}

static String dayStringLocalFromEpoch(time_t t) {
  struct tm tmLocal{};
  localtime_r(&t, &tmLocal);
  char buf[16];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
           tmLocal.tm_year + 1900, tmLocal.tm_mon + 1, tmLocal.tm_mday);
  return String(buf);
}

static String monthStringLocalFromEpoch(time_t t) {
  struct tm tmLocal{};
  localtime_r(&t, &tmLocal);
  char buf[8];
  snprintf(buf, sizeof(buf), "%04d-%02d", tmLocal.tm_year + 1900, tmLocal.tm_mon + 1);
  return String(buf);
}

static String logPathForDay(const String& day) {
  return String("/log/") + day + ".csv";
}

static void splitCsv(const String& s, String out[], int maxParts, int& n) {
  n = 0;
  int start = 0;
  while (n < maxParts) {
    int comma = s.indexOf(',', start);
    if (comma < 0) {
      out[n] = s.substring(start);
      out[n].trim();
      n++;
      break;
    }
    out[n] = s.substring(start, comma);
    out[n].trim();
    n++;
    start = comma + 1;
  }
}

static int findColIndex(const String cols[], int n, const char* name) {
  for (int i=0;i<n;i++) if (cols[i] == name) return i;
  return -1;
}

static bool parseFloatField(const String& f, float& out) {
  if (f.length() == 0) return false;

  // toFloat() gibt 0 zurück auch bei Müll → wir lassen 0 aber zu.
  // Für bessere Validierung könnte man isDigit/strtod nutzen, aber reicht hier.
  out = f.toFloat();
  return true;
}

static void calcDailyAvg(const String& path, const String& metricKey,
                         double& sum, uint32_t& cnt) {
  if (!SD.exists(path)) return;

  File f = SD.open(path, FILE_READ);
  if (!f) return;

  String first = f.readStringUntil('\n');
  first.trim();
  if (!first.length()) { f.close(); return; }

  const int MAXC=16;
  String cols[MAXC]; int n=0;
  String parts[MAXC]; int pn=0;

  bool hasHeader = looksLikeHeader(first);

  int idxEpoch = 0;
  int idxVal   = -1;

  if (hasHeader) {
    splitCsv(first, cols, MAXC, n);
    idxEpoch = findColIndex(cols, n, COL_EPOCH);
    const char* col = metricToCol(metricKey);
    idxVal = col ? findColIndex(cols, n, col) : -1;
    if (idxEpoch < 0 || idxVal < 0) { f.close(); return; }
  } else {
    idxEpoch = 0;
    idxVal = metricToIndexNoHeader(metricKey);
    // first ist Datenzeile
    splitCsv(first, parts, MAXC, pn);
    if (pn > idxVal) {
      float v;
      if (parseFloatField(parts[idxVal], v) && !isnan(v)) { sum += v; cnt++; }
    }
  }

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length()) continue;

    splitCsv(line, parts, MAXC, pn);
    if (pn <= idxVal) continue;

    float v;
    if (parseFloatField(parts[idxVal], v) && !isnan(v)) { sum += v; cnt++; }
  }

  f.close();
}

static const char* metricToCol(const String& metricKey) {
  if (metricKey == "temp")  return COL_TEMP;
  if (metricKey == "hum")   return COL_HUM;
  if (metricKey == "press") return COL_PRESS;
  if (metricKey == "co2")   return COL_CO2;
  return nullptr;
}

void apiHistory(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

    if (!loggerSdOk()) {
    server.send(503, "application/json", "{\"error\":\"sd_not_ready\"}");
    return;
  }


  if (!timeIsValid()) {
    server.send(409, "application/json", "{\"error\":\"time_not_set\"}");
    return;
  }

  String range = server.hasArg("range") ? server.arg("range") : "24h";
  String metricsArg = server.hasArg("metrics") ? server.arg("metrics") : "temp,hum,press,co2";

  // metrics split
  String metrics[8]; int mn=0;
  {
    int start=0;
    while (mn < 8) {
      int c = metricsArg.indexOf(',', start);
      if (c < 0) { metrics[mn++] = metricsArg.substring(start); break; }
      metrics[mn++] = metricsArg.substring(start, c);
      start = c+1;
    }
    for (int i=0;i<mn;i++) metrics[i].trim();
  }

  DynamicJsonDocument doc(18000);
  JsonArray labels = doc.createNestedArray("labels");
  JsonObject series = doc.createNestedObject("series");
  doc["mode"] = "line";

  time_t now = time(nullptr);

  if (range == "24h") {
    // Labels/Datensätze auf Rohpunkte (max ~48)
    // Wir lesen heute + gestern und filtern nach now-24h
    time_t tMin = now - 24*3600;

    String d0 = dayStringLocalFromEpoch(now);
    String d1 = dayStringLocalFromEpoch(now - 24*3600);

    String p0 = logPathForDay(d0);
    String p1 = logPathForDay(d1);

    // Für 24h wollen wir EIN gemeinsames labels-array (Zeitpunkte)
    // => wir lesen pro Datei einmal und sammeln labels; pro metric parallel.
    // Einfacher Ansatz: wir bauen zuerst eine temporäre Liste von epochs/labels und pro metric Werte.
    // Für geringe Punktzahl ok: wir lesen je metric separat (simpel).

    // Labels entstehen beim ersten Metric-Lauf (damit alle gleich lang sind).
    // Danach müssen die anderen Serien exakt gleich viele Werte bekommen.
    // => Wir lesen einmal die "Master"-Metric (erste gültige), und bei anderen metrics lesen wir mit gleicher Filterung,
    // aber das wäre aufwändiger.
    //
    // Pragmatik: Wir liefern pro metric eigene Datenlänge? -> schlecht fürs Chart.
    //
    // Daher: Wir nehmen als Master den epoch aus CSV und füllen pro Zeile alle metrics in einem Pass.
    // => implementieren wir jetzt direkt.

    auto read24hFile = [&](const String& path,
                       const String metricKeys[], int metricCount,
                       JsonArray labelsArr, JsonArray seriesArrs[])->void {

  if (!SD.exists(path)) return;
  File f = SD.open(path, FILE_READ);
  if (!f) return;

  String first = f.readStringUntil('\n');
  first.trim();
  if (!first.length()) { f.close(); return; }

  const int MAXC = 16;
  String cols[MAXC]; int n=0;
  String parts[MAXC]; int pn=0;

  bool hasHeader = looksLikeHeader(first);

  int idxEpoch = 0;
  int idx[8];
  for (int i=0;i<metricCount;i++) idx[i] = -1;

  if (hasHeader) {
    splitCsv(first, cols, MAXC, n);
    idxEpoch = findColIndex(cols, n, COL_EPOCH);
    if (idxEpoch < 0) { f.close(); return; }

    for (int i=0;i<metricCount;i++) {
      const char* col = metricToCol(metricKeys[i]);
      idx[i] = col ? findColIndex(cols, n, col) : -1;
    }
  } else {
    // kein Header → feste Positionen
    idxEpoch = 0;
    for (int i=0;i<metricCount;i++) idx[i] = metricToIndexNoHeader(metricKeys[i]);

    // first ist Datenzeile -> mitverarbeiten
    splitCsv(first, parts, MAXC, pn);
    if (pn > idxEpoch) {
      time_t ep = (time_t)parts[idxEpoch].toInt();
      if (ep >= tMin) {
        struct tm tmLocal{};
        localtime_r(&ep, &tmLocal);
        char lb[6];
        snprintf(lb, sizeof(lb), "%02d:%02d", tmLocal.tm_hour, tmLocal.tm_min);
        labelsArr.add(String(lb));

        for (int i=0;i<metricCount;i++) {
          float v;
          if (idx[i] >= 0 && pn > idx[i] && parseFloatField(parts[idx[i]], v)) seriesArrs[i].add(v);
          else seriesArrs[i].add(nullptr);
        }
      }
    }
  }

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length()) continue;

    splitCsv(line, parts, MAXC, pn);
    if (pn <= idxEpoch) continue;

    time_t ep = (time_t)parts[idxEpoch].toInt();
    if (ep < tMin) continue;

    struct tm tmLocal{};
    localtime_r(&ep, &tmLocal);
    char lb[6];
    snprintf(lb, sizeof(lb), "%02d:%02d", tmLocal.tm_hour, tmLocal.tm_min);
    labelsArr.add(String(lb));

    for (int i=0;i<metricCount;i++) {
      float v;
      if (idx[i] >= 0 && pn > idx[i] && parseFloatField(parts[idx[i]], v)) seriesArrs[i].add(v);
      else seriesArrs[i].add(nullptr);
    }
  }

    f.close();
  };

        // series arrays anlegen
    String metricKeys[8];
JsonArray seriesArrs[8];
int metricCount = 0;

for (int i=0;i<mn;i++){
  const String& key = metrics[i];
  if (!metricToCol(key)) continue; // nur bekannte

  metricKeys[metricCount] = key;
  seriesArrs[metricCount] = series.createNestedArray(key);
  metricCount++;
}

read24hFile(p1, metricKeys, metricCount, labels, seriesArrs);
read24hFile(p0, metricKeys, metricCount, labels, seriesArrs);

  } else if (range == "7d") {
    // pro Tag 1 Punkt: Tages-AVG
    for (int di=6; di>=0; di--) {
      time_t t = now - (time_t)di * 86400;
      String day = dayStringLocalFromEpoch(t);
      String path = logPathForDay(day);

      labels.add(day);

      for (int i=0;i<mn;i++){
        const String& key = metrics[i];
        const char* col = metricToCol(key);
        if (!col) continue;

        if (!series.containsKey(key)) series.createNestedArray(key);
        JsonArray arr = series[key].as<JsonArray>();

        double sum=0; uint32_t cnt=0;
        calcDailyAvg(path, key, sum, cnt);
        if (cnt == 0) arr.add(nullptr);
        else arr.add((float)(sum / (double)cnt));
      }
    }

  } else if (range == "month") {
    // aktueller Monat: Tages-AVG für jeden Tag vom 1. bis heute
    String curMonth = monthStringLocalFromEpoch(now);
    doc["mode"] = "bar";


    struct tm tmNow{};
    localtime_r(&now, &tmNow);

    int days = tmNow.tm_mday; // 1..heute
    for (int d=1; d<=days; d++) {
      char dayBuf[16];
      snprintf(dayBuf, sizeof(dayBuf), "%s-%02d", curMonth.c_str(), d);
      String day(dayBuf);
      String path = logPathForDay(day);

      labels.add(day);

      for (int i=0;i<mn;i++){
        const String& key = metrics[i];
        const char* col = metricToCol(key);
        if (!col) continue;

        if (!series.containsKey(key)) series.createNestedArray(key);
        JsonArray arr = series[key].as<JsonArray>();

        double sum=0; uint32_t cnt=0;
        calcDailyAvg(path, col, sum, cnt);
        if (cnt == 0) arr.add(nullptr);
        else arr.add((float)(sum / (double)cnt));
      }
    }

  } else {
    server.send(400, "application/json", "{\"error\":\"bad_range\"}");
    return;
  }

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}
