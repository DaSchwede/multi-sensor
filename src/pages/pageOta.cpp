#include <Arduino.h>
#include <WebServer.h>
#include <Update.h>
#include <WiFi.h>
#include "pages.h"
#include "auth.h"
#include "crypto_utils.h"
#include <esp_system.h>

static String gOtaToken;
static uint32_t gOtaTokenAtMs = 0;
static IPAddress gOtaTokenIp;

static String humanSize(size_t b){
  if (b > 1024*1024) return String((float)b/(1024.0f*1024.0f), 2) + " MB";
  if (b > 1024)      return String((float)b/1024.0f, 1) + " KB";
  return String(b) + " B";
}

static String mkToken(){
  // 128 Bit aus dem Hardware-Zufallsgenerator
  char buf[33];
  for (int i = 0; i < 4; i++) snprintf(buf + i * 8, 9, "%08x", (unsigned)esp_random());
  return String(buf);
}

// Zustand des laufenden Uploads (Upload-Handler sendet nichts, Antwort kommt in pageOtaDone)
static bool   gOtaAccepted = false;
static bool   gOtaError    = false;
static String gOtaErrMsg;

static bool tokenValid(WebServer &server){
  // Token per Query: /ota_upload?token=...
  String tok = server.arg("token");
  if (!tok.length()) return false;
  if (!gOtaToken.length()) return false;
  if (tok != gOtaToken) return false;

  // IP binden + Ablauf (2 Minuten)
  if (server.client().remoteIP() != gOtaTokenIp) return false;
  if (millis() - gOtaTokenAtMs > 120000UL) return false;
  return true;
}

static String updateErrorText() {
  // ESP32: Fehlercode als Zahl, Text notfalls über Serial
  return "Fehlercode " + String(Update.getError());
}

// ------------------------------------------------------------------
// 1) /ota  (Sicherheitsabfrage + Admin-Passwort)
// ------------------------------------------------------------------
void pageOtaForm(WebServer &server){
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  size_t freeSketch = ESP.getFreeSketchSpace();
  size_t maxSketchSpace = (freeSketch > 0x1000) ? ((freeSketch - 0x1000) & 0xFFFFF000) : 0;

  String html = pagesHeaderAuth("OTA Update", "/settings/tools");

  html += "<div class='card'><h2>Firmware Update (WebOTA)</h2>";
  html += "<p class='small'>Aus Sicherheitsgründen musst du die Aktion bestätigen und dein Admin-Passwort eingeben.</p>";

  html += "<table class='tbl'>";
  html += "<tr><th>Freier Sketch-Speicher</th><td>" + humanSize(freeSketch) + "</td></tr>";
  html += "<tr><th>Max. Update-Größe</th><td>" + humanSize(maxSketchSpace) + "</td></tr>";
  html += "</table>";

  html += "<form method='POST' action='/ota_prepare' class='mt-14'>";

  html += "<div class='form-row'>"
          "<div class='checkline'>"
          "<input type='checkbox' id='ota_confirm' name='ota_confirm' required>"
          "<label for='ota_confirm'>Ich verstehe, dass das Gerät nach dem Update neu startet.</label>"
          "</div>"
          "<div class='small'>Nur Firmware (<code>firmware.bin</code>), LittleFS bleibt unverändert.</div>"
          "</div>";

  html += "<div class='form-row'><label>Admin-Passwort bestätigen</label>"
          "<input name='admin_pass' type='password' required></div>";

  html += "<div class='actions'>"
          "<button class='btn-primary' type='submit'>Weiter zum Upload</button>"
          "<a class='btn btn-secondary' href='/settings/tools'>Abbrechen</a>"
          "</div>";

  html += "</form>";
  html += "</div>";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}

// ------------------------------------------------------------------
// 2) /ota_prepare  (prüft Passwort + "armt" Token)
// ------------------------------------------------------------------
void pageOtaPrepare(WebServer &server){
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  // Checkbox muss gesetzt sein
  if (!server.hasArg("ota_confirm")) {
    server.sendHeader("Location", "/ota", true);
    server.send(302, "text/plain", "");
    return;
  }

  String pass = server.arg("admin_pass");
  String pwErr;
  if (!authCheckAdminPassword(*cfg, pass, pwErr)) {
    String html = pagesHeaderAuth("OTA Update", "/settings");
    html += "<div class='card'><h2>Update gesperrt</h2>";
    html += "<p class='badge badge-bad badge-inline'>" + pwErr + "</p>";
    html += "<div class='actions mt-14'>"
            "<a class='btn btn-secondary' href='/ota'>Zurück</a>"
            "<a class='btn btn-secondary' href='/settings/tools'>Tools</a>"
            "</div>";
    html += "</div>";
    html += pagesFooter();
    server.send(200, "text/html; charset=utf-8", html);
    return;
  }

  // Token setzen
  gOtaToken = mkToken();
  gOtaTokenAtMs = millis();
  gOtaTokenIp = server.client().remoteIP();

  server.sendHeader("Location", "/ota_upload?token=" + gOtaToken, true);
  server.send(302, "text/plain", "");
}

// ------------------------------------------------------------------
// 3) /ota_upload GET (Upload-Form + Progress)
// ------------------------------------------------------------------
void pageOtaUploadForm(WebServer &server){
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  if (!tokenValid(server)) {
    server.sendHeader("Location", "/ota", true);
    server.send(302, "text/plain", "");
    return;
  }

  String tok = server.arg("token");

  String html = pagesHeaderAuth("OTA Upload", "/settings");

  html += "<div class='card'><h2>Firmware hochladen</h2>";
  html += "<p class='small'>Wähle <code>firmware.bin</code>. Fortschritt wird angezeigt. Danach Neustart.</p>";

  html += "<form id='otaForm' method='POST' action='/ota_upload?token=" + tok + "' enctype='multipart/form-data'>";
  html += "<div class='form-row'><label>Firmware Datei</label>"
          "<input type='file' name='fw' accept='.bin,application/octet-stream' required></div>";

  html += "<div class='progress-row'>"
          "<div class='progress'><div class='bar' id='otaBar'></div></div>"
          "<div class='progress-pct' id='otaPct'>0%</div>"
          "</div>";

  html += "<div class='small status-line' id='otaStatus'>Bereit.</div>";

  html += "<div class='actions'>"
          "<button class='btn-primary' type='submit'>Update starten</button>"
          "<a class='btn btn-secondary' href='/ota'>Zurück</a>"
          "</div>";

  html += "</form>";
  html += "</div>";

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}

// ------------------------------------------------------------------
// 4) /ota_upload POST – Upload-Daten (keine Antwort hier!)
// ------------------------------------------------------------------
static void otaFail(const String &msg) {
  if (!gOtaError) {
    gOtaError = true;
    gOtaErrMsg = msg;
    Serial.println("OTA: " + msg);
  }
  if (Update.isRunning()) Update.abort();
}

void pageOtaUpload(WebServer &server){
  HTTPUpload& up = server.upload();

  if (up.status == UPLOAD_FILE_START){
    AppConfig* cfg = pagesCfg();
    gOtaError = false;
    gOtaErrMsg = "";
    gOtaAccepted = cfg && isAuthenticated(server) && tokenValid(server);
    if (!gOtaAccepted) return;

    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      otaFail("Update.begin fehlgeschlagen (" + updateErrorText() + ")");
    } else {
      Serial.println("OTA begin OK");
    }
  }
  else if (up.status == UPLOAD_FILE_WRITE){
    if (!gOtaAccepted || gOtaError) return;
    if (Update.write(up.buf, up.currentSize) != up.currentSize) {
      otaFail("Schreibfehler (" + updateErrorText() + ")");
    }
  }
  else if (up.status == UPLOAD_FILE_END){
    if (!gOtaAccepted || gOtaError) return;
    if (!Update.end(true)) {
      otaFail("Abschluss fehlgeschlagen (" + updateErrorText() + ")");
    } else {
      Serial.printf("OTA fertig: %u Bytes\n", (unsigned)up.totalSize);
    }
  }
  else if (up.status == UPLOAD_FILE_ABORTED){
    if (gOtaAccepted) otaFail("Upload abgebrochen");
  }
}

// ------------------------------------------------------------------
// 5) /ota_upload POST – Antwort nach dem Upload
// ------------------------------------------------------------------
void pageOtaDone(WebServer &server){
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  // Token ist one-shot
  gOtaToken = "";

  const bool ok = gOtaAccepted && !gOtaError && Update.isFinished();

  String html = pagesHeaderAuth("OTA Update", "/settings/tools");
  if (!ok) {
    String err = !gOtaAccepted ? String("Upload-Freigabe ungültig oder abgelaufen (max. 2 Minuten). Bitte erneut starten.")
                               : String("OTA fehlgeschlagen: ") + (gOtaErrMsg.length() ? gOtaErrMsg : updateErrorText());
    html += "<div class='card'><h2>Update fehlgeschlagen</h2>";
    html += "<p class='badge badge-bad badge-inline'>" + err + "</p>";
    html += "<div class='actions mt-14'>"
            "<a class='btn btn-secondary' href='/ota'>Zurück</a>"
            "<a class='btn btn-secondary' href='/settings/tools'>Tools</a>"
            "</div>";
    html += "</div>";
    html += pagesFooter();
    server.send(200, "text/html; charset=utf-8", html);
    return;
  }

  html += "<div class='card'><h2>Update erfolgreich</h2>";
  html += "<p class='badge badge-ok badge-inline'>Firmware wurde aktualisiert. Neustart…</p>";
  html += "<p class='small'>Die Seite lädt in ca. 15 Sekunden neu.</p>";
  html += "<script>setTimeout(function(){location.href='/';},15000);</script>";
  html += "</div>";
  html += pagesFooter();

  server.send(200, "text/html; charset=utf-8", html);
  delay(500);
  ESP.restart();
}
