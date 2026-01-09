#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "pages.h"
#include "settings_config/settings_common.h"

// Hinweis: Diese Seite erwartet folgende Felder in AppConfig:
//   bool   mqtt_enabled;
//   String mqtt_host;
//   int    mqtt_port;
//   String mqtt_user;
//   String mqtt_pass;        // wird NICHT im UI angezeigt
//   String mqtt_client_id;
//   String mqtt_topic_base;
//   bool   mqtt_retain;
//   int    mqtt_qos;         // 0..2
//   int    mqtt_keepalive;   // Sekunden
//   bool   mqtt_clean_session;

static String trimCopy(const String &s) {
  String t = s;
  t.trim();
  return t;
}

static int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

void pageSettingsMqtt(ESP8266WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String msg = "";

  if (server.method() == HTTP_POST) {
    // Enable
    cfg->mqtt_enabled = server.hasArg("mqtt_enabled");

    // Broker
    if (server.hasArg("mqtt_host")) {
      cfg->mqtt_host = trimCopy(server.arg("mqtt_host"));
    }
    if (server.hasArg("mqtt_port")) {
      cfg->mqtt_port = toIntSafe(server.arg("mqtt_port"), cfg->mqtt_port);
      cfg->mqtt_port = clampInt(cfg->mqtt_port, 1, 65535);
    }

    // Credentials
    if (server.hasArg("mqtt_user")) {
      cfg->mqtt_user = server.arg("mqtt_user"); // bewusst ohne trim (User kann Spaces haben)
    }

    // Passwort: nur setzen, wenn Feld NICHT leer ist
    if (server.hasArg("mqtt_pass")) {
      String p = server.arg("mqtt_pass");
      if (p.length() > 0) cfg->mqtt_pass = p;
    }
    // Optional: Passwort löschen
    if (server.hasArg("mqtt_pass_clear")) {
      cfg->mqtt_pass = "";
    }

    // Client / Topic
    if (server.hasArg("mqtt_client_id")) {
      cfg->mqtt_client_id = trimCopy(server.arg("mqtt_client_id"));
    }
    if (server.hasArg("mqtt_topic_base")) {
      cfg->mqtt_topic_base = trimCopy(server.arg("mqtt_topic_base"));
      // optional: führenden Slash entfernen
      while (cfg->mqtt_topic_base.startsWith("/")) cfg->mqtt_topic_base.remove(0, 1);
      // optional: abschließenden Slash entfernen
      while (cfg->mqtt_topic_base.endsWith("/")) cfg->mqtt_topic_base.remove(cfg->mqtt_topic_base.length() - 1);
    }

    // Optionen
    cfg->mqtt_retain = server.hasArg("mqtt_retain");
    cfg->mqtt_clean_session = server.hasArg("mqtt_clean_session");

    if (server.hasArg("mqtt_qos")) {
      cfg->mqtt_qos = toIntSafe(server.arg("mqtt_qos"), cfg->mqtt_qos);
      cfg->mqtt_qos = clampInt(cfg->mqtt_qos, 0, 2);
    }
    if (server.hasArg("mqtt_keepalive")) {
      cfg->mqtt_keepalive = toIntSafe(server.arg("mqtt_keepalive"), cfg->mqtt_keepalive);
      cfg->mqtt_keepalive = clampInt(cfg->mqtt_keepalive, 5, 3600);
    }

    saveConfig(*cfg);
    msg = "Gespeichert.";
  }

  String html = pagesHeaderAuth("Einstellungen – MQTT", "/settings/mqtt");
  settingsSendOkBadge(html, msg);

  html += "<form method='POST' autocomplete='off'>";

  // Card: MQTT aktivieren
  html += "<div class='card'><h2>MQTT</h2>";
  html += "<div class='form-row'>"
          "<label><input type='checkbox' name='mqtt_enabled' " + String(cfg->mqtt_enabled ? "checked" : "") + "> MQTT aktiv</label>"
          "</div>";
  html += "</div>";

  // Card: Broker
  html += "<div class='card'><h2>Broker</h2>";
  html += "<div class='form-row'><label>Host / IP</label>"
          "<input name='mqtt_host' placeholder='z.B. 192.168.1.10 oder broker.local' value='" + cfg->mqtt_host + "'></div>";
  html += "<div class='form-row'><label>Port</label>"
          "<input name='mqtt_port' type='number' min='1' max='65535' value='" + String(cfg->mqtt_port) + "'></div>";
  html += "</div>";

  // Card: Zugangsdaten
  html += "<div class='card'><h2>Zugangsdaten</h2>";
  html += "<div class='form-row'><label>Benutzer</label>"
          "<input name='mqtt_user' value='" + cfg->mqtt_user + "'></div>";

  // Passwort wird NICHT als value gesetzt (nicht leaken)
  html += "<div class='form-row'><label>Passwort</label>"
          "<input name='mqtt_pass' type='password' placeholder='leer lassen = unverändert'></div>";

  html += "<div class='form-row'>"
          "<label><input type='checkbox' name='mqtt_pass_clear'> Passwort löschen</label>"
          "</div>";

  html += "</div>";

  // Card: Topics / Client
  html += "<div class='card'><h2>Topics</h2>";
  html += "<div class='form-row'><label>Topic Base</label>"
          "<input name='mqtt_topic_base' placeholder='z.B. multisensor/wohnzimmer' value='" + cfg->mqtt_topic_base + "'></div>";
  html += "<div class='form-row'><label>Client ID</label>"
          "<input name='mqtt_client_id' placeholder='z.B. multi-sensor-01' value='" + cfg->mqtt_client_id + "'></div>";
  html += "<div class='hint'>Beispiel-Publish: <code>" + cfg->mqtt_topic_base + "/temperature</code></div>";
  html += "</div>";

  // Card: Optionen
  html += "<div class='card'><h2>Optionen</h2>";
  html += "<div class='form-row'>"
          "<label><input type='checkbox' name='mqtt_clean_session' " + String(cfg->mqtt_clean_session ? "checked" : "") + "> Clean Session</label>"
          "</div>";
  html += "<div class='form-row'>"
          "<label><input type='checkbox' name='mqtt_retain' " + String(cfg->mqtt_retain ? "checked" : "") + "> Retain (für Publishes)</label>"
          "</div>";

  // QoS als Select
  html += "<div class='form-row'><label>QoS</label><select name='mqtt_qos'>";
  for (int q = 0; q <= 2; q++) {
    html += "<option value='" + String(q) + "' " + String(cfg->mqtt_qos == q ? "selected" : "") + ">" + String(q) + "</option>";
  }
  html += "</select></div>";

  html += "<div class='form-row'><label>Keepalive (Sek.)</label>"
          "<input name='mqtt_keepalive' type='number' min='5' max='3600' value='" + String(cfg->mqtt_keepalive) + "'></div>";
  html += "</div>";

  // Actions
  html += "<div class='card'><div class='actions'>"
          "<button class='btn-primary' type='submit'>Speichern</button>"
          "</div></div>";

  html += "</form>";
  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
