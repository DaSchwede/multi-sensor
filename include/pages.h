#pragma once
#include <ESP8266WebServer.h>
#include "settings.h"
#include "bme280_sensor.h"

// Init (setzt interne Pointer)
void pagesInit(AppConfig &cfg,
               SensorData *liveData,
               uint32_t *lastReadMs,
               uint32_t *lastSendMs);

// Page-Handler
void pageRoot(ESP8266WebServer &server);
void pageInfo(ESP8266WebServer &server);
void pageSettings(ESP8266WebServer &server);
void pageAbout(ESP8266WebServer &server);
void pageBackup(ESP8266WebServer &server);
void pageRestoreForm(ESP8266WebServer &server);
void pageRestoreUpload(ESP8266WebServer &server);
void pageFactoryResetForm(ESP8266WebServer &server);
void pageFactoryResetDo(ESP8266WebServer &server);

// API
void apiLive(ESP8266WebServer &server);
