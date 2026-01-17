#pragma once
#include <Arduino.h>
#include <WebServer.h>

void wifiMgrBegin(WebServer &server, const String &apNamePrefix);
void wifiMgrLoop();

bool wifiMgrIsConnected();
bool wifiMgrPortalActive();

void wifiMgrStartPortal();
void wifiMgrForget();

bool wifiMgrHasCredentials();
String wifiMgrApSsid();
void wifiMgrStartPortalManual();   // STA trennen + Portal starten

void wifiMgrRequestStartPortalManual(); // requested: STA trennen + AP starten
void wifiMgrRequestForget();            // requested: creds löschen + AP starten
String wifiMgrApSsid();
bool wifiMgrHasCredentials();

void wifiMgrRequestWifiUi(uint32_t seconds); // erlaubt /wifi kurzzeitig
void wifiMgrRequestStartPortalKeepSta();     // AP starten, STA behalten


