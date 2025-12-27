#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>
#include "settings.h"
#include "bme280_sensor.h"

void loxoneSendUDP(const AppConfig &cfg, const SensorData &d);
