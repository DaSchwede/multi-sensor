#pragma once
#include <Arduino.h>
#include "log_bits.h"

// MQTT Publish Flags (Bitmaske)
#define MQTT_PUB_TEMP   (1UL << 0)
#define MQTT_PUB_HUM    (1UL << 1)
#define MQTT_PUB_PRESS  (1UL << 2)
#define MQTT_PUB_CO2    (1UL << 3)

struct AppConfig {

    // MQTT LWT
  bool   mqtt_lwt_enabled = true;
  String mqtt_lwt_topic   = "status";
  String mqtt_lwt_online  = "online";
  String mqtt_lwt_offline = "offline";
  bool   mqtt_lwt_retain  = true;
  uint8_t mqtt_lwt_qos    = 1;

  uint32_t mqtt_pub_mask = MQTT_PUB_TEMP | MQTT_PUB_HUM | MQTT_PUB_PRESS | MQTT_PUB_CO2;

  // MQTT TLS
  bool   mqtt_tls_enabled = false;
  String mqtt_tls_ca;   // PEM Zertifikat

  //Menü
  bool mqtt_ha_discovery = false;
  String mqtt_ha_prefix = "homeassistant";
  bool mqtt_ha_retain = true;
  String device_name = ""; // wenn leer -> sensor_id verwenden

  // UDP
  bool udp_enabled = true;
  String server_udp_ip = "192.168.1.10";
  uint16_t server_udp_port = 7000;
  uint32_t send_interval_ms = 5000;
  String sensor_id = "";          // leer -> beim Start "Sensor-<MAC>" 

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

  // Sensoren
  float bme_temp_offset_c = 0.0f;   // Korrektur BME280-Temperatur (Eigenerwärmung), z.B. -1.5
  bool  scd_asc_enabled   = true;   // SCD40 automatische Selbstkalibrierung

  // Logger
  bool     log_enabled        = false;
  uint16_t log_interval_min   = 30;

  // Default: Temp+Hum+Press+CO2
  uint32_t log_metric_mask    = LOG_TEMP | LOG_HUM | LOG_PRESS | LOG_CO2;

  // 0 = nie löschen
  uint16_t log_retention_days = 30;

  // NTP
  String ntp_server = "pool.ntp.org";
  bool tz_auto_berlin = true;
  int tz_base_seconds = 3600;
  int dst_add_seconds = 3600;

  // Welche Werte sollen gesendet werden? (Bitmaske)
  // Bit 0: Temp, Bit 1: RH, Bit 2: Pressure, Bit 3: CO2
  uint32_t udp_fields_mask = 0x0F;         // <-- NEU (Default: alles)
  
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

// Lädt /config.json (bei Defekt automatisch /config.json.bak)
bool loadConfig(AppConfig &cfg);

// Speichert atomar: erst .tmp schreiben, dann umbenennen (alte Version bleibt als .bak)
bool saveConfig(const AppConfig &cfg);

// Liest eine Konfigurationsdatei in cfg ein (fehlende Keys behalten ihren Wert).
// Gibt false zurück, wenn die Datei kein gültiges Konfigurations-JSON ist.
bool configLoadFromFile(const char *path, AppConfig &cfg);

// Prüft, ob eine Datei eine plausible Konfiguration ist (JSON-Objekt mit Admin-Hash)
bool configFileLooksValid(const char *path);

// Löscht config.json + Backup (Factory Reset)
void configRemoveAll();

// Gerätespezifische Defaults setzen (Sensor-ID, MQTT-Client-ID). true = geändert
bool settingsApplyDeviceDefaults(AppConfig &cfg);

