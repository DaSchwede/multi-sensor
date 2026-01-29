#include "settings.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "ntp_time.h"
#include "crypto_utils.h"

static const char* CFG_FILE = "/config.json";


String defaultAdminHash() {
  // Default-Passwort: "admin" (bitte nach Login ändern!)
  return sha1Hex("admin");
}

bool settingsBegin() {
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

  DynamicJsonDocument doc(12288);

            // <-- doc HIER deklarieren
  auto err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  if (cfg.mqtt_lwt_qos > 1) cfg.mqtt_lwt_qos = 1;

  // ----- ab hier doc benutzen -----
  cfg.log_enabled      = doc["log_enabled"] | cfg.log_enabled;
  cfg.log_interval_min = doc["log_interval_min"] | cfg.log_interval_min;
  cfg.log_metric_mask  = doc["log_metric_mask"] | cfg.log_metric_mask;

  cfg.log_retention_days = doc["log_retention_days"] | cfg.log_retention_days;

  cfg.ui_root_order = doc["ui_root_order"] | cfg.ui_root_order;
  cfg.ui_info_order = doc["ui_info_order"] | cfg.ui_info_order;
  cfg.ui_info_hide  = doc["ui_info_hide"]  | cfg.ui_info_hide;
  cfg.ui_home_order = doc["ui_home_order"] | cfg.ui_home_order;
  cfg.ui_home_hide  = doc["ui_home_hide"]  | cfg.ui_home_hide;

  cfg.device_name = doc["device_name"] | cfg.device_name;

  cfg.server_udp_ip = doc["server_udp_ip"] | cfg.server_udp_ip;
  cfg.server_udp_port = doc["server_udp_port"] | cfg.server_udp_port;
  cfg.send_interval_ms = doc["send_interval_ms"] | cfg.send_interval_ms;
  cfg.sensor_id = doc["sensor_id"] | cfg.sensor_id;

  cfg.udp_enabled = doc["udp_enabled"] | cfg.udp_enabled;

  // NEU: falls noch nicht vorhanden, Default = alles (0x0F)
  cfg.udp_fields_mask = doc["udp_fields_mask"] | cfg.udp_fields_mask;

  cfg.udp_format = doc["udp_format"] | cfg.udp_format;

  cfg.ntp_server = doc["ntp_server"] | cfg.ntp_server;
  cfg.tz_auto_berlin  = doc["tz_auto_berlin"]  | cfg.tz_auto_berlin;
  cfg.tz_base_seconds = doc["tz_base_seconds"] | cfg.tz_base_seconds;
  cfg.dst_add_seconds = doc["dst_add_seconds"] | cfg.dst_add_seconds;

  cfg.license_require_accept = doc["license_require_accept"] | cfg.license_require_accept;

  cfg.admin_user = doc["admin_user"] | cfg.admin_user;

  cfg.admin_pass_hash = doc["admin_pass_hash"] | defaultAdminHash();
  cfg.force_pw_change = doc["force_pw_change"] | true;

  cfg.mqtt_enabled = doc["mqtt_enabled"] | cfg.mqtt_enabled;
    // MQTT (vollständig)
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
  cfg.mqtt_tls_enabled = doc["mqtt_tls_enabled"] | cfg.mqtt_tls_enabled;
  cfg.mqtt_tls_ca      = doc["mqtt_tls_ca"]      | cfg.mqtt_tls_ca;
  cfg.mqtt_lwt_enabled = doc["mqtt_lwt_enabled"] | cfg.mqtt_lwt_enabled;
  cfg.mqtt_lwt_topic   = doc["mqtt_lwt_topic"]   | cfg.mqtt_lwt_topic;
  cfg.mqtt_lwt_online  = doc["mqtt_lwt_online"]  | cfg.mqtt_lwt_online;
  cfg.mqtt_lwt_offline = doc["mqtt_lwt_offline"] | cfg.mqtt_lwt_offline;
  cfg.mqtt_lwt_retain  = doc["mqtt_lwt_retain"]  | cfg.mqtt_lwt_retain;
  cfg.mqtt_lwt_qos     = doc["mqtt_lwt_qos"]     | cfg.mqtt_lwt_qos;
  cfg.mqtt_pub_mask = doc["mqtt_pub_mask"] | cfg.mqtt_pub_mask;
  cfg.mqtt_ha_discovery = doc["mqtt_ha_discovery"] | cfg.mqtt_ha_discovery;
  cfg.mqtt_ha_prefix    = doc["mqtt_ha_prefix"]    | cfg.mqtt_ha_prefix;
  cfg.mqtt_ha_retain    = doc["mqtt_ha_retain"]    | cfg.mqtt_ha_retain;

    // Sanity
  if (cfg.mqtt_port == 0) cfg.mqtt_port = 1883;
  if (cfg.mqtt_qos > 2) cfg.mqtt_qos = 0;
  if (cfg.mqtt_keepalive < 5) cfg.mqtt_keepalive = 30;

  return true;
}


bool saveConfig(const AppConfig &cfg) {
  DynamicJsonDocument doc(12288);

  doc["log_enabled"]      = cfg.log_enabled;
  doc["log_interval_min"] = cfg.log_interval_min;
  doc["log_metric_mask"]  = cfg.log_metric_mask;

  doc["log_retention_days"] = cfg.log_retention_days;

  doc["ui_root_order"] = cfg.ui_root_order;
  doc["ui_info_order"] = cfg.ui_info_order;  
  doc["ui_info_hide"]  = cfg.ui_info_hide;
  doc["ui_home_order"] = cfg.ui_home_order;
  doc["ui_home_hide"]  = cfg.ui_home_hide;

  doc["device_name"] = cfg.device_name;

  doc["server_udp_ip"] = cfg.server_udp_ip;
  doc["server_udp_port"] = cfg.server_udp_port;
  doc["send_interval_ms"] = cfg.send_interval_ms;
  doc["sensor_id"] = cfg.sensor_id;

  doc["udp_enabled"] = cfg.udp_enabled;
  doc["udp_fields_mask"] = cfg.udp_fields_mask;

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
    // MQTT (vollständig)
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
  doc["mqtt_tls_enabled"] = cfg.mqtt_tls_enabled;
  doc["mqtt_tls_ca"]      = cfg.mqtt_tls_ca;
  doc["mqtt_lwt_enabled"] = cfg.mqtt_lwt_enabled;
  doc["mqtt_lwt_topic"]   = cfg.mqtt_lwt_topic;
  doc["mqtt_lwt_online"]  = cfg.mqtt_lwt_online;
  doc["mqtt_lwt_offline"] = cfg.mqtt_lwt_offline;
  doc["mqtt_lwt_retain"]  = cfg.mqtt_lwt_retain;
  doc["mqtt_lwt_qos"]     = cfg.mqtt_lwt_qos;
  doc["mqtt_pub_mask"] = cfg.mqtt_pub_mask;  
  doc["mqtt_ha_discovery"] = cfg.mqtt_ha_discovery;
  doc["mqtt_ha_prefix"]    = cfg.mqtt_ha_prefix;
  doc["mqtt_ha_retain"]    = cfg.mqtt_ha_retain;


  File f = LittleFS.open(CFG_FILE, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}
