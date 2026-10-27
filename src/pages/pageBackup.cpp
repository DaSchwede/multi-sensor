#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "pages.h"
#include "auth.h"
#include "settings.h"
#include "mqtt_client.h"
#include "ntp_time.h"

static const char*  RESTORE_TMP = "/restore.tmp";
static const size_t RESTORE_MAX_BYTES = 32 * 1024;

static File   gUploadFile;
static bool   gRestoreAuthed = false;
static bool   gRestoreError  = false;
static size_t gRestoreBytes  = 0;

static void sendResultPage(WebServer &server, const String &title, const String &text, bool ok) {
  String html = pagesHeaderAuth(title, "/settings/tools");
  html += "<div class='card'><h2>" + title + "</h2>";
  html += String("<p class='badge ") + (ok ? "badge-ok" : "badge-bad") + " badge-inline'>" + text + "</p>";
  html += "<div class='actions mt-14'><a class='btn btn-secondary' href='/settings/tools'>Zurück zu Tools</a></div>";
  html += "</div>";
  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}

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

  String html = pagesHeaderAuth("Restore", "/settings/tools");

  html += "<div class='card'><h2>Restore config.json</h2>"
          "<p class='small'>Die Datei wird vor dem Übernehmen geprüft. Bei einer ungültigen Datei bleibt die aktuelle Konfiguration erhalten.</p>"
          "<form method='POST' action='/restore' enctype='multipart/form-data'>"
          "<div class='form-row'><label>config.json auswählen</label>"
          "<input type='file' name='cfg' accept='application/json,.json' required></div>"
          "<div class='actions'>"
          "<button class='btn-primary' type='submit'>Upload</button>"
          "<a class='btn btn-secondary' href='/settings/tools'>Zurück</a>"
          "</div>"
          "</form></div>";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}

// Upload-Handler: nimmt nur Daten an, sendet KEINE Antwort (die kommt in pageRestoreDone)
void pageRestoreUpload(WebServer &server) {
  AppConfig* cfg = pagesCfg();
  HTTPUpload& up = server.upload();

  if (up.status == UPLOAD_FILE_START) {
    gRestoreAuthed = cfg && isAuthenticated(server) && !cfg->force_pw_change && !passwordIsDefault(*cfg);
    gRestoreError = false;
    gRestoreBytes = 0;
    if (!gRestoreAuthed) return;
    LittleFS.remove(RESTORE_TMP);
    gUploadFile = LittleFS.open(RESTORE_TMP, "w");
    if (!gUploadFile) gRestoreError = true;
  }
  else if (up.status == UPLOAD_FILE_WRITE) {
    if (!gRestoreAuthed || gRestoreError || !gUploadFile) return;
    if (gRestoreBytes + up.currentSize > RESTORE_MAX_BYTES) {
      gRestoreError = true;
      gUploadFile.close();
      return;
    }
    if (gUploadFile.write(up.buf, up.currentSize) != up.currentSize) {
      gRestoreError = true;
      gUploadFile.close();
      return;
    }
    gRestoreBytes += up.currentSize;
  }
  else if (up.status == UPLOAD_FILE_END) {
    if (gUploadFile) gUploadFile.close();
  }
  else if (up.status == UPLOAD_FILE_ABORTED) {
    if (gUploadFile) gUploadFile.close();
    gRestoreError = true;
  }
}

// Antwort nach dem Upload: prüfen, übernehmen, Dienste neu starten
void pageRestoreDone(WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) { LittleFS.remove(RESTORE_TMP); return; }

  if (!gRestoreAuthed || gRestoreError || gRestoreBytes == 0) {
    LittleFS.remove(RESTORE_TMP);
    sendResultPage(server, "Restore fehlgeschlagen",
                   gRestoreBytes == 0 ? "Keine Datei empfangen." : "Upload fehlgeschlagen oder Datei zu groß (max. 32 KB).",
                   false);
    return;
  }

  if (!configFileLooksValid(RESTORE_TMP)) {
    LittleFS.remove(RESTORE_TMP);
    sendResultPage(server, "Restore abgelehnt",
                   "Die Datei ist keine gültige Multi-Sensor-Konfiguration. Die aktuelle Konfiguration bleibt unverändert.",
                   false);
    return;
  }

  AppConfig restored = *cfg;   // fehlende Keys behalten die aktuellen Werte
  if (!configLoadFromFile(RESTORE_TMP, restored)) {
    LittleFS.remove(RESTORE_TMP);
    sendResultPage(server, "Restore fehlgeschlagen", "Datei konnte nicht gelesen werden.", false);
    return;
  }
  LittleFS.remove(RESTORE_TMP);

  settingsApplyDeviceDefaults(restored);
  *cfg = restored;
  if (!saveConfig(*cfg)) {
    sendResultPage(server, "Restore fehlgeschlagen", "Konfiguration konnte nicht gespeichert werden.", false);
    return;
  }

  // Dienste mit neuen Einstellungen starten
  mqttBegin(*cfg);
  if (WiFi.status() == WL_CONNECTED) ntpBegin(*cfg);
  else ntpApplyTimezone(*cfg);

  sendResultPage(server, "Restore erfolgreich", "Konfiguration wurde übernommen.", true);
}

void pageFactoryResetForm(WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  String html = pagesHeaderAuth("Factory Reset", "/settings/tools");

  html += "<div class='card'><h2>Werkseinstellungen</h2>"
          "<p class='small'>Das löscht die Konfiguration (config.json inkl. Backup). WLAN-Zugangsdaten bleiben erhalten "
          "(dafür unter Einstellungen → WLAN „WLAN vergessen“). Danach startet das Gerät neu.</p>"
          "<form method='POST' action='/factory_reset'>"
          "<div class='actions'>"
          "<button class='btn-primary' type='submit'>Ja, löschen & neu starten</button>"
          "<a class='btn btn-secondary' href='/settings/tools'>Abbrechen</a>"
          "</div></form></div>";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}

void pageFactoryResetDo(WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  configRemoveAll();

  server.send(200, "text/plain; charset=utf-8", "OK, reboot...");
  delay(300);
  ESP.restart();
}
