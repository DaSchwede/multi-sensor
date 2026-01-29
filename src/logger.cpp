#include "logger.h"
#include "ntp_time.h"
#include <time.h>
#include <math.h>
#include "sensor_data.h"
#include "log_bits.h"

#include "pins.h"

#if defined(ESP32)
  #include <SD.h>
  #include <SPI.h>
#else
  #error "Logger SD-only: aktuell nur fuer ESP32 vorgesehen"
#endif

static bool     g_sd_ok = false;
static uint32_t g_lastLogMs = 0;
static String   g_curDay = "";
static bool     g_headerWritten = false;

static uint32_t g_lastCleanupEpoch = 0;   // 1x pro Tag Cleanup

bool loggerSdOk() { return g_sd_ok; }

const uint32_t spiHz = 10000000; // 10 MHz

static bool timeIsValid() {
  time_t now = time(nullptr);
  // grob: >= 2023-01-01
  return (now > 1672531200);
}

static String dayStringLocal() {
  time_t now = time(nullptr);
  struct tm t {};
  localtime_r(&now, &t);
  char buf[16];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
  return String(buf);
}

static String logPathForDay(const String& day) {
  return String("/log/") + day + ".csv";
}

static void ensureLogDir() {
  if (!SD.exists("/log")) SD.mkdir("/log");
}

static void writeHeaderIfNeeded(const AppConfig& cfg, File& f) {
  if (g_headerWritten) return;

  // falls Datei schon Inhalt hat -> Header als vorhanden ansehen
  if (f.size() > 0) {
    g_headerWritten = true;
    return;
  }

  String h = "epoch";
  if (cfg.log_metric_mask & LOG_TEMP)  h += ",temp_c";
  if (cfg.log_metric_mask & LOG_HUM)   h += ",hum_rh";
  if (cfg.log_metric_mask & LOG_PRESS) h += ",press_hpa";
  if (cfg.log_metric_mask & LOG_CO2)   h += ",co2_ppm";
  h += "\n";

  f.print(h);
  g_headerWritten = true;
}

static void appendLine(const AppConfig& cfg, const SensorData& d) {
  if (!g_sd_ok) return;
  if (!timeIsValid()) return;

  const String day = dayStringLocal();
  if (day != g_curDay) {
    g_curDay = day;
    g_headerWritten = false;
  }

  ensureLogDir();
  const String path = logPathForDay(day);

  Serial.println("[logger] try write: " + path);
  Serial.println("[logger] /log exists? " + String(SD.exists("/log")));

  File f = SD.open(path, FILE_APPEND);
    if (!f) {
    Serial.println("[logger] SD.open FAILED: " + path);
    return;
  }
  Serial.println("[logger] SD.open OK");


  writeHeaderIfNeeded(cfg, f);

  time_t now = time(nullptr);
  String line = String((uint32_t)now);

  auto addVal = [&](bool enabled, float v) {
    if (!enabled) return;
    line += ",";
    if (!isnan(v)) line += String(v, 2);
    // wenn NAN -> leer lassen (",")
  };

  addVal(cfg.log_metric_mask & LOG_TEMP,  d.temperature_c);
  addVal(cfg.log_metric_mask & LOG_HUM,   d.humidity_rh);
  addVal(cfg.log_metric_mask & LOG_PRESS, d.pressure_hpa);
  addVal(cfg.log_metric_mask & LOG_CO2,   d.co2_ppm);

  line += "\n";
  f.print(line);
  f.close();
}

LoggerSdInfo loggerGetSdInfo() {
  LoggerSdInfo s;
  s.ok = g_sd_ok;
  if (!g_sd_ok) return s;
  s.total = SD.totalBytes();
  s.used  = SD.usedBytes();
  s.free  = (s.total > s.used) ? (s.total - s.used) : 0;
  return s;
}

uint16_t loggerCountLogDays() {
  if (!g_sd_ok) return 0;
  if (!SD.exists("/log")) return 0;   // <-- wichtig

  File dir = SD.open("/log");
  if (!dir) return 0;

  uint16_t cnt = 0;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (!f.isDirectory()) cnt++;
    f.close();
  }
  dir.close();
  return cnt;
}


static int32_t daysBetween(time_t a, time_t b) {
  return (int32_t)((a - b) / 86400);
}

static bool parseDayFromFilename(const String& name, int& y, int& m, int& d) {
  // erwartet: "YYYY-MM-DD.csv"
  if (name.length() != 14) return false;
  if (name.charAt(4) != '-' || name.charAt(7) != '-') return false;
  if (!name.endsWith(".csv")) return false;

  y = name.substring(0, 4).toInt();
  m = name.substring(5, 7).toInt();
  d = name.substring(8,10).toInt();
  return (y >= 1970 && m >= 1 && m <= 12 && d >= 1 && d <= 31);
}

static time_t dayStartEpochLocal(int y, int m, int d) {
  struct tm t {};
  t.tm_year = y - 1900;
  t.tm_mon  = m - 1;
  t.tm_mday = d;
  t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0;
  return mktime(&t); // local time
}

static void retentionCleanup(const AppConfig& cfg) {
  if (!g_sd_ok) return;
  if (!timeIsValid()) return;

  // 0 = nie löschen
  if (cfg.log_retention_days == 0) return;

  time_t now = time(nullptr);

  // max 1x pro Tag
  if (g_lastCleanupEpoch != 0 && daysBetween(now, (time_t)g_lastCleanupEpoch) < 1) return;
  g_lastCleanupEpoch = (uint32_t)now;

  File dir = SD.open("/log");
  if (!dir) return;

  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (f.isDirectory()) { f.close(); continue; }

    String fn = String(f.name());
    f.close();

    // basename
    int slash = fn.lastIndexOf('/');
    String base = (slash >= 0) ? fn.substring(slash + 1) : fn;

    int y,m,d;
    if (!parseDayFromFilename(base, y, m, d)) continue;

    time_t dayEpoch = dayStartEpochLocal(y, m, d);
    int32_t ageDays = daysBetween(now, dayEpoch);

    if (ageDays > (int32_t)cfg.log_retention_days) {
      SD.remove(String("/log/") + base);
    }
  }

  dir.close();
}

void loggerBegin(const AppConfig& cfg) {
  g_sd_ok = false;
  g_lastLogMs = 0;
  g_curDay = "";
  g_headerWritten = false;

  #if defined(PIN_SD_SCK) && defined(PIN_SD_MISO) && defined(PIN_SD_MOSI)
    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  #else
    SPI.begin();
  #endif

  if (!SD.begin(PIN_SD_CS, SPI, spiHz)) {
    Serial.println("SD init fehlgeschlagen (kein Logging).");
    return;
  }

  g_sd_ok = true;
  ensureLogDir();

  Serial.printf("SD OK: cardType=%u total=%.2fMB used=%.2fMB\n",
                SD.cardType(),
                SD.totalBytes() / 1024.0 / 1024.0,
                SD.usedBytes()  / 1024.0 / 1024.0);
}


void loggerLoop(const AppConfig& cfg, const SensorData& data) {
  static bool wasTimeValid = false;
  bool tv = timeIsValid();
  if (tv && !wasTimeValid) {
  // Zeit wurde gerade gültig -> sofort einmal schreiben
  wasTimeValid = true;
  retentionCleanup(cfg);
  appendLine(cfg, data);
  g_lastLogMs = millis();
  return;
}
if (!tv) { wasTimeValid = false; return; }

  static uint32_t dbg=0;
  if (millis() - dbg > 10000) {
    dbg = millis();
      Serial.printf("[logger] enabled=%d sd=%d interval=%u timeValid=%d\n",
        cfg.log_enabled, g_sd_ok, cfg.log_interval_min, timeIsValid());
  }

  if (!cfg.log_enabled) return;
  if (!g_sd_ok) return;

  const uint32_t intervalMs = (uint32_t)cfg.log_interval_min * 60ul * 1000ul;
  if (intervalMs == 0) return;

  if ((uint32_t)(millis() - g_lastLogMs) < intervalMs) return;
  g_lastLogMs = millis();

  retentionCleanup(cfg);
  appendLine(cfg, data);
}

void loggerForceOnce(const AppConfig& cfg, const SensorData& data) {
  if (!cfg.log_enabled) return;
  if (!g_sd_ok) return;
  if (!timeIsValid()) return;

  retentionCleanup(cfg);
  appendLine(cfg, data);

  // Intervall-Timer neu setzen, damit nicht direkt nochmal geloggt wird
  g_lastLogMs = millis();
}

void loggerRescan(){
  g_sd_ok = false;
  SD.end();
  delay(50);
  if (!SD.begin(PIN_SD_CS, SPI, spiHz)) {
    Serial.println("[logger] SD rescan failed");
    return;
  }
  g_sd_ok = true;
  ensureLogDir();
  Serial.println("[logger] SD rescan OK");
}
