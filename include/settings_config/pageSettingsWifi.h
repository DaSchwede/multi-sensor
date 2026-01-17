#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>

#include "pages.h"
#include "wifi_mgr.h"
#include "settings_config/settings_common.h"

void pageSettingsWifi(WebServer &server);

bool wifiMgrHasCredentials();
String wifiMgrApSsid();
void wifiMgrStartPortalManual();   // STA trennen + Portal starten
