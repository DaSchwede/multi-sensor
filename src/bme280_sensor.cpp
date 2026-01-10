#include "bme280_sensor.h"
#include <Wire.h>
#include <Adafruit_BME280.h>

static Adafruit_BME280 bme;
static bool ok = false;

// ===== I2C Scan MUSS VOR bmeBegin() stehen =====
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

static uint8_t readChipId(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(0xD0); // Chip-ID Register
  if (Wire.endTransmission(false) != 0) return 0xFF;

  Wire.requestFrom(addr, (uint8_t)1);
  if (Wire.available()) return Wire.read();
  return 0xFF;
}

bool bmeIsOk() {
  return ok;
}

bool bmeBegin() {
  delay(10);

#if defined(DEBUG_I2C_SCAN)
  i2cScan();
#endif

  uint8_t id76 = readChipId(0x76);
  Serial.printf("ChipID @0x76 = 0x%02X\n", id76);

  if (id76 == 0x60) Serial.println("Das ist ein BME280.");
  if (id76 == 0x58 || id76 == 0x56 || id76 == 0x57) Serial.println("Das ist sehr wahrscheinlich ein BMP280.");

  if (bme.begin(0x76, &Wire)) { ok = true; return true; }
  if (bme.begin(0x77, &Wire)) { ok = true; return true; }

  ok = false;
  return false;
}


SensorData bmeRead() {
  SensorData d;
  if (!ok) return d;

  d.temperature_c = bme.readTemperature();
  d.humidity_rh = bme.readHumidity();
  d.pressure_hpa = bme.readPressure() / 100.0f;
  return d;
}


