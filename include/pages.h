#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "settings.h"
#include "bme280_sensor.h"
#include "sensor_data.h"


// Init: setzt interne Pointer (cfg/live/zeiten)
void pagesInit(AppConfig &cfg, SensorData *liveData, uint32_t *lastReadMs, uint32_t *lastSendMs);

// Pages
void pageRoot(ESP8266WebServer &server);
void pageInfo(ESP8266WebServer &server);
void pageSettings(ESP8266WebServer &server);
void pageAbout(ESP8266WebServer &server);
void pageLicense(ESP8266WebServer &server);

// Backup/Restore/Reset
void pageBackup(ESP8266WebServer &server);
void pageRestoreForm(ESP8266WebServer &server);
void pageRestoreUpload(ESP8266WebServer &server);
void pageFactoryResetForm(ESP8266WebServer &server);
void pageFactoryResetDo(ESP8266WebServer &server);

// OTA
void pageOtaForm(ESP8266WebServer &server);        // Sicherheitsabfrage + Admin-Passwort
void pageOtaPrepare(ESP8266WebServer &server);     // prüft & "armed" Token
void pageOtaUploadForm(ESP8266WebServer &server);  // Upload-Form mit Progressbar
void pageOtaUpload(ESP8266WebServer &server);      // Upload-Handler

// API
void apiLive(ESP8266WebServer &server);

// ===== Shared helpers (werden in pages.cpp definiert, von Subpages genutzt) =====
AppConfig* pagesCfg();
SensorData* pagesLive();
uint32_t pagesLastReadMs();
uint32_t pagesLastSendMs();

String pagesHeaderAuth(const String &title, const String &currentPath);                // requires auth menu
String pagesHeaderPublic(ESP8266WebServer &server, const String &title, const String &currentPath);
String pagesFooter();

String pagesUptimeString();
String pagesLoadLicenseText();
std::vector<String> pagesSplitCsv(const String &csv);

// Cards (wenn du sie in mehreren Seiten nutzen willst)
String cardSystem();
String cardNetzwerk();
String cardSensor();
String cardSpeicher();
String cardZeit();
String cardAktuelleEinstellungen();
