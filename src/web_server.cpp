#include "web_server.h"
#include <LittleFS.h>
#include "auth.h"
#include "pages.h"

#include "settings_config/pageSettingsUdp.h"
#include "settings_config/pageSettingsTime.h"
#include "settings_config/pageSettingsMqtt.h"
#include "settings_config/pageSettingsUi.h"
#include "settings_config/settings_common.h"

// Schneller 302 Redirect
static inline void sendRedirect(WebServer &server, const char *to) {
  server.sendHeader("Location", to, true);
  server.send(302, "text/plain", "");
}

// NotFound ohne unnötige Heap-Last
static void handleNotFound(WebServer &server) {
  const String &u = server.uri(); // Referenz -> keine extra Kopie

  // Captive-Portal / OS Checks leise beantworten
  if (u.equals("/generate_204") || u.equals("/gen_204") ||
      u.equals("/hotspot-detect.html") || u.equals("/fwlink") ||
      u.equals("/wpad.dat")) {
    server.send(204, "text/plain", "");
    return;
  }

  // favicon fallback
  if (u.equals("/favicon.ico")) {
    sendRedirect(server, "/favicon.svg");
    return;
  }

  server.send(404, "text/plain", "Not found");
}

void webServerBegin(WebServer &server,
                    AppConfig &cfg,
                    SensorData *liveData,
                    uint32_t *lastReadMs,
                    uint32_t *lastSendMs) {

  // ESP32 WebServer: weniger interne delays (wenn deine Core-Version das nicht kennt -> Zeile entfernen)
  server.enableDelay(false);

  // pages bekommt Zugriff auf cfg/live/zeiten
  pagesInit(cfg, liveData, lastReadMs, lastSendMs);

  // Cookie-Header lesen können (einmalig)
  static const char* headerKeys[] = { "Cookie" };
  server.collectHeaders(headerKeys, 1);

  // onNotFound früh setzen
  server.onNotFound([&server](){ handleNotFound(server); });

  Serial.printf("LittleFS style.css exists? %d\n", LittleFS.exists("/style.css"));

  // ---------- Static Files ----------
  // Cache-Control ist auf dem C3 Gold wert: weniger Requests => weniger CPU/WLAN
  server.serveStatic("/style.css", LittleFS, "/style.css", "max-age=86400");
  server.serveStatic("/script.js", LittleFS, "/script.js", "max-age=86400");

  server.serveStatic("/favicon.svg", LittleFS, "/favicon.svg", "max-age=604800");
  server.serveStatic("/logo_name_weiss.svg", LittleFS, "/logo_name_weiss.svg", "max-age=604800");
  server.serveStatic("/logo_name_weiss_gruen.svg", LittleFS, "/logo_name_weiss_gruen.svg", "max-age=604800");
  server.serveStatic("/logo_name_gruen.svg", LittleFS, "/logo_name_gruen.svg", "max-age=604800");

  // ---------- Auth-Routen (/login, /force_pw, /logout) ----------
  authAttach(server, cfg);

  // ---------- Routen ----------
  server.on("/license", HTTP_GET, [&server](){ pageLicense(server); });

  // API
  server.on("/api/live", HTTP_GET, [&server](){ apiLive(server); });

  // Seiten
  server.on("/",     HTTP_GET, [&server](){ pageRoot(server); });
  server.on("/info", HTTP_GET, [&server](){ pageInfo(server); });

  server.on("/settings", HTTP_GET, [&server](){
    sendRedirect(server, "/settings/udp");
  });

  // Settings
  server.on("/settings/udp",  HTTP_GET,  [&server](){ pageSettingsUdp(server); });
  server.on("/settings/udp",  HTTP_POST, [&server](){ pageSettingsUdp(server); });

  server.on("/settings/time", HTTP_GET,  [&server](){ pageSettingsTime(server); });
  server.on("/settings/time", HTTP_POST, [&server](){ pageSettingsTime(server); });

  server.on("/settings/mqtt", HTTP_GET,  [&server](){ pageSettingsMqtt(server); });
  server.on("/settings/mqtt", HTTP_POST, [&server](){ pageSettingsMqtt(server); });

  server.on("/settings/ui",   HTTP_GET,  [&server](){ pageSettingsUi(server); });
  server.on("/settings/ui",   HTTP_POST, [&server](){ pageSettingsUi(server); });

  server.on("/settings/tools", HTTP_GET, [&server](){ pageSettingsTools(server); });

  server.on("/about",  HTTP_GET, [&server](){ pageAbout(server); });
  server.on("/backup", HTTP_GET, [&server](){ pageBackup(server); });

  // Restore Upload
  server.on("/restore", HTTP_GET, [&server](){ pageRestoreForm(server); });
  server.on("/restore", HTTP_POST,
    [&server](){ /* Antwort kommt im Upload-End */ },
    [&server](){ pageRestoreUpload(server); }
  );

  // OTA
  server.on("/ota", HTTP_GET, [&server](){ pageOtaForm(server); });
  server.on("/ota_prepare", HTTP_POST, [&server](){ pageOtaPrepare(server); });

  server.on("/ota_upload", HTTP_GET, [&server](){ pageOtaUploadForm(server); });
  server.on("/ota_upload", HTTP_POST,
    [&server](){ /* Antwort kommt im Upload END */ },
    [&server](){ pageOtaUpload(server); }
  );

  server.on("/factory_reset", HTTP_GET,  [&server](){ pageFactoryResetForm(server); });
  server.on("/factory_reset", HTTP_POST, [&server](){ pageFactoryResetDo(server); });

  server.begin();
}

void webServerLoop(WebServer &server) {
  server.handleClient();
}
