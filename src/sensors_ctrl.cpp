#include "sensors_ctrl.h"
#include <Wire.h>
#include "pins.h"
#include "bme280_sensor.h"
#include "scd40_sensor.h"
#include <math.h>

static volatile bool gReq = false;

void requestSensorRescan() { gReq = true; }

bool consumeSensorRescanRequest() {
  if (!gReq) return false;
  gReq = false;
  return true;
}

void sensorsRescanNow() {
  Serial.println("Rescan: I2C/Sensoren neu initialisieren...");

  Wire.end();
  delay(50);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  delay(50);

  bool okB = bmeBegin();
  bool okS = scdBegin();

  if (!okB) Serial.println("Rescan: BME280 weiterhin nicht gefunden.");
  if (!okS) Serial.println("Rescan: SCD40/41 weiterhin nicht gefunden.");
}
