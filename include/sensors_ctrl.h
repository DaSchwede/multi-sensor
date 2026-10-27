#pragma once
#include <Arduino.h>

// Rescan anfordern (wird im loop() ausgeführt)
void requestSensorRescan();

// Zustand der Sensoren (für Weboberfläche)
struct SensorStatus {
  bool     bmeFound = false;
  bool     scdFound = false;
  int32_t  bmeAgeMs = -1;        // Alter des letzten gültigen Werts, -1 = noch nie
  int32_t  scdAgeMs = -1;
  float    bmeRawTempC = NAN;    // Temperatur ohne Offset
  float    bmeRawHum   = NAN;    // Feuchte ohne Korrektur
  uint32_t scdRunningMs = 0;     // seit wann der SCD40 ununterbrochen misst
  float    scdPressureHpa = NAN; // zuletzt an den SCD40 übergebener Luftdruck
};
SensorStatus sensorsGetStatus();

// Werte gelten nach dieser Zeit ohne neue gültige Messung als veraltet (-> NaN)
static constexpr uint32_t SENSOR_STALE_MS = 30000;
