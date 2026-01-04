#include "settings.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Hash.h> // ESP8266: sha1()
#include "ntp_time.h"

static const char* CFG_FILE = "/config.json";

// Einfacher Hash (SHA1) – für ESP8266 ok als Start.
// Optional: später auf SHA256 (BearSSL) umstellen.
static String sha1Hex(const String &in) {
  return sha1(in);
}

String defaultAdminHash() {
  // Default-Passwort: "admin" (bitte nach Login ändern!)
  return sha1Hex("admin");
}

bool settingsBegin() {
  if (!LittleFS.begin()) return false;
  return true;
}

bool loadConfig(AppConfig &cfg) {
  if (!LittleFS.exists(CFG_FILE)) {
    cfg.admin_pass_hash = defaultAdminHash();
    cfg.force_pw_change = true;
    return saveConfig(cfg);
  }

  File f = LittleFS.open(CFG_FILE, "r");
  if (!f) return false;

  JsonDocument doc;                  // <-- doc HIER deklarieren
  auto err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  // ----- ab hier doc benutzen -----

  cfg.ui_root_order = doc["ui_root_order"] | cfg.ui_root_order;
  cfg.ui_info_order = doc["ui_info_order"] | cfg.ui_info_order;

  cfg.loxone_ip = doc["loxone_ip"] | cfg.loxone_ip;
  cfg.loxone_udp_port = doc["loxone_udp_port"] | cfg.loxone_udp_port;
  cfg.send_interval_ms = doc["send_interval_ms"] | cfg.send_interval_ms;
  cfg.sensor_id = doc["sensor_id"] | cfg.sensor_id;

  cfg.ntp_server = doc["ntp_server"] | cfg.ntp_server;
  cfg.tz_auto_berlin = doc["tz_auto_berlin"] | true;
  cfg.tz_base_seconds = doc["tz_base_seconds"] | 3600;
  cfg.dst_add_seconds = doc["dst_add_seconds"] | 3600;

  cfg.udp_format = doc["udp_format"] | cfg.udp_format;

  cfg.license_require_accept = doc["license_require_accept"] | cfg.license_require_accept;

  cfg.admin_user = doc["admin_user"] | cfg.admin_user;

  cfg.admin_pass_hash = doc["admin_pass_hash"] | defaultAdminHash();
  cfg.force_pw_change = doc["force_pw_change"] | true;

  cfg.mqtt_enabled = doc["mqtt_enabled"] | cfg.mqtt_enabled;

  cfg.ui_info_order = doc["ui_info_order"] | cfg.ui_info_order;
  cfg.ui_info_hide  = doc["ui_info_hide"]  | cfg.ui_info_hide;
  cfg.ui_home_order = doc["ui_home_order"] | cfg.ui_home_order;
  cfg.ui_home_hide  = doc["ui_home_hide"]  | cfg.ui_home_hide;

  return true;
}


bool saveConfig(const AppConfig &cfg) {
  JsonDocument doc;

  doc["ui_root_order"] = cfg.ui_root_order;
  doc["ui_info_order"] = cfg.ui_info_order;

  doc["loxone_ip"] = cfg.loxone_ip;
  doc["loxone_udp_port"] = cfg.loxone_udp_port;
  doc["send_interval_ms"] = cfg.send_interval_ms;
  doc["sensor_id"] = cfg.sensor_id;

  doc["ntp_server"] = cfg.ntp_server;
  doc["tz_auto_berlin"] = cfg.tz_auto_berlin;
  doc["tz_base_seconds"] = cfg.tz_base_seconds;
  doc["dst_add_seconds"] = cfg.dst_add_seconds;

  doc["udp_format"] = cfg.udp_format;

  doc["license_require_accept"] = cfg.license_require_accept;

  doc["admin_user"] = cfg.admin_user;
  doc["admin_pass_hash"] = cfg.admin_pass_hash;   // 🔴 WICHTIG
  doc["force_pw_change"] = cfg.force_pw_change;

  doc["mqtt_enabled"] = cfg.mqtt_enabled;

  doc["ui_info_order"] = cfg.ui_info_order;
  doc["ui_info_hide"]  = cfg.ui_info_hide;
  doc["ui_home_order"] = cfg.ui_home_order;
  doc["ui_home_hide"]  = cfg.ui_home_hide;


  File f = LittleFS.open(CFG_FILE, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}
