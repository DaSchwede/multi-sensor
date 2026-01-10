#include "scd40_sensor.h"
#include <Wire.h>
#include <SensirionI2CScd4x.h>

static SensirionI2CScd4x scd4x;
static bool ok = false;

// ===== optional wie bei dir =====
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

bool scdIsOk() {
  return ok;
}

bool scdBegin() {
  delay(10);

  #if defined(DEBUG_I2C_SCAN)
    i2cScan();
  #endif

  scd4x.begin(Wire);

  // Falls schon aktiv (Warmstart), stoppen (Fehler ignorieren)
  scd4x.stopPeriodicMeasurement();
  delay(200);

  // Seriennummer als Check
  uint16_t sn0 = 0, sn1 = 0, sn2 = 0;
  uint16_t err = scd4x.getSerialNumber(sn0, sn1, sn2);
  if (err) {
    Serial.print("SCD4x getSerialNumber failed: ");
    Serial.println(err);
    ok = false;
    return false;
  }
  Serial.printf("SCD4x Serial: %04X-%04X-%04X\n", sn0, sn1, sn2);

  // Messung starten (alle ~5s neue Daten)
  err = scd4x.startPeriodicMeasurement();
  if (err) {
    Serial.print("SCD4x startPeriodicMeasurement failed: ");
    Serial.println(err);
    ok = false;
    return false;
  }

  Serial.println("SCD4x periodic measurement started.");
  ok = true;
  return true;
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

  // Nur CO2 setzen – Temp/Feuchte ignorieren
  d.co2_ppm = (float)co2;
  return d;
}
