#include "ntp_time.h"
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

static WiFiUDP ntpUDP;
static NTPClient ntp(ntpUDP);

static bool started = false;
static bool valid = false;

static bool isLeap(int y) {
  return ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
}

// UTC epoch -> Y/M/D/h/m/s (UTC)
static void epochToDateTime(unsigned long t, int &Y, int &M, int &D, int &h, int &m, int &s) {
  s = t % 60; t /= 60;
  m = t % 60; t /= 60;
  h = t % 24; t /= 24;
  unsigned long days = t;

  int year = 1970;
  while (true) {
    unsigned long diy = isLeap(year) ? 366 : 365;
    if (days >= diy) { days -= diy; year++; } else break;
  }

  static const int mdaysNorm[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  int month = 0;
  while (true) {
    int dim = mdaysNorm[month];
    if (month == 1 && isLeap(year)) dim = 29;
    if (days >= (unsigned long)dim) { days -= dim; month++; } else break;
  }

  Y = year;
  M = month + 1;
  D = (int)days + 1;
}

// Wochentag: 0=So..6=Sa (UTC)
static int weekdayFromEpoch(unsigned long t) {
  // 1970-01-01 war Donnerstag (4) wenn 0=So => Do=4
  unsigned long days = t / 86400UL;
  return (int)((days + 4) % 7);
}

// Letzter Sonntag eines Monats (UTC, 00:00) als Tag im Monat
static int lastSundayOfMonth(int year, int month) {
  // finde Tage im Monat
  int dim;
  switch(month) {
    case 1: dim = 31; break;
    case 2: dim = isLeap(year) ? 29 : 28; break;
    case 3: dim = 31; break;
    case 4: dim = 30; break;
    case 5: dim = 31; break;
    case 6: dim = 30; break;
    case 7: dim = 31; break;
    case 8: dim = 31; break;
    case 9: dim = 30; break;
    case 10: dim = 31; break;
    case 11: dim = 30; break;
    case 12: dim = 31; break;
    default: dim = 30; break;
  }

  // wir brauchen Wochentag vom letzten Tag
  // dafür bauen wir epoch für year-month-dim 00:00 UTC über Vorwärtsrechnung:
  // -> wir nutzen epochToDateTime umgekehrt nicht; daher machen wir’s simpel:
  // wir iterieren ab 1970 hoch (für ESP ok, nur 1x pro loop wäre doof; hier nur beim DST check selten genutzt)
  // ABER wir optimieren: DST check nutzt nur aktuelles Jahr/Monat, Aufruf ist selten, ok.

  // Hilfsfunktion: epoch für year-month-day 00:00 UTC
  auto epochAt = [&](int Y, int Mo, int Day)->unsigned long {
    // Tage seit 1970-01-01 bis Y-Mo-Day
    unsigned long days = 0;
    for (int y=1970; y<Y; y++) days += isLeap(y)?366:365;
    static const int mdaysNorm2[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    for (int m=1; m<Mo; m++) {
      int d = mdaysNorm2[m-1];
      if (m==2 && isLeap(Y)) d = 29;
      days += d;
    }
    days += (unsigned long)(Day - 1);
    return days * 86400UL;
  };

  unsigned long e = epochAt(year, month, dim);
  int wd = weekdayFromEpoch(e); // 0=So..6=Sa
  int lastSunday = dim - wd;    // wenn letzter Tag Sonntag (wd=0) => dim
  return lastSunday;
}

// Europe/Berlin DST: letzte So im März 01:00 UTC -> letzte So im Okt 01:00 UTC
bool ntpDstActiveBerlin(unsigned long utcEpoch) {
  int Y,M,D,h,m,s;
  epochToDateTime(utcEpoch, Y,M,D,h,m,s);

  if (M < 3 || M > 10) return false;
  if (M > 3 && M < 10) return true;

  int startDay = lastSundayOfMonth(Y, 3);
  int endDay   = lastSundayOfMonth(Y, 10);

  // Start: März, letzter Sonntag 01:00 UTC
  if (M == 3) {
    if (D > startDay) return true;
    if (D < startDay) return false;
    return (h >= 1);
  }

  // Ende: Oktober, letzter Sonntag 01:00 UTC
  if (M == 10) {
    if (D < endDay) return true;
    if (D > endDay) return false;
    return (h < 1);
  }

  return false;
}

int ntpLocalOffsetSeconds(const AppConfig &cfg, unsigned long utcEpoch) {
  if (cfg.tz_auto_berlin) {
    int off = cfg.tz_base_seconds;
    if (ntpDstActiveBerlin(utcEpoch)) off += cfg.dst_add_seconds;
    return off;
  }
  // manuell: base + dst_add (wenn du willst kannst du hier dst_add_seconds als “immer an” interpretieren)
  return cfg.tz_base_seconds; // simpel: nur base
}

void ntpBegin(const AppConfig &cfg) {
  ntp.end();
  ntp = NTPClient(ntpUDP, cfg.ntp_server.c_str(), 0, 60 * 1000); // UTC!
  ntp.begin();
  started = true;
  valid = false;
}

void ntpLoop() {
  if (!started) return;
  if (WiFi.status() != WL_CONNECTED) return;

  if (!valid) {
    if (ntp.forceUpdate()) valid = true;
  } else {
    ntp.update();
  }
}

bool ntpIsValid() {
  if (!started) return false;
  unsigned long e = ntp.getEpochTime();
  return valid && e > 1577836800UL; // > 2020-01-01
}

unsigned long ntpEpochUtc() {
  if (!ntpIsValid()) return 0;
  return ntp.getEpochTime();
}

unsigned long ntpEpochLocal(const AppConfig &cfg) {
  if (!ntpIsValid()) return 0;
  unsigned long utc = ntpEpochUtc();
  return utc + (unsigned long)ntpLocalOffsetSeconds(cfg, utc);
}

static String fmtDateTime(unsigned long epoch) {
  int Y,M,D,h,m,s;
  epochToDateTime(epoch, Y,M,D,h,m,s);
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", Y,M,D,h,m,s);
  return String(buf);
}

String ntpTimeString(const AppConfig &cfg) {
  unsigned long loc = ntpEpochLocal(cfg);
  if (loc == 0) return "—";
  int Y,M,D,h,m,s;
  epochToDateTime(loc, Y,M,D,h,m,s);
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h,m,s);
  return String(buf);
}

String ntpDateTimeString(const AppConfig &cfg) {
  unsigned long loc = ntpEpochLocal(cfg);
  if (loc == 0) return "—";
  return fmtDateTime(loc);
}
