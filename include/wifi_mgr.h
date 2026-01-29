#pragma once
#include <Arduino.h>
#include <WebServer.h>

void wifiMgrBegin(WebServer &server, const String &apNamePrefix);
void wifiMgrLoop();

bool wifiMgrIsConnected();
bool wifiMgrPortalActive();

bool wifiMgrHasCredentials();
String wifiMgrApSsid();
String wifiMgrApPass();              // <— NEU: Setup-AP Passwort (WPA2)

void wifiMgrRequestStartPortal();    // aus Settings: AP+STA starten, /wifi öffnen
void wifiMgrRequestForget();         // Credentials löschen, AP-only starten
void wifiMgrRequestWifiUi(uint32_t seconds); // /wifi Zeitfenster im Heimnetz
void wifiMgrRequestStartPortalKeepSta();




