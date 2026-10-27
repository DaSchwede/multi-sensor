#include <Arduino.h>
#include <WebServer.h>
#include "pages.h"
#include "auth.h"
#include "version.h"

void pageAbout(WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  String html = pagesHeaderAuth("Über", "/about");
  html += pagesTitle("Über", "Version, Lizenz und Entstehung dieser Firmware.");

  html += "<div class='card'><h2>" + String(FW_NAME) + "</h2>";
  html += "<table class='tbl'>";
  html += "<tr><th>Firmware</th><td>" + String(FW_NAME) + "</td></tr>";
  html += "<tr><th>Version</th><td>" + String(FW_VERSION) + "</td></tr>";
  html += "<tr><th>Build-Datum</th><td>" + String(FW_DATE) + "</td></tr>";
  html += "<tr><th>Autor</th><td>" + String(FW_AUTHOR) + "</td></tr>";
  html += "<tr><th>Copyright</th><td>" + String(FW_COPYRIGHT) + "</td></tr>";
  html += "<tr><th>Lizenz</th><td><a href='/license'>anzeigen</a></td></tr>";
  html += "</table>";
  html += "</div>";

  html += "<div class='card'><h2>Entstehung</h2>";
  html += "<p>" + String(FW_NAME) + " ist ein privates Hobbyprojekt von " + String(FW_AUTHOR) + ".</p>";
  html += "<div class='hint'><b>Hinweis zur KI-Unterstützung:</b> Teile des Programmcodes und der Weboberfläche "
          "wurden mit Unterstützung künstlicher Intelligenz (Claude von Anthropic) geschrieben, überarbeitet "
          "und auf Fehler geprüft. Idee, Hardware, Tests am Gerät und die Verantwortung für das Projekt "
          "liegen beim Autor.</div>";
  html += "<div class='hint'>Die Nutzung erfolgt auf eigene Verantwortung. Es gelten die "
          "<a href='/license'>Lizenzbedingungen</a>.</div>";
  html += "</div>";

  html += cardChangelog();

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
