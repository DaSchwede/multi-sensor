#pragma once
#include <Arduino.h>
#include <WebServer.h>

// Init: registriert /wifi + /api/wifi/* auf dem übergebenen WebServer.
// apNamePrefix z.B. "Multi-Sensor" -> SSID+PASS wird "Multi-Sensor-<letzte4lower>"
void wifiMgrBegin(WebServer &server, const String &apNamePrefix);

// Muss in loop() regelmäßig aufgerufen werden
void wifiMgrLoop();

// Status
bool wifiMgrIsConnected();
bool wifiMgrPortalActive();
bool wifiMgrHasCredentials();

String wifiMgrApSsid();
String wifiMgrApPass();

// Requests (für Settings/UI)
void wifiMgrRequestStartPortal();              // Portal starten (AP+STA)
void wifiMgrRequestStartPortalKeepSta();       // Portal starten, STA nicht disconnecten (optional)
void wifiMgrRequestForget();                   // Credentials löschen + Portal
void wifiMgrRequestWifiUi(uint32_t seconds);   // /wifi im Heimnetz zeitlich erlauben

// Optional: Fallback Verhalten konfigurieren
void wifiMgrConfigureFallback(uint8_t maxFails,
                              uint32_t offlineGraceMs,
                              uint32_t retryIntervalMs);
