#include "web_server.h"
#include <LittleFS.h>
#include "auth.h"
#include "pages.h"
#include "sensors_ctrl.h"
#include "settings_config/settings_common.h"
#include "logger.h"
#include "ui_css.h"
#include "ui_logger_js.h"

// Pfad der aufrufenden Seite aus dem Referer (nur lokale Pfade, sonst Fallback)
static String backPathFromReferer(WebServer &server, const char *fallback) {
  String ref = server.header("Referer");
  const int scheme = ref.indexOf("://");
  if (scheme < 0) return fallback;
  const int slash = ref.indexOf('/', scheme + 3);
  if (slash < 0) return "/";
  String path = ref.substring(slash);
  if (!path.startsWith("/") || path.startsWith("//")) return fallback;
  if (path.indexOf('\r') >= 0 || path.indexOf('\n') >= 0) return fallback;
  return path;
}

void webServerBegin(WebServer &server,
                    AppConfig &cfg,
                    SensorData *liveData,
                    uint32_t *lastReadMs,
                    uint32_t *lastSendMs) {

  // pages bekommt Zugriff auf cfg/live/zeiten
  pagesInit(cfg, liveData, lastReadMs, lastSendMs);

  // Statische Dateien
  server.serveStatic("/style.css", LittleFS, "/style.css");
  // Ergänzende Styles aus der Firmware (siehe include/ui_css.h)
  server.on("/ui-logger.js", HTTP_GET, [&](){
    server.sendHeader("Cache-Control", "max-age=300");
    server.send_P(200, "application/javascript", UI_LOGGER_JS);
  });
  server.on("/ui.css", HTTP_GET, [&](){
    server.sendHeader("Cache-Control", "max-age=300");
    server.send_P(200, "text/css", UI_EXTRA_CSS);
  });
  server.serveStatic("/script.js", LittleFS, "/script.js");
  server.serveStatic("/logger.js", LittleFS, "/logger.js");
  server.serveStatic("/wifi.js", LittleFS, "/wifi.js");
  server.serveStatic("/favicon.svg", LittleFS, "/favicon.svg");
  server.serveStatic("/logo_name_weiss.svg", LittleFS, "/logo_name_weiss.svg");
  server.serveStatic("/logo_name_weiss_gruen.svg", LittleFS, "/logo_name_weiss_gruen.svg");
  server.serveStatic("/logo_name_gruen.svg", LittleFS, "/logo_name_gruen.svg");

      // Cookie-Header lesen können
  static const char* headerKeys[] = { "Cookie", "Referer" };
  server.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys[0]));


  // Auth-Routen (/login, /force_pw, /logout)
  authAttach(server, cfg);

  server.on("/action/rescan_sensors", HTTP_POST, [&](){
    // Auth check wie bei settings-Seiten
    AppConfig* c = settingsRequireCfgAndAuth(server);
    if (!c) return;

    // Rescan anstoßen
    requestSensorRescan();   // wird im loop() ausgeführt

    // Zurück auf die Seite, von der der Knopf gedrückt wurde
    server.sendHeader("Location", backPathFromReferer(server, "/info"), true);
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
  server.on("/api/logs", HTTP_GET, [&](){ apiLogList(server); });
  server.on("/api/time/status", HTTP_GET, [&](){ apiTimeStatus(server); });
  server.on("/api/time/sync", HTTP_POST, [&](){ apiTimeSync(server); });
  server.on("/api/logs/download", HTTP_GET, [&](){ apiLogDownload(server); });

  // Seiten
  server.on("/", HTTP_GET,        [&](){ pageRoot(server); });
  server.on("/info", HTTP_GET,    [&](){ pageInfo(server); });

  server.on("/logger", HTTP_GET,  [&](){ pageLogger(server); });

  server.on("/info/system", HTTP_GET, [&](){ pageSystemInfo(server); });
  server.on("/info/time", HTTP_GET, [&](){ pageNTP(server); });
  server.on("/settings/ntp", HTTP_GET, [&](){   // alte Adresse (Feb 2026) -> Einstellungen Zeit
    server.sendHeader("Location", "/settings/time", true);
    server.send(302, "text/plain", "");
  });


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
  server.on("/settings/tools", HTTP_POST, [&](){ pageSettingsTools(server); });

  server.on("/settings/sensors", HTTP_GET, [&](){ pageSettingsSensors(server); });
  server.on("/settings/sensors", HTTP_POST, [&](){ pageSettingsSensors(server); });

  server.on("/about", HTTP_GET,   [&](){ pageAbout(server); });
  server.on("/backup", HTTP_GET,  [&](){ pageBackup(server); });

  // Restore Upload (GET Form + POST Upload)
  server.on("/restore", HTTP_GET, [&](){ pageRestoreForm(server); });
  server.on("/restore", HTTP_POST,
    [&](){ pageRestoreDone(server); },      // Antwort nach dem Upload
    [&](){ pageRestoreUpload(server); }     // Upload-Daten
  );

  server.on("/ota", HTTP_GET,  [&](){ pageOtaForm(server); });
  server.on("/ota_prepare", HTTP_POST, [&](){ pageOtaPrepare(server); });

  server.on("/ota_upload", HTTP_GET,  [&](){ pageOtaUploadForm(server); });

  server.on("/ota_upload", HTTP_POST,
    [&](){ pageOtaDone(server); },          // Antwort nach dem Upload
    [&](){ pageOtaUpload(server); }         // Upload-Daten
  );

  server.on("/factory_reset", HTTP_GET,  [&](){ pageFactoryResetForm(server); });
  server.on("/factory_reset", HTTP_POST, [&](){ pageFactoryResetDo(server); });


  server.begin();
}

void webServerLoop(WebServer &server) {
  server.handleClient();
}
