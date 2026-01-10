#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "settings.h"
#include "bme280_sensor.h"


void webServerBegin(WebServer &server, AppConfig &cfg, SensorData *liveData, uint32_t *lastReadMs, uint32_t *lastSendMs);
void webServerLoop(WebServer &server);
