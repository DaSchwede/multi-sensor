#pragma once
#include <Arduino.h>
#include "settings.h"

// Setzt nur die Zeitzone (TZ/tzset) aus cfg – ohne Netzwerk, z.B. direkt beim Boot
void ntpApplyTimezone(const AppConfig &cfg);

// Startet SNTP (ESP-IDF, läuft im Hintergrund) mit Server + Zeitzone aus cfg.
// Darf nach Einstellungsänderungen erneut aufgerufen werden.
void ntpBegin(const AppConfig &cfg);

// Für Kompatibilität – SNTP synchronisiert selbstständig im Hintergrund
void ntpLoop();

// true, wenn die Systemzeit plausibel ist (> 2023-01-01)
bool ntpIsValid();

// UTC epoch (0 wenn noch keine Zeit)
unsigned long ntpEpochUtc();

// Epoch inkl. lokalem Offset (0 wenn noch keine Zeit)
unsigned long ntpEpochLocal(const AppConfig &cfg);

// Strings in lokaler Zeit
String ntpTimeString(const AppConfig &cfg);       // "HH:MM:SS"
String ntpDateTimeString(const AppConfig &cfg);   // "YYYY-MM-DD HH:MM:SS"

// Info
bool ntpDstActive();                                    // Sommerzeit gerade aktiv?
int  ntpLocalOffsetSeconds(const AppConfig &cfg, unsigned long utcEpoch);
String ntpPosixTz(const AppConfig &cfg);                // verwendeter POSIX-TZ-String
