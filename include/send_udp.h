#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>
#include "settings.h"
#include "bme280_sensor.h"
#include "sensor_data.h"


void SendUDP(const AppConfig &cfg, const SensorData &d);
