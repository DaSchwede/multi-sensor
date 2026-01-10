#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "pages.h"
#include "auth.h"
#include "settings.h"

static File gUploadFile;

void pageBackup(WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  if (!LittleFS.exists("/config.json")) {
    server.send(404, "text/plain; charset=utf-8", "config.json nicht gefunden");
    return;
  }

  File f = LittleFS.open("/config.json", "r");
  server.sendHeader("Content-Disposition", "attachment; filename=\"config.json\"");
  server.streamFile(f, "application/json");
  f.close();
}

void pageRestoreForm(WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  String html = pagesHeaderAuth("Restore", "/settings");

  html += "<div class='card'><h2>Restore config.json</h2>"
          "<form method='POST' action='/restore' enctype='multipart/form-data'>"
          "<div class='form-row'><label>config.json auswählen</label>"
          "<input type='file' name='cfg' accept='application/json' required></div>"
          "<div class='actions'>"
          "<button class='btn-primary' type='submit'>Upload</button>"
          "<a class='btn btn-secondary' href='/settings'>Zurück</a>"
          "</div>"
          "</form></div>";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}

void pageRestoreUpload(WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  HTTPUpload& up = server.upload();

  if (up.status == UPLOAD_FILE_START) {
    if (LittleFS.exists("/config.json")) LittleFS.remove("/config.json");
    gUploadFile = LittleFS.open("/config.json", "w");
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (gUploadFile) gUploadFile.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    if (gUploadFile) gUploadFile.close();

    // Config neu laden (damit UI sofort aktuelle Werte nutzt)
    loadConfig(*cfg);

    server.sendHeader("Location", "/settings", true);
    server.send(302, "text/plain", "");
  }
}

void pageFactoryResetForm(WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  String html = pagesHeaderAuth("Factory Reset", "/settings");

  html += "<div class='card'><h2>Werkseinstellungen</h2>"
          "<p class='small'>Das löscht die Konfiguration (config.json). Danach startet das Gerät neu.</p>"
          "<form method='POST' action='/factory_reset'>"
          "<div class='actions'>"
          "<button class='btn-primary' type='submit'>Ja, löschen & neu starten</button>"
          "<a class='btn btn-secondary' href='/settings'>Abbrechen</a>"
          "</div></form></div>";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}

void pageFactoryResetDo(WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  if (LittleFS.exists("/config.json")) LittleFS.remove("/config.json");

  server.send(200, "text/plain; charset=utf-8", "OK, reboot...");
  delay(300);
  ESP.restart();
}
