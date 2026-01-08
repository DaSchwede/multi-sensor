#pragma once
#include <Arduino.h>

struct AppConfig {
  // UDP
  String server_udp_ip = "192.168.1.10";
  uint16_t server_udp_port = 7000;
  uint32_t send_interval_ms = 5000;
  String sensor_id = "sensor01";

  // NTP
  String ntp_server = "pool.ntp.org";
  bool tz_auto_berlin = true;
  int tz_base_seconds = 3600;
  int dst_add_seconds = 3600;

  int udp_format = 0; // 0=CSV, 1=JSON

  // Admin
  // wir speichern einen Hash, nicht Klartext
  String admin_user = "admin";
  String admin_pass_hash;
  bool force_pw_change = true;
  bool license_require_accept = true;  // optional: Login-Checkbox erzwingen

  // Sonstiges
  bool mqtt_enabled = false; // Platzhalter für später

  // UI-Reihenfolge (kommasepariert)
  String ui_root_order = "live,network,time";
  String ui_info_order = "system,network,sensor,memory,time,settings,cfg";

  String ui_info_hide  = ""; // kommasepariert, z.B. "memory,cfg"
  String ui_home_order = "live,network,time"; // Beispiel
  String ui_home_hide  = "";
};

bool settingsBegin();
bool loadConfig(AppConfig &cfg);
bool saveConfig(const AppConfig &cfg);
String defaultAdminHash(); // "admin"



