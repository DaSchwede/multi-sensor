#pragma once
#include <Arduino.h>

struct AppConfig {
  // UDP
  String server_udp_ip = "192.168.1.10";
  uint16_t server_udp_port = 7000;
  uint32_t send_interval_ms = 5000;
  String sensor_id = "Sensor-<chipid>";

  // MQTT
  bool   mqtt_enabled        = false;
  String mqtt_host           = "";
  uint16_t mqtt_port         = 1883;
  String mqtt_user           = "";
  String mqtt_pass           = "";          // wird im UI NICHT angezeigt
  String mqtt_client_id      = "";          // wird beim Start automatisch gesetzt, wenn leer
  String mqtt_topic_base     = "multisensor";
  bool   mqtt_retain         = false;
  uint8_t mqtt_qos           = 0;           // 0..2
  uint16_t mqtt_keepalive    = 30;          // Sekunden
  bool   mqtt_clean_session  = true;

  // NTP
  String ntp_server = "pool.ntp.org";
  bool tz_auto_berlin = true;
  int tz_base_seconds = 3600;
  int dst_add_seconds = 3600;

  int udp_format = 0; // 0=CSV, 1=JSON

  // Admin
  String admin_user = "admin";
  String admin_pass_hash;
  bool force_pw_change = true;
  bool license_require_accept = true;

  // UI-Reihenfolge (kommasepariert)
  String ui_root_order = "live,network,time";
  String ui_info_order = "system,network,sensor,memory,time,settings";
  String ui_info_hide  = "";
  String ui_home_order = "live,network,time";
  String ui_home_hide  = "";
};

bool settingsBegin();
bool loadConfig(AppConfig &cfg);
bool saveConfig(const AppConfig &cfg);
String defaultAdminHash();
