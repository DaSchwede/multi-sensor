#pragma once
#include <Arduino.h>
#include "sensor_data.h"

// Status / Init
bool scdIsOk();
bool scdBegin();                 // nutzt die zuletzt gesetzte ASC-Einstellung
SensorData scdRead();

// Automatische Selbstkalibrierung (ASC).
// Vor scdBegin(): nur merken. Bei laufendem Sensor: Messung kurz stoppen, setzen, neu starten (~1 s).
bool scdSetAsc(bool enabled);
bool scdAscEnabled();

// Luftdruck-Kompensation (hPa), darf während der Messung gesendet werden
bool scdSetAmbientPressure(float hpa);
float scdLastPressureHpa();      // NAN, wenn noch nie gesendet

// Wie lange misst der Sensor schon ununterbrochen? (für Kalibrierung >= 3 min nötig)
uint32_t scdRunningMs();

// Forced Recalibration auf einen Referenzwert (z.B. 420 ppm an Frischluft).
// Rückgabe: 0 = OK (correctionPpm gesetzt), -1 = Sensor fehlt, -2 = läuft noch keine 3 min,
//           -3 = Sensor meldet Fehler
int scdForcedRecalibration(uint16_t targetPpm, int &correctionPpm);
