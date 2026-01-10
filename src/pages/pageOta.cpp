#include <Arduino.h>
#include <WebServer.h>
#include <Update.h>
#include <WiFi.h>
#include "pages.h"
#include "auth.h"

static String gOtaToken;
static uint32_t gOtaTokenAtMs = 0;
static IPAddress gOtaTokenIp;

static String humanSize(size_t b){
  if (b > 1024*1024) return String((float)b/(1024.0f*1024.0f), 2) + " MB";
  if (b > 1024)      return String((float)b/1024.0f, 1) + " KB";
  return String(b) + " B";
}

static String mkToken(){
  uint32_t id = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF);
  String t = String(id, HEX); t.toUpperCase();
  return t + "-" + String(millis(), HEX);
}

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
  size_t maxSketchSpace = (freeSketch - 0x1000) & 0xFFFFF000;

  String html = pagesHeaderAuth("OTA Update", "/settings");

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
          "<a class='btn btn-secondary' href='/settings'>Abbrechen</a>"
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
  if (!pass.length() || sha1Hex(pass) != cfg->admin_pass_hash) {
    String html = pagesHeaderAuth("OTA Update", "/settings");
    html += "<div class='card'><h2>Update gesperrt</h2>";
    html += "<p class='badge badge-bad badge-inline'>Admin-Passwort ist falsch.</p>";
    html += "<div class='actions mt-14'>"
            "<a class='btn btn-secondary' href='/ota'>Zurück</a>"
            "<a class='btn btn-secondary' href='/settings'>Einstellungen</a>"
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
// 4) /ota_upload POST (Upload handler -> Update)
// ------------------------------------------------------------------
void pageOtaUpload(WebServer &server){
  AppConfig* cfg = pagesCfg();
  if (!cfg) { return; }
  if (!requireAuth(server, *cfg)) return;

  // Token bei jedem Status prüfen (sonst Upload ignorieren)
  if (!tokenValid(server)) {
    if (Update.isRunning()) {
    Update.end(false);  // beendet mit Fehler
  }
  return;
  }

  HTTPUpload& up = server.upload();

  if (up.status == UPLOAD_FILE_START){
    size_t freeSketch = ESP.getFreeSketchSpace();
    size_t maxSketchSpace = (freeSketch - 0x1000) & 0xFFFFF000;

    if (!Update.begin(maxSketchSpace, U_FLASH)) {
      Serial.printf("OTA begin failed. Error=%u\n", (unsigned)Update.getError());
      Update.printError(Serial);
    } else {
      Serial.println("OTA begin OK");
    }
  }
  else if (up.status == UPLOAD_FILE_WRITE){
    if (Update.isRunning()) {
      size_t written = Update.write(up.buf, up.currentSize);
      if (written != up.currentSize) {
        Serial.printf("OTA write mismatch: %u/%u\n", (unsigned)written, (unsigned)up.currentSize);
      }
    }
  }
  else if (up.status == UPLOAD_FILE_END){
    bool ok = false;
    if (Update.isRunning()) ok = Update.end(true);

    // Token verbrauchen (one-shot)
    gOtaToken = "";

    if (!ok) {
      String err = String("OTA fehlgeschlagen: ") + updateErrorText();

      String html = pagesHeaderAuth("OTA Update", "/settings");
      html += "<div class='card'><h2>Update fehlgeschlagen</h2>";
      html += "<p class='badge badge-bad badge-inline'>" + err + "</p>";
      html += "<div class='actions mt-14'>"
              "<a class='btn btn-secondary' href='/ota'>Zurück</a>"
              "<a class='btn btn-secondary' href='/settings'>Einstellungen</a>"
              "</div>";
      html += "</div>";
      html += pagesFooter();

      server.send(200, "text/html; charset=utf-8", html);
      return;
    }

    String html = pagesHeaderAuth("OTA Update", "/settings");
    html += "<div class='card'><h2>Update erfolgreich</h2>";
    html += "<p class='badge badge-ok badge-inline'>Firmware wurde aktualisiert. Neustart…</p>";
    html += "</div>";
    html += pagesFooter();

    server.send(200, "text/html; charset=utf-8", html);
    delay(500);
    ESP.restart();
  }
}
