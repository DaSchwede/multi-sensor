#include "ntp_time.h"
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"

// Zeit kommt jetzt vom SNTP-Client des ESP-IDF (configTzTime). Vorteile:
//  - nicht blockierend (vorher konnte NTPClient den Loop bis zu 1 s anhalten)
//  - Systemzeit + Zeitzone gelten überall: localtime(), Logger, Diagramme
//  - Sommerzeit über POSIX-TZ-Regel statt eigener Berechnung

static bool started = false;
static volatile time_t lastSyncUtc = 0;   // Zeitpunkt der letzten erfolgreichen NTP-Synchronisation

// wird vom SNTP-Dienst nach jeder Synchronisation aufgerufen
static void onTimeSync(struct timeval *tv) {
  lastSyncUtc = tv ? tv->tv_sec : time(nullptr);
}
static String gTz;

// SNTP merkt sich nur den Zeiger auf den Servernamen -> eigener, dauerhafter Puffer.
// Zwei Puffer im Wechsel, damit der alte gültig bleibt, bis configTzTime den neuen setzt.
static char gServerBuf[2][64];
static int  gServerIdx = 0;

// POSIX: Offset mit umgekehrtem Vorzeichen, z.B. UTC+1 -> "-1"
static String posixOffset(int secs) {
  int v = -secs;
  char sign = v < 0 ? '-' : '+';
  if (v < 0) v = -v;
  const int h = v / 3600, m = (v % 3600) / 60;
  char buf[12];
  if (m) snprintf(buf, sizeof(buf), "%c%d:%02d", sign, h, m);
  else   snprintf(buf, sizeof(buf), "%c%d", sign, h);
  return String(buf);
}

static String clockTime(int secs) {
  secs = ((secs % 86400) + 86400) % 86400;
  char buf[8];
  snprintf(buf, sizeof(buf), "%d:%02d", secs / 3600, (secs % 3600) / 60);
  return String(buf);
}

String ntpPosixTz(const AppConfig &cfg) {
  const int base = cfg.tz_base_seconds;
  if (cfg.tz_auto_berlin) {
    // EU-Regel: Umstellung am letzten Sonntag im März/Oktober um 01:00 UTC.
    // Mit base=3600, dst=3600 ergibt das "STD-1DST-2,M3.5.0/2:00,M10.5.0/3:00" (= Europe/Berlin)
    const int dst = cfg.dst_add_seconds;
    if (dst == 0) return "STD" + posixOffset(base);
    return "STD" + posixOffset(base) + "DST" + posixOffset(base + dst) +
           ",M3.5.0/" + clockTime(3600 + base) +
           ",M10.5.0/" + clockTime(3600 + base + dst);
  }
  // fester Offset: Basis + (dauerhafter) Zuschlag, kein Wechsel
  return "STD" + posixOffset(base + cfg.dst_add_seconds);
}

void ntpApplyTimezone(const AppConfig &cfg) {
  gTz = ntpPosixTz(cfg);
  setenv("TZ", gTz.c_str(), 1);
  tzset();
}

void ntpBegin(const AppConfig &cfg) {
  String server = cfg.ntp_server;
  server.trim();
  if (server.length() == 0) server = "pool.ntp.org";

  gServerIdx ^= 1;
  strlcpy(gServerBuf[gServerIdx], server.c_str(), sizeof(gServerBuf[gServerIdx]));

  gTz = ntpPosixTz(cfg);
  sntp_set_time_sync_notification_cb(onTimeSync);
  configTzTime(gTz.c_str(), gServerBuf[gServerIdx]);
  started = true;
  Serial.printf("NTP: Server=%s TZ=%s\n", gServerBuf[gServerIdx], gTz.c_str());
}

void ntpLoop() {
  // nichts zu tun – SNTP läuft im Hintergrund
}

bool ntpIsValid() {
  return time(nullptr) > 1672531200; // > 2023-01-01
}

unsigned long ntpEpochUtc() {
  if (!ntpIsValid()) return 0;
  return (unsigned long)time(nullptr);
}

int ntpLocalOffsetSeconds(const AppConfig &cfg, unsigned long utcEpoch) {
  (void)cfg;
  const time_t t = (time_t)utcEpoch;
  struct tm lt {}, gt {};
  localtime_r(&t, &lt);
  gmtime_r(&t, &gt);
  int diff = (lt.tm_hour - gt.tm_hour) * 3600 + (lt.tm_min - gt.tm_min) * 60 + (lt.tm_sec - gt.tm_sec);
  if (lt.tm_year != gt.tm_year || lt.tm_yday != gt.tm_yday) {
    const bool localAhead = (lt.tm_year > gt.tm_year) ||
                            (lt.tm_year == gt.tm_year && lt.tm_yday > gt.tm_yday);
    diff += localAhead ? 86400 : -86400;
  }
  return diff;
}

unsigned long ntpEpochLocal(const AppConfig &cfg) {
  const unsigned long utc = ntpEpochUtc();
  if (utc == 0) return 0;
  return utc + (long)ntpLocalOffsetSeconds(cfg, utc);
}

bool ntpDstActive() {
  const time_t t = time(nullptr);
  struct tm lt {};
  localtime_r(&t, &lt);
  return lt.tm_isdst > 0;
}

static String fmtLocal(const char *fmt) {
  if (!ntpIsValid()) return "—";
  const time_t t = time(nullptr);
  struct tm lt {};
  localtime_r(&t, &lt);
  char buf[32];
  strftime(buf, sizeof(buf), fmt, &lt);
  return String(buf);
}

String ntpTimeString(const AppConfig &cfg) {
  (void)cfg;
  return fmtLocal("%H:%M:%S");
}

String ntpDateTimeString(const AppConfig &cfg) {
  (void)cfg;
  return fmtLocal("%Y-%m-%d %H:%M:%S");
}

bool ntpStarted() { return started; }

const char* ntpServer() { return started ? gServerBuf[gServerIdx] : ""; }

unsigned long ntpLastSyncUtc() { return (unsigned long)lastSyncUtc; }

bool ntpSyncNow() {
  if (!started || WiFi.status() != WL_CONNECTED) return false;
  sntp_restart();   // neue Anfrage an den Server sofort auslösen
  return true;
}
