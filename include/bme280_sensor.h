#pragma once
#include <Arduino.h>
#include "sensor_data.h"


bool bmeIsOk();
bool bmeBegin();
SensorData bmeRead();
