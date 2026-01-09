#pragma once
#include <math.h>

struct SensorData {
  float temperature_c = NAN;
  float humidity_rh   = NAN;
  float pressure_hpa  = NAN;
  float co2_ppm       = NAN;   // nur CO2 neu
};
