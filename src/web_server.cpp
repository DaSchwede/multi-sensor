#include "web_server.h"
#include <LittleFS.h>
#include "auth.h"
#include "pages.h"
#include "sensors_ctrl.h"
#include "settings_config/settings_common.h"
#include "logger.h"

void webServerBegin(WebServer &server,
                    AppConfig &cfg,
                    SensorData *liveData,
                    uint32_t *lastReadMs,
                    uint32_t *lastSendMs) {

  Serial.println("style.css exists? " + String(LittleFS.exists("/style.css")));

  // pages bekommt Zugriff auf cfg/live/zeiten
  pagesInit(cfg, liveData, lastReadMs, lastSendMs);

  // Statische Dateien
  server.serveStatic("/style.css", LittleFS, "/style.css");
  server.serveStatic("/script.js", LittleFS, "/script.js");
  server.serveStatic("/logger.js", LittleFS, "/logger.js");
  server.serveStatic("/favicon.svg", LittleFS, "/favicon.svg");
  server.serveStatic("/logo_name_weiss.svg", LittleFS, "/logo_name_weiss.svg");
  server.serveStatic("/logo_name_weiss_gruen.svg", LittleFS, "/logo_name_weiss_gruen.svg");
  server.serveStatic("/logo_name_gruen.svg", LittleFS, "/logo_name_gruen.svg");

      // Cookie-Header lesen können
  static const char* headerKeys[] = { "Cookie" };
  server.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys[0]));


  // Auth-Routen (/login, /force_pw, /logout)
  authAttach(server, cfg);

  server.on("/action/rescan_sensors", HTTP_POST, [&](){
    // Auth check wie bei settings-Seiten
    AppConfig* c = settingsRequireCfgAndAuth(server);
    if (!c) return;

    // Rescan anstoßen
    extern void requestSensorRescan(); // falls in main.cpp static ist: siehe Hinweis unten
    requestSensorRescan();

    // Redirect zurück (z.B. Tools)
    server.sendHeader("Location", "/settings/tools?msg=rescan", true);
    server.send(302, "text/plain", "");
  });
  
  server.on("/action/logger_rescan", HTTP_POST, [&](){
  AppConfig* c = settingsRequireCfgAndAuth(server);
  if (!c) return;

  loggerRescan();

  server.sendHeader("Location", "/settings/logger?msg=rescan", true);
  server.send(302, "text/plain", "");
  });


  server.on("/license", HTTP_GET, [&](){ pageLicense(server); });

  // API
  server.on("/api/live", HTTP_GET, [&](){ apiLive(server); });
  server.on("/api/history", HTTP_GET, [&](){ apiHistory(server); });

  // Seiten
  server.on("/", HTTP_GET,        [&](){ pageRoot(server); });
  server.on("/info", HTTP_GET,    [&](){ pageInfo(server); });

  server.on("/logger", HTTP_GET,  [&](){ pageLogger(server); });

  server.on("/info/system", HTTP_GET, [&](){ pageSystemInfo(server); });


  server.on("/settings", HTTP_GET, [&](){
  server.sendHeader("Location", "/settings/udp", true);
  server.send(302, "text/plain", "");
  });

  server.on("/settings/udp",   HTTP_GET, [&](){ pageSettingsUdp(server); });
  server.on("/settings/udp",   HTTP_POST, [&](){ pageSettingsUdp(server); });

  server.on("/settings/time",  HTTP_GET, [&](){ pageSettingsTime(server); });
  server.on("/settings/time",  HTTP_POST, [&](){ pageSettingsTime(server); });
  
  server.on("/settings/mqtt",  HTTP_GET, [&](){ pageSettingsMqtt(server); });
  server.on("/settings/mqtt",  HTTP_POST, [&](){ pageSettingsMqtt(server); });

  server.on("/settings/logger", HTTP_GET, [&](){ pageSettingsLogger(server); });
  server.on("/settings/logger", HTTP_POST, [&](){ pageSettingsLogger(server); });

  server.on("/settings/ui",    HTTP_GET, [&](){ pageSettingsUi(server); });
  server.on("/settings/ui",    HTTP_POST, [&](){ pageSettingsUi(server); });

  server.on("/settings/wifi",  HTTP_GET, [&](){ pageSettingsWifi(server); });
  server.on("/settings/wifi",  HTTP_POST, [&](){ pageSettingsWifi(server); });

  server.on("/settings/tools", HTTP_GET, [&](){ pageSettingsTools(server); });

  server.on("/about", HTTP_GET,   [&](){ pageAbout(server); });
  server.on("/backup", HTTP_GET,  [&](){ pageBackup(server); });

  // Restore Upload (GET Form + POST Upload)
  server.on("/restore", HTTP_GET, [&](){ pageRestoreForm(server); });
  server.on("/restore", HTTP_POST,
    [&](){ /* Antwort kommt im Upload-End */ },
    [&](){ pageRestoreUpload(server); }
  );

  server.on("/ota", HTTP_GET,  [&](){ pageOtaForm(server); });
  server.on("/ota_prepare", HTTP_POST, [&](){ pageOtaPrepare(server); });

  server.on("/ota_upload", HTTP_GET,  [&](){ pageOtaUploadForm(server); });

  server.on("/ota_upload", HTTP_POST,
    [&](){ /* Antwort kommt im Upload END */ },
    [&](){ pageOtaUpload(server); }
  );

  server.on("/factory_reset", HTTP_GET,  [&](){ pageFactoryResetForm(server); });
  server.on("/factory_reset", HTTP_POST, [&](){ pageFactoryResetDo(server); });


  server.begin();
}

void webServerLoop(WebServer &server) {
  server.handleClient();
}
