#pragma once
#include <Arduino.h>
#include <WebServer.h>

void wifiMgrBegin(WebServer &server, const String &apNamePrefix);
void wifiMgrLoop();

bool wifiMgrIsConnected();
bool wifiMgrPortalActive();

bool wifiMgrHasCredentials();
String wifiMgrApSsid();
String wifiMgrApPass();              // WPA2-Passwort des Setup-AP (gilt, sobald WLAN-Daten gespeichert sind)

void wifiMgrRequestForget();         // Credentials löschen, AP-only starten
void wifiMgrRequestWifiUi(uint32_t seconds); // /wifi + /api/wifi/* Zeitfenster im Heimnetz
void wifiMgrRequestStartPortalKeepSta();
