#pragma once
#include "arduino.h"
#include "sensor_data.h"


// Status / Init
bool scdIsOk();
bool scdBegin();
SensorData scdRead();
