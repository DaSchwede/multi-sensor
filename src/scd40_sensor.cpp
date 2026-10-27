#include "scd40_sensor.h"
#include <Wire.h>
#include <SensirionI2CScd4x.h>
#include <math.h>

static SensirionI2CScd4x scd4x;
static bool ok = false;
static bool ascEnabled = true;
static uint32_t runningSinceMs = 0;
static float lastPressureHpa = NAN;

static constexpr uint32_t FRC_MIN_RUNTIME_MS = 3UL * 60UL * 1000UL;

#if defined(DEBUG_I2C_SCAN)
static void i2cScan() {
  Serial.println("I2C scan...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("I2C device found at 0x%02X\n", addr);
      found++;
    }
  }
  Serial.printf("I2C scan done, found %d device(s)\n", found);
}
#endif

bool scdIsOk() {
  return ok;
}

bool scdAscEnabled() {
  return ascEnabled;
}

float scdLastPressureHpa() {
  return lastPressureHpa;
}

uint32_t scdRunningMs() {
  return ok ? (millis() - runningSinceMs) : 0;
}

static bool startMeasurement() {
  const uint16_t err = scd4x.startPeriodicMeasurement();
  if (err) {
    Serial.printf("SCD4x startPeriodicMeasurement failed: %u\n", err);
    return false;
  }
  runningSinceMs = millis();
  // Druckwert nach Neustart erneut senden (wird beim nächsten Durchlauf gemacht)
  lastPressureHpa = NAN;
  return true;
}

static void stopMeasurement() {
  scd4x.stopPeriodicMeasurement();
  delay(500);   // Datenblatt: 500 ms bis der Sensor Befehle im Idle annimmt
}

bool scdBegin() {
  delay(10);

#if defined(DEBUG_I2C_SCAN)
  i2cScan();
#endif

  scd4x.begin(Wire);

  // Falls schon aktiv (Warmstart), stoppen (Fehler ignorieren)
  stopMeasurement();

  uint16_t sn0 = 0, sn1 = 0, sn2 = 0;
  uint16_t err = scd4x.getSerialNumber(sn0, sn1, sn2);
  if (err) {
    Serial.printf("SCD4x getSerialNumber failed: %u\n", err);
    ok = false;
    return false;
  }
  Serial.printf("SCD4x Serial: %04X-%04X-%04X\n", sn0, sn1, sn2);

  err = scd4x.setAutomaticSelfCalibration(ascEnabled ? 1 : 0);
  if (err) Serial.printf("SCD4x setAutomaticSelfCalibration failed: %u\n", err);
  else Serial.printf("SCD4x ASC %s\n", ascEnabled ? "an" : "aus");

  if (!startMeasurement()) {
    ok = false;
    return false;
  }

  Serial.println("SCD4x periodic measurement started.");
  ok = true;
  return true;
}

bool scdSetAsc(bool enabled) {
  ascEnabled = enabled;
  if (!ok) return true;   // wird beim nächsten scdBegin() gesetzt

  stopMeasurement();
  const uint16_t err = scd4x.setAutomaticSelfCalibration(enabled ? 1 : 0);
  const bool started = startMeasurement();
  if (err) Serial.printf("SCD4x setAutomaticSelfCalibration failed: %u\n", err);
  if (!started) ok = false;
  return err == 0 && started;
}

bool scdSetAmbientPressure(float hpa) {
  if (!ok || isnan(hpa) || hpa < 700.0f || hpa > 1200.0f) return false;
  const uint16_t err = scd4x.setAmbientPressure((uint16_t)lroundf(hpa));
  if (err) return false;
  lastPressureHpa = hpa;
  return true;
}

int scdForcedRecalibration(uint16_t targetPpm, int &correctionPpm) {
  correctionPpm = 0;
  if (!ok) return -1;
  if (millis() - runningSinceMs < FRC_MIN_RUNTIME_MS) return -2;

  stopMeasurement();
  uint16_t frc = 0xFFFF;
  const uint16_t err = scd4x.performForcedRecalibration(targetPpm, frc);
  const bool started = startMeasurement();
  if (!started) ok = false;

  if (err || frc == 0xFFFF) {
    Serial.printf("SCD4x FRC fehlgeschlagen (err=%u, frc=0x%04X)\n", err, frc);
    return -3;
  }
  correctionPpm = (int)frc - 0x8000;
  Serial.printf("SCD4x FRC OK: Ziel %u ppm, Korrektur %+d ppm\n", targetPpm, correctionPpm);
  return 0;
}

SensorData scdRead() {
  SensorData d;
  if (!ok) return d;

  bool ready = false;
  uint16_t err = scd4x.getDataReadyFlag(ready);
  if (err || !ready) return d;

  uint16_t co2 = 0;
  float t = 0.0f, rh = 0.0f;   // müssen mitgelesen werden, auch wenn wir sie nicht nutzen
  err = scd4x.readMeasurement(co2, t, rh);
  if (err || co2 == 0) return d;

  d.co2_ppm = (float)co2;
  return d;
}
