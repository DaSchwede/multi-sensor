#include "settings.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "crypto_utils.h"
#include "device_id.h"
#include <math.h>

static const char* CFG_FILE = "/config.json";
static const char* CFG_TMP  = "/config.json.tmp";
static const char* CFG_BAK  = "/config.json.bak";

static String defaultAdminHash() {
  // Default-Passwort: "admin" (wird beim ersten Login zwangsweise geändert)
  return sha1Hex("admin");
}

bool settingsBegin() {
  return true;
}

static void sanitize(AppConfig &cfg) {
  if (cfg.mqtt_port == 0) cfg.mqtt_port = 1883;
  if (cfg.mqtt_qos > 2) cfg.mqtt_qos = 0;
  if (cfg.mqtt_lwt_qos > 1) cfg.mqtt_lwt_qos = 1;
  if (cfg.mqtt_keepalive < 5) cfg.mqtt_keepalive = 30;
  if (cfg.server_udp_port == 0) cfg.server_udp_port = 7000;
  if (cfg.send_interval_ms < 1000) cfg.send_interval_ms = 1000;
  if (cfg.send_interval_ms > 3600000UL) cfg.send_interval_ms = 3600000UL;
  if (cfg.log_interval_min < 1) cfg.log_interval_min = 30;
  if (cfg.udp_format < 0 || cfg.udp_format > 1) cfg.udp_format = 0;
  if (isnan(cfg.bme_temp_offset_c) || cfg.bme_temp_offset_c < -10.0f || cfg.bme_temp_offset_c > 10.0f) cfg.bme_temp_offset_c = 0.0f;
  cfg.sensor_id.trim();
  cfg.mqtt_device_id.trim();
}

bool configLoadFromFile(const char *path, AppConfig &cfg) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;

  JsonDocument doc;
  auto err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  if (!doc.is<JsonObject>()) return false;

  cfg.log_enabled        = doc["log_enabled"] | cfg.log_enabled;
  cfg.log_interval_min   = doc["log_interval_min"] | cfg.log_interval_min;
  cfg.log_metric_mask    = doc["log_metric_mask"] | cfg.log_metric_mask;
  cfg.log_retention_days = doc["log_retention_days"] | cfg.log_retention_days;

  cfg.ui_root_order = doc["ui_root_order"] | cfg.ui_root_order;
  cfg.ui_info_order = doc["ui_info_order"] | cfg.ui_info_order;
  cfg.ui_info_hide  = doc["ui_info_hide"]  | cfg.ui_info_hide;
  cfg.ui_home_order = doc["ui_home_order"] | cfg.ui_home_order;
  cfg.ui_home_hide  = doc["ui_home_hide"]  | cfg.ui_home_hide;

  cfg.device_name = doc["device_name"] | cfg.device_name;

  cfg.bme_temp_offset_c = doc["bme_temp_offset_c"] | cfg.bme_temp_offset_c;
  cfg.scd_asc_enabled   = doc["scd_asc_enabled"]   | cfg.scd_asc_enabled;

  cfg.server_udp_ip    = doc["server_udp_ip"] | cfg.server_udp_ip;
  cfg.server_udp_port  = doc["server_udp_port"] | cfg.server_udp_port;
  cfg.send_interval_ms = doc["send_interval_ms"] | cfg.send_interval_ms;
  // UDP Sensor-ID: Key "udp_sensor_id" (seit Feb 2026), ältere Configs nutzen "sensor_id"
  if (doc["udp_sensor_id"].is<const char*>()) cfg.sensor_id = doc["udp_sensor_id"] | cfg.sensor_id;
  else                                        cfg.sensor_id = doc["sensor_id"]     | cfg.sensor_id;
  cfg.mqtt_device_id   = doc["mqtt_device_id"] | cfg.mqtt_device_id;

  cfg.udp_enabled     = doc["udp_enabled"] | cfg.udp_enabled;
  cfg.udp_fields_mask = doc["udp_fields_mask"] | cfg.udp_fields_mask;
  cfg.udp_format      = doc["udp_format"] | cfg.udp_format;

  cfg.ntp_server      = doc["ntp_server"] | cfg.ntp_server;
  cfg.tz_auto_berlin  = doc["tz_auto_berlin"]  | cfg.tz_auto_berlin;
  cfg.tz_base_seconds = doc["tz_base_seconds"] | cfg.tz_base_seconds;
  cfg.dst_add_seconds = doc["dst_add_seconds"] | cfg.dst_add_seconds;

  cfg.license_require_accept = doc["license_require_accept"] | cfg.license_require_accept;

  cfg.admin_user      = doc["admin_user"] | cfg.admin_user;
  cfg.admin_pass_hash = doc["admin_pass_hash"] | defaultAdminHash();
  cfg.force_pw_change = doc["force_pw_change"] | true;

  cfg.mqtt_enabled       = doc["mqtt_enabled"]       | cfg.mqtt_enabled;
  cfg.mqtt_host          = doc["mqtt_host"]          | cfg.mqtt_host;
  cfg.mqtt_port          = doc["mqtt_port"]          | cfg.mqtt_port;
  cfg.mqtt_user          = doc["mqtt_user"]          | cfg.mqtt_user;
  cfg.mqtt_pass          = doc["mqtt_pass"]          | cfg.mqtt_pass;
  cfg.mqtt_client_id     = doc["mqtt_client_id"]     | cfg.mqtt_client_id;
  cfg.mqtt_topic_base    = doc["mqtt_topic_base"]    | cfg.mqtt_topic_base;
  cfg.mqtt_retain        = doc["mqtt_retain"]        | cfg.mqtt_retain;
  cfg.mqtt_qos           = doc["mqtt_qos"]           | cfg.mqtt_qos;
  cfg.mqtt_keepalive     = doc["mqtt_keepalive"]     | cfg.mqtt_keepalive;
  cfg.mqtt_clean_session = doc["mqtt_clean_session"] | cfg.mqtt_clean_session;
  cfg.mqtt_tls_enabled   = doc["mqtt_tls_enabled"]   | cfg.mqtt_tls_enabled;
  cfg.mqtt_tls_ca        = doc["mqtt_tls_ca"]        | cfg.mqtt_tls_ca;
  cfg.mqtt_lwt_enabled   = doc["mqtt_lwt_enabled"]   | cfg.mqtt_lwt_enabled;
  cfg.mqtt_lwt_topic     = doc["mqtt_lwt_topic"]     | cfg.mqtt_lwt_topic;
  cfg.mqtt_lwt_online    = doc["mqtt_lwt_online"]    | cfg.mqtt_lwt_online;
  cfg.mqtt_lwt_offline   = doc["mqtt_lwt_offline"]   | cfg.mqtt_lwt_offline;
  cfg.mqtt_lwt_retain    = doc["mqtt_lwt_retain"]    | cfg.mqtt_lwt_retain;
  cfg.mqtt_lwt_qos       = doc["mqtt_lwt_qos"]       | cfg.mqtt_lwt_qos;
  cfg.mqtt_pub_mask      = doc["mqtt_pub_mask"]      | cfg.mqtt_pub_mask;
  cfg.mqtt_ha_discovery  = doc["mqtt_ha_discovery"]  | cfg.mqtt_ha_discovery;
  cfg.mqtt_ha_prefix     = doc["mqtt_ha_prefix"]     | cfg.mqtt_ha_prefix;
  cfg.mqtt_ha_retain     = doc["mqtt_ha_retain"]     | cfg.mqtt_ha_retain;

  sanitize(cfg);
  return true;
}

bool configFileLooksValid(const char *path) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  JsonDocument doc;
  auto err = deserializeJson(doc, f);
  f.close();
  if (err || !doc.is<JsonObject>()) return false;
  const char* h = doc["admin_pass_hash"] | "";
  return passwordHashLooksValid(String(h));   // alter SHA-1- oder neuer PBKDF2-Hash
}

bool loadConfig(AppConfig &cfg) {
  // Reste eines unterbrochenen Speicherns aufräumen
  if (!LittleFS.exists(CFG_FILE) && LittleFS.exists(CFG_BAK)) {
    Serial.println("config.json fehlt -> stelle config.json.bak wieder her");
    LittleFS.rename(CFG_BAK, CFG_FILE);
  }
  // remove() ohne exists(): exists() loggt bei fehlender Datei einen [E]-Eintrag
  LittleFS.remove(CFG_TMP);

  if (!LittleFS.exists(CFG_FILE)) {
    cfg.admin_pass_hash = defaultAdminHash();
    cfg.force_pw_change = true;
    return saveConfig(cfg);
  }

  if (configLoadFromFile(CFG_FILE, cfg)) return true;

  Serial.println("config.json defekt -> versuche config.json.bak");
  if (LittleFS.exists(CFG_BAK)) {
    AppConfig fromBak;
    if (configLoadFromFile(CFG_BAK, fromBak)) {
      cfg = fromBak;
      saveConfig(cfg);
      return true;
    }
  }

  // Nichts Lesbares vorhanden: Defaults mit Passwort-Zwang
  Serial.println("Keine gültige Konfiguration gefunden -> Defaults");
  cfg = AppConfig();
  cfg.admin_pass_hash = defaultAdminHash();
  cfg.force_pw_change = true;
  return false;
}

bool saveConfig(const AppConfig &cfg) {
  JsonDocument doc;

  doc["log_enabled"]        = cfg.log_enabled;
  doc["log_interval_min"]   = cfg.log_interval_min;
  doc["log_metric_mask"]    = cfg.log_metric_mask;
  doc["log_retention_days"] = cfg.log_retention_days;

  doc["ui_root_order"] = cfg.ui_root_order;
  doc["ui_info_order"] = cfg.ui_info_order;
  doc["ui_info_hide"]  = cfg.ui_info_hide;
  doc["ui_home_order"] = cfg.ui_home_order;
  doc["ui_home_hide"]  = cfg.ui_home_hide;

  doc["device_name"] = cfg.device_name;

  doc["bme_temp_offset_c"] = cfg.bme_temp_offset_c;
  doc["scd_asc_enabled"]   = cfg.scd_asc_enabled;

  doc["server_udp_ip"]    = cfg.server_udp_ip;
  doc["server_udp_port"]  = cfg.server_udp_port;
  doc["send_interval_ms"] = cfg.send_interval_ms;
  doc["udp_sensor_id"]    = cfg.sensor_id;
  doc["sensor_id"]        = cfg.sensor_id;   // für ältere Firmware-Stände (Downgrade)
  doc["mqtt_device_id"]   = cfg.mqtt_device_id;

  doc["udp_enabled"]     = cfg.udp_enabled;
  doc["udp_fields_mask"] = cfg.udp_fields_mask;
  doc["udp_format"]      = cfg.udp_format;

  doc["ntp_server"]      = cfg.ntp_server;
  doc["tz_auto_berlin"]  = cfg.tz_auto_berlin;
  doc["tz_base_seconds"] = cfg.tz_base_seconds;
  doc["dst_add_seconds"] = cfg.dst_add_seconds;

  doc["license_require_accept"] = cfg.license_require_accept;

  doc["admin_user"]      = cfg.admin_user;
  doc["admin_pass_hash"] = cfg.admin_pass_hash;
  doc["force_pw_change"] = cfg.force_pw_change;

  doc["mqtt_enabled"]       = cfg.mqtt_enabled;
  doc["mqtt_host"]          = cfg.mqtt_host;
  doc["mqtt_port"]          = cfg.mqtt_port;
  doc["mqtt_user"]          = cfg.mqtt_user;
  doc["mqtt_pass"]          = cfg.mqtt_pass;
  doc["mqtt_client_id"]     = cfg.mqtt_client_id;
  doc["mqtt_topic_base"]    = cfg.mqtt_topic_base;
  doc["mqtt_retain"]        = cfg.mqtt_retain;
  doc["mqtt_qos"]           = cfg.mqtt_qos;
  doc["mqtt_keepalive"]     = cfg.mqtt_keepalive;
  doc["mqtt_clean_session"] = cfg.mqtt_clean_session;
  doc["mqtt_tls_enabled"]   = cfg.mqtt_tls_enabled;
  doc["mqtt_tls_ca"]        = cfg.mqtt_tls_ca;
  doc["mqtt_lwt_enabled"]   = cfg.mqtt_lwt_enabled;
  doc["mqtt_lwt_topic"]     = cfg.mqtt_lwt_topic;
  doc["mqtt_lwt_online"]    = cfg.mqtt_lwt_online;
  doc["mqtt_lwt_offline"]   = cfg.mqtt_lwt_offline;
  doc["mqtt_lwt_retain"]    = cfg.mqtt_lwt_retain;
  doc["mqtt_lwt_qos"]       = cfg.mqtt_lwt_qos;
  doc["mqtt_pub_mask"]      = cfg.mqtt_pub_mask;
  doc["mqtt_ha_discovery"]  = cfg.mqtt_ha_discovery;
  doc["mqtt_ha_prefix"]     = cfg.mqtt_ha_prefix;
  doc["mqtt_ha_retain"]     = cfg.mqtt_ha_retain;

  // 1) In temporäre Datei schreiben
  File f = LittleFS.open(CFG_TMP, "w");
  if (!f) return false;
  const size_t want = measureJson(doc);
  const size_t written = serializeJson(doc, f);
  f.close();
  if (written == 0 || written != want) {
    Serial.println("saveConfig: Schreiben unvollständig, abgebrochen");
    LittleFS.remove(CFG_TMP);
    return false;
  }

  // 2) Alte Datei als Backup behalten, neue an ihren Platz
  if (LittleFS.exists(CFG_FILE)) {
    LittleFS.remove(CFG_BAK);
    if (!LittleFS.rename(CFG_FILE, CFG_BAK)) {
      LittleFS.remove(CFG_TMP);
      return false;
    }
  }
  if (!LittleFS.rename(CFG_TMP, CFG_FILE)) {
    // zurückrollen
    if (LittleFS.exists(CFG_BAK)) LittleFS.rename(CFG_BAK, CFG_FILE);
    return false;
  }
  return true;
}

void configRemoveAll() {
  LittleFS.remove(CFG_FILE);
  LittleFS.remove(CFG_BAK);
  LittleFS.remove(CFG_TMP);
}

bool settingsApplyDeviceDefaults(AppConfig &cfg) {
  bool changed = false;
  const String shortId = deviceShortId();

  // Alter Platzhalter "Sensor-<chipid>" wurde früher nie ersetzt
  cfg.sensor_id.trim();
  if (cfg.sensor_id.length() == 0 || cfg.sensor_id.indexOf('<') >= 0) {
    cfg.sensor_id = "Sensor-" + shortId;
    changed = true;
  }
  if (cfg.mqtt_client_id.length() == 0) {
    cfg.mqtt_client_id = "multi-sensor-" + shortId;
    changed = true;
  }
  return changed;
}
