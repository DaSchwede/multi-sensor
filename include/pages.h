#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "settings.h"
#include "bme280_sensor.h"
#include "sensor_data.h"


// Init: setzt interne Pointer (cfg/live/zeiten)
void pagesInit(AppConfig &cfg, SensorData *liveData, uint32_t *lastReadMs, uint32_t *lastSendMs);

// Pages
void pageRoot(WebServer &server);
void pageInfo(WebServer &server);
void pageSettings(WebServer &server);
void pageAbout(WebServer &server);
void pageLicense(WebServer &server);

// Backup/Restore/Reset
void pageBackup(WebServer &server);
void pageRestoreForm(WebServer &server);
void pageRestoreUpload(WebServer &server);
void pageFactoryResetForm(WebServer &server);
void pageFactoryResetDo(WebServer &server);

// settings pages
void pageSettingsUdp(WebServer &server);
void pageSettingsTime(WebServer &server);
void pageSettingsMqtt(WebServer &server);
void pageSettingsUi(WebServer &server);
void pageSettingsTools(WebServer &server);

// OTA
void pageOtaForm(WebServer &server);        // Sicherheitsabfrage + Admin-Passwort
void pageOtaPrepare(WebServer &server);     // prüft & "armed" Token
void pageOtaUploadForm(WebServer &server);  // Upload-Form mit Progressbar
void pageOtaUpload(WebServer &server);      // Upload-Handler

// API
void apiLive(WebServer &server);

// ===== Shared helpers (werden in pages.cpp definiert, von Subpages genutzt) =====
AppConfig* pagesCfg();
SensorData* pagesLive();
uint32_t pagesLastReadMs();
uint32_t pagesLastSendMs();

String pagesHeaderAuth(const String &title, const String &currentPath);                // requires auth menu
String pagesHeaderPublic(WebServer &server, const String &title, const String &currentPath);
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
