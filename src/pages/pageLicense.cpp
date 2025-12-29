#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "pages.h"

void pageLicense(ESP8266WebServer &server) {
  String html = pagesHeaderPublic(server, "Lizenz", "/license");

  html += "<div class='card'><h2>Lizenz</h2>";
  html += "<pre style='white-space:pre-wrap; font-size:13px; line-height:1.35;'>";
  html += pagesLoadLicenseText();
  html += "</pre>";

  html += "<div class='actions'>"
          "<a class='btn btn-secondary' href='/about'>Zurück</a>"
          "</div>";

  html += "</div>";
  html += pagesFooter();

  server.send(200, "text/html; charset=utf-8", html);
}
