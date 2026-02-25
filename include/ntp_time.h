#pragma once
#include <Arduino.h>
#include "settings.h"

// Startet NTP (mit Server + TZ/DST aus cfg)
void ntpBegin(const AppConfig &cfg);

// Muss regelmäßig im loop() aufgerufen werden
void ntpLoop();

// true, wenn wir schon eine plausible Zeit haben
bool ntpIsValid();

// UTC epoch (vom NTP)
unsigned long ntpEpochUtc();

// Epoch seconds (UTC+offset bereits eingerechnet wie in NTPClient)
unsigned long ntpEpochLocal(const AppConfig &cfg);

int ntpLocalOffsetSeconds(const AppConfig &cfg, unsigned long utcEpoch);

// "HH:MM:SS"
// Strings
String ntpTimeString(const AppConfig &cfg);
String ntpDateTimeString(const AppConfig &cfg);

// neu: Status/Infos
bool ntpStarted();
const char* ntpServer();
unsigned long ntpLastSyncUtc();
uint32_t ntpLastAttemptMs();
uint32_t ntpNextTryMs();
bool ntpSyncNow();     // triggert forceUpdate sofort (wenn möglich)