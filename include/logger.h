#pragma once
#include <Arduino.h>
#include "settings.h"
#include "log_bits.h"

struct SensorData;

struct LoggerSdInfo {
  bool     ok = false;
  uint64_t total = 0;
  uint64_t used  = 0;
  uint64_t free  = 0;
};

void loggerBegin(const AppConfig& cfg);
void loggerLoop(const AppConfig& cfg, const SensorData& data);

void loggerForceOnce(const AppConfig& cfg, const SensorData& data);

void loggerRescan();   // SD neu initialisieren

bool loggerSdOk();
LoggerSdInfo loggerGetSdInfo();
uint16_t loggerCountLogDays();     // Anzahl Dateien in /log
