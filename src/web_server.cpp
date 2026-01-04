#include "web_server.h"
#include <LittleFS.h>
#include "auth.h"
#include "pages.h"

void webServerBegin(ESP8266WebServer &server,
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
  server.serveStatic("/favicon.svg", LittleFS, "/favicon.svg");
  server.serveStatic("/logo_name_weiss.svg", LittleFS, "/logo_name_weiss.svg");
  server.serveStatic("/logo_name_weiss_gruen.svg", LittleFS, "/logo_name_weiss_gruen.svg");
  server.serveStatic("/logo_name_gruen.svg", LittleFS, "/logo_name_gruen.svg");

  // Auth-Routen (/login, /force_pw, /logout)
  authAttach(server, cfg);

  server.on("/license", HTTP_GET, [&](){ pageLicense(server); });

  // API
  server.on("/api/live", HTTP_GET, [&](){ apiLive(server); });

  // Seiten
  server.on("/", HTTP_GET,        [&](){ pageRoot(server); });
  server.on("/info", HTTP_GET,    [&](){ pageInfo(server); });

  // ✅ settings GET + POST müssen beide pageSettings sein
  server.on("/settings", HTTP_GET,  [&](){ pageSettings(server); });
  server.on("/settings", HTTP_POST, [&](){ pageSettings(server); });

  server.on("/about", HTTP_GET,   [&](){ pageAbout(server); });
  server.on("/backup", HTTP_GET,  [&](){ pageBackup(server); });

  // Restore Upload (GET Form + POST Upload)
  server.on("/restore", HTTP_GET, [&](){ pageRestoreForm(server); });
  server.on("/restore", HTTP_POST,
    [&](){ /* Antwort kommt im Upload-End */ },
    [&](){ pageRestoreUpload(server); }
  );

  server.on("/factory_reset", HTTP_GET,  [&](){ pageFactoryResetForm(server); });
  server.on("/factory_reset", HTTP_POST, [&](){ pageFactoryResetDo(server); });

  // Cookie-Header lesen können
  server.collectHeaders("Cookie");
  server.begin();
}

void webServerLoop(ESP8266WebServer &server) {
  server.handleClient();
}
