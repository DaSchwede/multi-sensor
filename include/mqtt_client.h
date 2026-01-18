#pragma once
#include "settings.h"
#include "sensor_data.h"

void mqttBegin(const AppConfig &cfg);
void mqttLoop(const AppConfig &cfg);
bool mqttIsConnected();
bool mqttEnsureConnected(const AppConfig &cfg);
bool mqttPublish(const AppConfig &cfg,
                 const String &subtopic,
                 const String &payload,
                 bool retainOverride = false);
