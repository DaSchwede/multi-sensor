#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "settings.h"
#include "bme280_sensor.h"


void webServerBegin(ESP8266WebServer &server, AppConfig &cfg, SensorData *liveData, uint32_t *lastReadMs, uint32_t *lastSendMs);
void webServerLoop(ESP8266WebServer &server);
