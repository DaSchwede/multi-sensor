#include <Arduino.h>
#include <WebServer.h>
#include "pages.h"
#include "auth.h"
#include "crypto_utils.h"
#include "html_util.h"
#include "version.h"
#include "settings_config/settings_common.h"

static const size_t ADMIN_PW_MIN = 8;
static const size_t ADMIN_PW_MAX = 64;

// Karte mit Kopfzeile (wie in der restlichen Oberfläche, nur mit Titelbalken)
static String secOpen(const String &title, bool danger = false) {
  return String("<div class='card sec'><div class='sec-head") + (danger ? " danger" : "") + "'>" +
         title + "</div><div class='sec-body'>";
}
static const char *SEC_CLOSE = "</div></div>";

void pageSettingsTools(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String okMsg, errMsg;

  // ===== Passwort ändern =====
  if (server.method() == HTTP_POST && server.arg("action") == "change_pw") {
    const String cur = server.arg("pw_current");
    const String p1  = server.arg("pw_new");
    const String p2  = server.arg("pw_repeat");

    String pwErr;
    if (!authCheckAdminPassword(*cfg, cur, pwErr)) {
      errMsg = (pwErr == "Passwort falsch.") ? String("Aktuelles Passwort ist falsch.") : pwErr;
    } else if (p1.length() < ADMIN_PW_MIN || p1.length() > ADMIN_PW_MAX) {
      errMsg = "Das neue Passwort muss " + String(ADMIN_PW_MIN) + " bis " + String(ADMIN_PW_MAX) + " Zeichen lang sein.";
    } else if (p1 != p2) {
      errMsg = "Die neuen Passwörter stimmen nicht überein.";
    } else if (p1 == cur) {
      errMsg = "Das neue Passwort ist identisch mit dem aktuellen.";
    } else if (p1 == "admin") {
      errMsg = "Bitte ein anderes Passwort als „admin“ wählen.";
    } else {
      if (authSetAdminPassword(*cfg, p1)) {
        authKeepOnlyCurrentSession(server);   // andere Geräte müssen sich neu anmelden
        okMsg = "Passwort geändert. Du bleibst angemeldet, andere Geräte wurden abgemeldet.";
      } else {
        errMsg = "Passwort konnte nicht gespeichert werden.";
      }
    }
  }

  String html = pagesHeaderAuth("Einstellungen – Tools", "/settings/tools");

  html += pagesTitle("Tools", "Sicherung, Wiederherstellung, Passwort, Firmware-Update und Werksreset.");

  if (okMsg.length())  html += "<div class='hint'><b>✔ " + htmlEscape(okMsg) + "</b></div>";
  if (errMsg.length()) html += "<div class='hint danger'><b>" + htmlEscape(errMsg) + "</b></div>";

  // ===== Sicherung =====
  html += secOpen("Sicherung der Konfiguration");
  html += "<div class='tool-row'>"
            "<div class='lbl'>Konfigurationsdatei</div>"
            "<div class='grow small'>config.json mit allen Einstellungen (ohne WLAN-Zugangsdaten)</div>"
            "<a class='btn btn-primary' href='/backup'>Sichern</a>"
          "</div>";
  html += "<div class='hint'><b>Hinweis:</b> Die Datei enthält das MQTT-Passwort im Klartext und den "
          "Hash des Admin-Passworts – bitte sicher aufbewahren.</div>";
  html += SEC_CLOSE;

  // ===== Wiederherstellen =====
  html += secOpen("Wiederherstellen der Konfiguration");
  html += "<form method='POST' action='/restore' enctype='multipart/form-data'>"
          "<div class='tool-row'>"
            "<div class='lbl'>config.json auswählen</div>"
            "<div class='grow'><input type='file' name='cfg' accept='application/json,.json' required></div>"
            "<button class='btn-primary' type='submit'>Wiederherstellen</button>"
          "</div>"
          "</form>";
  html += "<div class='hint warn'><b>Hinweis:</b> Die aktuelle Konfiguration wird durch die Datei ersetzt. "
          "Die Datei wird vorher geprüft – eine ungültige Datei wird abgelehnt und nichts geändert. "
          "MQTT und Zeit übernehmen die neuen Einstellungen sofort, ein Neustart ist nicht nötig. "
          "WLAN-Zugangsdaten bleiben unverändert.</div>";
  html += SEC_CLOSE;

  // ===== Administrator-Passwort =====
  html += secOpen("Administrator-Passwort");
  html += "<form method='POST' action='/settings/tools' autocomplete='off'>"
          "<input type='hidden' name='action' value='change_pw'>"
          "<div class='pw-grid'>"
            "<label for='pw_current'>Aktuelles Passwort:</label>"
            "<input id='pw_current' name='pw_current' type='password' autocomplete='current-password' required>"
            "<label for='pw_new'>Neues Passwort:</label>"
            "<input id='pw_new' name='pw_new' type='password' autocomplete='new-password' "
              "minlength='" + String(ADMIN_PW_MIN) + "' maxlength='" + String(ADMIN_PW_MAX) + "' required>"
            "<label for='pw_repeat'>Passwort wiederholen:</label>"
            "<input id='pw_repeat' name='pw_repeat' type='password' autocomplete='new-password' "
              "minlength='" + String(ADMIN_PW_MIN) + "' maxlength='" + String(ADMIN_PW_MAX) + "' required>"
          "</div>";
  html += "<div class='hint'><b>Hinweis:</b> Das Passwort gilt für die Weboberfläche (Benutzer '" +
          htmlEscape(cfg->admin_user) + "') und für die Bestätigung beim Firmware-Update. "
          "Es muss zwischen " + String(ADMIN_PW_MIN) + " und " + String(ADMIN_PW_MAX) + " Zeichen lang sein. "
          "Das Passwort des Setup-WLANs ist davon unabhängig (Einstellungen → WLAN).</div>";
  html += "<div class='right-actions'>"
            "<button class='btn-secondary' type='reset'>Abbrechen</button>"
            "<button class='btn-primary' type='submit'>Speichern</button>"
          "</div>"
          "</form>";
  html += SEC_CLOSE;

  // ===== Firmware-Update =====
  html += secOpen("Firmware-Update (OTA)");
  html += "<div class='tool-row'>"
            "<div class='lbl'>Installierte Version</div>"
            "<div class='grow'><b>" + String(FW_VERSION) + "</b> <span class='small'>(" + String(FW_DATE) + ")</span></div>"
            "<a class='btn btn-primary' href='/ota'>Update starten</a>"
          "</div>";
  html += "<div class='hint'><b>Hinweis:</b> Nur <code>firmware.bin</code> hochladen. Einstellungen und "
          "Messdaten bleiben erhalten. Das Gerät startet danach neu.</div>";
  html += SEC_CLOSE;

  // ===== Werksreset =====
  html += secOpen("Werksreset durchführen", true);
  html += "<form method='POST' action='/factory_reset' "
          "onsubmit=\"return confirm('Wirklich alle Einstellungen löschen und neu starten?');\">"
          "<div class='actions'>"
            "<button class='btn-danger' type='submit'>Werkseinstellungen wiederherstellen</button>"
          "</div>"
          "</form>";
  html += "<div class='hint danger'><b>Hinweis:</b> Löscht die komplette Konfiguration inkl. Admin-Passwort "
          "und startet neu. WLAN-Zugangsdaten und Messdaten auf der SD-Karte bleiben erhalten. "
          "Vorher am besten eine Sicherung erstellen.</div>";
  html += SEC_CLOSE;

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
