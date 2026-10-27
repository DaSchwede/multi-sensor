#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <time.h>
#include <math.h>
#include <memory>
#include <vector>
#include <algorithm>
#include "logger.h"

#include "settings_config/settings_common.h"

// Messwerte: Key in der API  <->  Spaltenname im CSV
static const int NM = 4;
static const char* KEYS[NM] = { "temp",   "hum",    "press",     "co2"     };
static const char* COLS[NM] = { "temp_c", "hum_rh", "press_hpa", "co2_ppm" };

// max. Anzahl Punkte, die an den Browser gehen (RAM + Lesbarkeit)
static const int MAX_BUCKETS = 240;

static int keyIndex(const String &k) {
  for (int i = 0; i < NM; i++) if (k == KEYS[i]) return i;
  return -1;
}

static bool timeIsValid() {
  return time(nullptr) > 1672531200;
}

static time_t localMidnight(time_t t) {
  struct tm lt {};
  localtime_r(&t, &lt);
  lt.tm_hour = 0; lt.tm_min = 0; lt.tm_sec = 0;
  lt.tm_isdst = -1;
  return mktime(&lt);
}

static time_t addDaysMidnight(time_t midnight, int days) {
  struct tm lt {};
  localtime_r(&midnight, &lt);
  lt.tm_mday += days;
  lt.tm_hour = 0; lt.tm_min = 0; lt.tm_sec = 0;
  lt.tm_isdst = -1;
  return mktime(&lt);
}

static String logPathForDay(time_t t) {
  struct tm lt {};
  localtime_r(&t, &lt);
  char buf[24];
  snprintf(buf, sizeof(buf), "/log/%04d-%02d-%02d.csv", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
  return String(buf);
}

// Liest eine CSV-Zeile in Felder (ohne String-Kopien pro Feld)
static int splitFields(char *line, char *fields[], int maxF) {
  int n = 0;
  char *p = line;
  while (n < maxF) {
    fields[n++] = p;
    char *c = strchr(p, ',');
    if (!c) break;
    *c = 0;
    p = c + 1;
  }
  return n;
}

// Ruft cb(epoch, values[NM], has[NM]) für jede Datenzeile der Datei auf.
template <typename F>
static void scanLogFile(const String &path, F cb) {
  if (!SD.exists(path)) return;
  File f = SD.open(path, FILE_READ);
  if (!f) return;

  int idxEpoch = 0;
  int idx[NM] = { 1, 2, 3, 4 };   // Positionen ohne Header (altes Format)
  bool first = true;

  char line[160];
  char *fields[16];

  while (f.available()) {
    size_t len = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[len] = 0;
    while (len && (line[len - 1] == '\r' || line[len - 1] == ' ')) line[--len] = 0;
    if (!len) continue;

    if (first) {
      first = false;
      if (strncmp(line, "epoch", 5) == 0) {
        // Header: Spalten per Name zuordnen (ältere Dateien hatten je nach Auswahl andere Spalten)
        const int n = splitFields(line, fields, 16);
        idxEpoch = -1;
        for (int m = 0; m < NM; m++) idx[m] = -1;
        for (int i = 0; i < n; i++) {
          if (strcmp(fields[i], "epoch") == 0) idxEpoch = i;
          for (int m = 0; m < NM; m++) if (strcmp(fields[i], COLS[m]) == 0) idx[m] = i;
        }
        if (idxEpoch < 0) break;
        continue;
      }
    }

    const int n = splitFields(line, fields, 16);
    if (n <= idxEpoch) continue;
    const time_t ep = (time_t)strtoul(fields[idxEpoch], nullptr, 10);
    if (ep < 1672531200) continue;   // unplausibel (z.B. abgeschnittene Zeile)

    float vals[NM];
    bool has[NM];
    for (int m = 0; m < NM; m++) {
      has[m] = false;
      vals[m] = NAN;
      if (idx[m] >= 0 && idx[m] < n && fields[idx[m]][0] != 0) {
        char *end = nullptr;
        const float v = strtof(fields[idx[m]], &end);
        if (end != fields[idx[m]] && !isnan(v)) { vals[m] = v; has[m] = true; }
      }
    }
    cb(ep, vals, has);
  }
  f.close();
}

struct Bucket {
  float    sum[NM];
  uint16_t cnt[NM];
};

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

  const String range = server.hasArg("range") ? server.arg("range") : "24h";
  const String chart = server.hasArg("chart") ? server.arg("chart") : "line";
  const String metricsArg = server.hasArg("metrics") ? server.arg("metrics") : "temp,hum,press,co2";

  // gewünschte Messwerte
  bool want[NM] = { false, false, false, false };
  {
    int start = 0;
    while (start <= (int)metricsArg.length()) {
      int c = metricsArg.indexOf(',', start);
      String k = (c < 0) ? metricsArg.substring(start) : metricsArg.substring(start, c);
      k.trim();
      const int i = keyIndex(k);
      if (i >= 0) want[i] = true;
      if (c < 0) break;
      start = c + 1;
    }
  }

  const time_t now = time(nullptr);
  const time_t today = localMidnight(now);

  // ===== Zeitfenster =====
  bool dayMode = false;     // true: ein Balken/Punkt pro Tag (Tagesmittel)
  time_t tMin = 0;
  uint32_t bucketSec = 60;
  int nb = 0;

  if (range == "1h")            { tMin = now - 3600;      bucketSec = 60;  }
  else if (range == "12h")      { tMin = now - 12 * 3600; bucketSec = 180; }
  else if (range == "24h")      { tMin = now - 24 * 3600; bucketSec = 360; }
  else if (range == "today")    { tMin = today;           bucketSec = 360; }
  else if (range == "7d")       { dayMode = true; tMin = addDaysMidnight(today, -6); }
  else if (range == "month")    {
    struct tm lt {};
    localtime_r(&now, &lt);
    dayMode = true;
    tMin = addDaysMidnight(today, -(lt.tm_mday - 1));
  } else {
    server.send(400, "application/json", "{\"error\":\"bad_range\"}");
    return;
  }

  if (dayMode) {
    for (time_t d = tMin; d <= today && nb < 31; d = addDaysMidnight(d, 1)) nb++;
  } else {
    nb = (int)((now - tMin) / bucketSec) + 1;
  }
  if (nb > MAX_BUCKETS) nb = MAX_BUCKETS;
  if (nb < 1) nb = 1;

  std::unique_ptr<Bucket[]> buckets(new (std::nothrow) Bucket[nb]);
  if (!buckets) {
    server.send(500, "application/json", "{\"error\":\"out_of_memory\"}");
    return;
  }
  memset(buckets.get(), 0, sizeof(Bucket) * nb);

  // ===== Dateien lesen: alle Tage, die das Zeitfenster berührt =====
  int dayIdx = 0;
  for (time_t d = localMidnight(tMin); d <= today; d = addDaysMidnight(d, 1), dayIdx++) {
    const int thisDay = dayIdx;
    scanLogFile(logPathForDay(d), [&](time_t ep, const float *vals, const bool *has) {
      int b;
      if (dayMode) {
        b = thisDay;
      } else {
        if (ep < tMin || ep > now) return;
        b = (int)((ep - tMin) / bucketSec);
      }
      if (b < 0 || b >= nb) return;
      for (int m = 0; m < NM; m++) {
        if (!want[m] || !has[m]) continue;
        buckets[b].sum[m] += vals[m];
        if (buckets[b].cnt[m] < 65535) buckets[b].cnt[m]++;
      }
    });
  }

  // ===== JSON bauen =====
  JsonDocument doc;
  doc["mode"] = (chart == "bar") ? "bar" : "line";
  doc["range"] = range;
  JsonArray labels = doc["labels"].to<JsonArray>();
  JsonObject series = doc["series"].to<JsonObject>();
  JsonArray arr[NM];
  for (int m = 0; m < NM; m++) if (want[m]) arr[m] = series[KEYS[m]].to<JsonArray>();

  for (int b = 0; b < nb; b++) {
    bool any = false;
    for (int m = 0; m < NM; m++) if (want[m] && buckets[b].cnt[m]) any = true;
    if (!any && !dayMode) continue;   // leere Zeitabschnitte weglassen, leere Tage als Lücke zeigen

    char lb[8];
    struct tm lt {};
    if (dayMode) {
      const time_t d = addDaysMidnight(tMin, b);
      localtime_r(&d, &lt);
      snprintf(lb, sizeof(lb), "%02d.%02d", lt.tm_mday, lt.tm_mon + 1);
    } else {
      const time_t t = tMin + (time_t)b * bucketSec;
      localtime_r(&t, &lt);
      snprintf(lb, sizeof(lb), "%02d:%02d", lt.tm_hour, lt.tm_min);
    }
    labels.add(lb);

    for (int m = 0; m < NM; m++) {
      if (!want[m]) continue;
      if (buckets[b].cnt[m]) {
        const double avg = buckets[b].sum[m] / buckets[b].cnt[m];
        arr[m].add(round(avg * 100.0) / 100.0);
      } else {
        arr[m].add(nullptr);
      }
    }
  }

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// ===== Log-Dateien: Liste + Download =====

// Erlaubt nur "JJJJ-MM-TT.csv" (verhindert Pfad-Tricks wie "../")
static bool validLogName(const String &n) {
  if (n.length() != 14 || !n.endsWith(".csv")) return false;
  for (int i = 0; i < 10; i++) {
    const char c = n[i];
    if (i == 4 || i == 7) { if (c != '-') return false; }
    else if (c < '0' || c > '9') return false;
  }
  return true;
}

void apiLogList(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;
  if (!loggerSdOk()) {
    server.send(503, "application/json", "{\"error\":\"sd_not_ready\"}");
    return;
  }

  struct Entry { String name; uint32_t size; };
  std::vector<Entry> files;
  File dir = SD.open("/log");
  if (dir) {
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      if (!f.isDirectory()) {
        String n = String(f.name());
        const int slash = n.lastIndexOf('/');
        if (slash >= 0) n = n.substring(slash + 1);
        if (validLogName(n)) files.push_back({ n, (uint32_t)f.size() });
      }
      f.close();
    }
    dir.close();
  }
  // neueste zuerst
  std::sort(files.begin(), files.end(), [](const Entry &a, const Entry &b){ return a.name > b.name; });

  String out = "{\"files\":[";
  for (size_t i = 0; i < files.size(); i++) {
    if (i) out += ",";
    out += "{\"name\":\"" + files[i].name + "\",\"size\":" + String(files[i].size) + "}";
  }
  out += "]}";
  server.send(200, "application/json", out);
}

void apiLogDownload(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  const String name = server.arg("f");
  if (!validLogName(name)) {
    server.send(400, "text/plain", "Ungültiger Dateiname");
    return;
  }
  if (!loggerSdOk()) {
    server.send(503, "text/plain", "SD-Karte nicht bereit");
    return;
  }
  const String path = "/log/" + name;
  File f = SD.open(path, FILE_READ);
  if (!f) {
    server.send(404, "text/plain", "Datei nicht gefunden");
    return;
  }
  const bool excel = (server.arg("fmt") == "excel");
  if (!excel) {
    // Original-Datei (Unix-Zeit, Komma-getrennt) – gut für Skripte/Auswertungen
    server.sendHeader("Content-Disposition", "attachment; filename=\"multi-sensor_" + name + "\"");
    server.streamFile(f, "text/csv");
    f.close();
    return;
  }
  f.close();

  // Excel (deutsch): lesbare Ortszeit, Semikolon, Dezimalkomma, UTF-8 mit BOM
  String outName = name;
  outName.replace(".csv", "_excel.csv");
  server.sendHeader("Content-Disposition", "attachment; filename=\"multi-sensor_" + outName + "\"");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv; charset=utf-8", "");

  String buf;
  buf.reserve(1200);
  buf += "\xEF\xBB\xBF";   // BOM, damit Excel UTF-8 (°, ₂) richtig erkennt
  buf += "Zeit;Temperatur (°C);Luftfeuchte (%);Luftdruck (hPa);CO2 (ppm)\r\n";

  auto num = [](float v, int dec) -> String {
    if (isnan(v)) return String();
    String t = String(v, dec);
    t.replace(".", ",");
    return t;
  };
  static const int DECIMALS[NM] = { 2, 2, 2, 0 };

  scanLogFile(path, [&](time_t ep, const float *vals, const bool *has) {
    struct tm lt {};
    localtime_r(&ep, &lt);
    char ts[24];
    strftime(ts, sizeof(ts), "%d.%m.%Y %H:%M:%S", &lt);
    buf += ts;
    for (int m = 0; m < NM; m++) {
      buf += ";";
      if (has[m]) buf += num(vals[m], DECIMALS[m]);
    }
    buf += "\r\n";
    if (buf.length() > 1000) {
      server.sendContent(buf);
      buf = "";
    }
  });
  if (buf.length()) server.sendContent(buf);
  server.sendContent("");   // Ende der chunked Antwort
}
