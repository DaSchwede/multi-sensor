#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <Updater.h>
#include "pages.h"
#include "auth.h"

static String humanSize(size_t b){
  if (b > 1024*1024) return String((float)b/(1024.0f*1024.0f), 2) + " MB";
  if (b > 1024)      return String((float)b/1024.0f, 1) + " KB";
  return String(b) + " B";
}

void pageOtaForm(ESP8266WebServer &server){
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  size_t freeSketch = ESP.getFreeSketchSpace();
  size_t maxSketchSpace = (freeSketch - 0x1000) & 0xFFFFF000;

  String html = pagesHeaderAuth("OTA Update", "/settings");

  html += "<div class='card'><h2>Firmware Update (WebOTA)</h2>";
  html += "<p class='small'>Hier kannst du eine neue Firmware (<code>firmware.bin</code>) hochladen. "
          "Nach erfolgreichem Update startet das Gerät neu.</p>";

  html += "<table class='tbl'>";
  html += "<tr><th>Freier Sketch-Speicher</th><td>" + humanSize(freeSketch) + "</td></tr>";
  html += "<tr><th>Max. Update-Größe</th><td>" + humanSize(maxSketchSpace) + "</td></tr>";
  html += "</table>";

  // Warnung wenn zu wenig Platz
  if (maxSketchSpace < 200*1024) {
    html += "<p class='badge badge-warn badge-inline mt-12'>"
            "Achtung: Sehr wenig freier Flash. OTA könnte fehlschlagen (insb. bei 1MB Boards).</p>";
  }

  html += "<form class='mt-14' method='POST' action='/ota' enctype='multipart/form-data'>";
  html += "<div class='form-row'><label>Firmware Datei</label>"
          "<input type='file' name='fw' accept='.bin,application/octet-stream' required></div>";
  html += "<div class='actions'>"
          "<button class='btn-primary' type='submit'>Update starten</button>"
          "<a class='btn btn-secondary' href='/settings'>Zurück</a>"
          "</div>";
  html += "</form>";

  html += "</div>";
  html += pagesFooter();

  server.send(200, "text/html; charset=utf-8", html);
}

void pageOtaUpload(ESP8266WebServer &server){
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  HTTPUpload& up = server.upload();

  if (up.status == UPLOAD_FILE_START){
    size_t freeSketch = ESP.getFreeSketchSpace();
    size_t maxSketchSpace = (freeSketch - 0x1000) & 0xFFFFF000;

    Update.runAsync(true);
    if (!Update.begin(maxSketchSpace)) {
      Serial.printf("OTA begin failed: %s\n", Update.getErrorString().c_str());
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

    if (Update.isRunning()) {
      ok = Update.end(true);
    }

    if (!ok) {
      String err = String("OTA fehlgeschlagen: ") + Update.getErrorString();

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
