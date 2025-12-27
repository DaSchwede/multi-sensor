#pragma once
#include <Arduino.h>

struct SensorData {
  float temperature_c = NAN;
  float humidity_rh = NAN;
  float pressure_hpa = NAN;
};

bool bmeIsOk();
bool bmeBegin();
SensorData bmeRead();
