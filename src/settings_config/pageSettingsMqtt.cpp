#include <Arduino.h>
#include <WebServer.h>
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

void pageSettingsMqtt(WebServer &server) {
  AppConfig* cfg = settingsRequireCfgAndAuth(server);
  if (!cfg) return;

  String msg = "";

  if (server.method() == HTTP_POST) {
        // Zusatz-Features
    cfg->mqtt_ha_discovery = server.hasArg("mqtt_ha_discovery");
    cfg->mqtt_tls_enabled = server.hasArg("mqtt_tls_enabled");
    if (server.hasArg("mqtt_tls_ca")) {
    cfg->mqtt_tls_ca = server.arg("mqtt_tls_ca");
    }
    cfg->mqtt_lwt_enabled = server.hasArg("mqtt_lwt_enabled");

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

    // LWT
    if (server.hasArg("mqtt_lwt_topic"))
    cfg->mqtt_lwt_topic = trimCopy(server.arg("mqtt_lwt_topic"));
    if (server.hasArg("mqtt_lwt_online"))
    cfg->mqtt_lwt_online = server.arg("mqtt_lwt_online");
    if (server.hasArg("mqtt_lwt_offline"))
    cfg->mqtt_lwt_offline = server.arg("mqtt_lwt_offline");

    cfg->mqtt_lwt_qos = clampInt(toIntSafe(server.arg("mqtt_lwt_qos"), cfg->mqtt_lwt_qos),0,1);
    cfg->mqtt_lwt_retain = server.hasArg("mqtt_lwt_retain");


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

        if (server.hasArg("mqtt_ha_prefix")) {
      cfg->mqtt_ha_prefix = trimCopy(server.arg("mqtt_ha_prefix"));
      while (cfg->mqtt_ha_prefix.startsWith("/")) cfg->mqtt_ha_prefix.remove(0, 1);
      while (cfg->mqtt_ha_prefix.endsWith("/")) cfg->mqtt_ha_prefix.remove(cfg->mqtt_ha_prefix.length() - 1);
    }
    cfg->mqtt_ha_retain = server.hasArg("mqtt_ha_retain");


    saveConfig(*cfg);
    msg = "Gespeichert.";
  }

  String html = pagesHeaderAuth("Einstellungen – MQTT", "/settings/mqtt");
  settingsSendOkBadge(html, msg);

  html += "<form method='POST' autocomplete='off'>";

  html += "<div class='card'><h2>MQTT-Konfiguration</h2>";

  html += "<div class='form-row'>"
          "<label>MQTT aktivieren</label>"
          "<label class='switch'>"
          "<input id='sw_mqtt' type='checkbox' name='mqtt_enabled' " + String(cfg->mqtt_enabled ? "checked" : "") + ">"
          "<span class='slider'></span>"
          "</label>"
          "</div>";

  html += "<div class='form-row'>"
          "<label>Home Assistant Auto-Discovery</label>"
          "<label class='switch'>"
          "<input id='sw_ha' type='checkbox' name='mqtt_ha_discovery' " + String(cfg->mqtt_ha_discovery ? "checked" : "") + ">"
          "<span class='slider'></span>"
          "</label>"
          "</div>";

  html += "<div class='form-row'>"
        "<label>TLS aktivieren</label>"
        "<label class='switch'>"
        "<input id='sw_tls' type='checkbox' name='mqtt_tls_enabled' " + String(cfg->mqtt_tls_enabled ? "checked" : "") + ">"
        "<span class='slider'></span>"
        "</label>"
        "</div>";

  html += "<div class='form-row'>"
        "<label>LWT aktivieren</label>"
        "<label class='switch'>"
        "<input id='sw_lwt' type='checkbox' name='mqtt_lwt_enabled' "
        + String(cfg->mqtt_lwt_enabled ? "checked" : "") + ">"
        "<span class='slider'></span>"
        "</label>"
        "</div>";


  html += "</div>";


  // Card: Broker
  html += "<div class='card' data-requires='mqtt'><h2>MQTT-Broker</h2>";
  html += "<div class='form-row'><label>Host / IP</label>"
          "<input name='mqtt_host' placeholder='z.B. 192.168.1.10 oder broker.local' value='" + cfg->mqtt_host + "'></div>";
  html += "<div class='form-row'><label>Port</label>"
          "<input name='mqtt_port' type='number' min='1' max='65535' value='" + String(cfg->mqtt_port) + "'></div>";
  html += "<div class='form-row'><label>Client ID</label>"
          "<input name='mqtt_client_id' placeholder='z.B. multi-sensor-01' value='" + cfg->mqtt_client_id + "'></div>";
  html += "<div class='form-row'><label>Benutzer</label>"
          "<input name='mqtt_user' value='" + cfg->mqtt_user + "'></div>";
  html += "<div class='form-row'><label>Passwort</label>"
          "<input name='mqtt_pass' type='password' placeholder='leer lassen = unverändert'></div>";
  html += "<div class='form-row'>"
          "<label><input type='checkbox' name='mqtt_pass_clear'> Passwort löschen</label>"
          "</div>";
  html += "<div class='form-row'><label>Topic Base</label>"
          "<input name='mqtt_topic_base' placeholder='z.B. multisensor/wohnzimmer' value='" + cfg->mqtt_topic_base + "'></div>";
  html += "<div class='hint'>Beispiel-Publish: <code>" + cfg->mqtt_topic_base + "/temperature</code></div>";
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

  html += "<div class='card' data-requires='mqtt,lwt'><h2>Last Will (LWT)</h2>";

  html += "<div class='form-row'><label>Status Topic</label>"
        "<input name='mqtt_lwt_topic' value='" + cfg->mqtt_lwt_topic + "'></div>";

  html += "<div class='form-row'><label>Payload Online</label>"
        "<input name='mqtt_lwt_online' value='" + cfg->mqtt_lwt_online + "'></div>";

  html += "<div class='form-row'><label>Payload Offline</label>"
        "<input name='mqtt_lwt_offline' value='" + cfg->mqtt_lwt_offline + "'></div>";

  html += "<div class='form-row'><label>QoS</label>"
        "<select name='mqtt_lwt_qos'>"
        "<option value='0' " + String(cfg->mqtt_lwt_qos==0?"selected":"") + ">0</option>"
        "<option value='1' " + String(cfg->mqtt_lwt_qos==1?"selected":"") + ">1</option>"
        "</select></div>";

  html += "<div class='form-row'>"
        "<label>Retain</label>"
        "<label class='switch'>"
        "<input type='checkbox' name='mqtt_lwt_retain' "
        + String(cfg->mqtt_lwt_retain ? "checked":"") + ">"
        "<span class='slider'></span>"
        "</label>"
        "</div>";

  html += "<div class='hint'>"
        "Bei unerwartetem Verbindungsabbruch sendet der Broker automatisch den Offline-Status."
        "</div>";

  html += "</div>";


  html += "<div class='card' data-requires='mqtt,tls'><h2>TLS / Zertifikat</h2>";

  html += "<div class='hint'>"
        "Für TLS (z.B. Port 8883) benötigst du das CA-Zertifikat des MQTT-Brokers."
        "</div>";

  html += "<div class='form-row'><label>CA Zertifikat (PEM)</label>"
        "<textarea name='mqtt_tls_ca' rows='6' placeholder='-----BEGIN CERTIFICATE-----\n...'>"
        + cfg->mqtt_tls_ca +
        "</textarea></div>";

  html += "</div>";

  html += "<div class='card' data-requires='mqtt,ha'><h2>Home Assistant Discovery</h2>";
  html += "<div class='form-row'><label>Prefix Topic</label>"
          "<input name='mqtt_ha_prefix' placeholder='homeassistant' value='" + cfg->mqtt_ha_prefix + "'></div>";
  html += "<div class='form-row'>"
          "<label>Retain für Discovery</label>"
          "<label class='switch'>"
          "<input type='checkbox' name='mqtt_ha_retain' " + String(cfg->mqtt_ha_retain ? "checked" : "") + ">"
          "<span class='slider'></span>"
          "</label>"
          "</div>";
  html += "<div class='hint'>Home Assistant hört typischerweise auf <code>homeassistant/#</code>.</div>";
  html += "</div>";

  // Actions
  html += "<div class='card'><div class='actions'>"
          "<button class='btn-primary' type='submit'>Speichern</button>"
          "</div></div>";

  html += "</form>";
  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
