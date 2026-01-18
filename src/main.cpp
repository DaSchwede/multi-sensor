#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include "pins.h"
#include <LittleFS.h>
#include "settings.h"
#include "wifi_mgr.h"
#include "bme280_sensor.h"
#include "send_udp.h"
#include "web_server.h"
#include "ntp_time.h"
#include "scd40_sensor.h"
#include <math.h>
#include "sensors_ctrl.h"

static WebServer server(80);
static AppConfig cfg;
static SensorData liveData;
static bool sensorsReady = false;
static bool printed = false;
static bool mdnsStarted = false;
static String gHost;

static uint32_t lastSend = 0;
static uint32_t lastRead = 0;

static uint32_t lastReadMs = 0;
static uint32_t lastSendMs = 0;

static String makeShortId() {
  uint64_t mac = ESP.getEfuseMac();
  uint32_t low = (uint32_t)(mac & 0xFFFFFFFF);
  String chip = String(low, HEX);
  chip.toUpperCase();
  while (chip.length() < 8) chip = "0" + chip;
  return chip.substring(chip.length() - 4); // z.B. A1B2
}

volatile bool gRequestSensorRescan = false;
void requestSensorRescan() { gRequestSensorRescan = true; }

static void doSensorRescan() {
  Serial.println("Rescan: I2C/Sensoren neu initialisieren...");

  // I2C soft reset
  Wire.end();
  delay(50);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  delay(50);

  bool okBme = bmeBegin();
  bool okScd = scdBegin();

  if (!okBme) Serial.println("Rescan: BME280 weiterhin nicht gefunden.");
  if (!okScd) Serial.println("Rescan: SCD40/41 weiterhin nicht gefunden.");

  // Optional: wenn SCD nicht da -> liveData CO2 auf "leer"
  if (!okScd) liveData.co2_ppm = NAN;
}

static bool appStarted = false;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Boot:serial ok");

  Serial.println("LittleFS begin...");
    if (!LittleFS.begin()) {
  Serial.println("LittleFS MOUNT FAIL!");
  } else {
  Serial.printf("LittleFS OK total=%u used=%u\n", LittleFS.totalBytes(), LittleFS.usedBytes());
  Serial.println("style.css exists? " + String(LittleFS.exists("/style.css")));
  }

  if (!settingsBegin()) {
    Serial.println("LittleFS init fehlgeschlagen!");
  }
  loadConfig(cfg);

  // Host / IDs
  String shortId = makeShortId();
  String host = "multi-sensor-" + shortId;
  gHost = host;

  // MQTT Client-ID automatisch setzen, falls leer
  if (cfg.mqtt_client_id.length() == 0) {
    cfg.mqtt_client_id = host;
    saveConfig(cfg);
  }

  // Hostname setzen BEVOR WiFi Begin
  WiFi.setHostname(host.c_str());

  // WiFi-Manager (registriert /wifi + /api/wifi/* auf dem Server)
  wifiMgrBegin(server, "Multi-Sensor");


  // I2C + Sensoren immer initialisieren (unabhängig vom WLAN)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  delay(50);

  if (!bmeBegin()) Serial.println("BME280 nicht gefunden!");
  if (!scdBegin()) Serial.println("SCD40/41 nicht gefunden!");

    // UDP senden
  if (cfg.udp_enabled && (millis() - lastSend >= cfg.send_interval_ms)) {
    lastSend = millis();
    SendUDP(cfg, liveData);
    lastSendMs = millis();
  }

  // Webserver starten (damit /wifi erreichbar ist!)
  webServerBegin(server, cfg, &liveData, &lastReadMs, &lastSendMs);
  Serial.println("Webserver gestartet (inkl. /wifi).");
  
  ntpBegin(cfg);
  Serial.println("App gestartet (Sensoren/NTP aktiv).");
}


void loop() {
  wifiMgrLoop();
  server.handleClient();      // Portal + WebUI bedienen

    if (gRequestSensorRescan) {
    gRequestSensorRescan = false;
    doSensorRescan();
    }

  // Sensor live lesen (immer!)
  if (millis() - lastRead >= 1000) {
    lastRead = millis();

    SensorData b = bmeRead();
    if (!isnan(b.temperature_c)) liveData.temperature_c = b.temperature_c;
    if (!isnan(b.humidity_rh))   liveData.humidity_rh   = b.humidity_rh;
    if (!isnan(b.pressure_hpa))  liveData.pressure_hpa  = b.pressure_hpa;

    SensorData s = scdRead();
    if (!isnan(s.co2_ppm))       liveData.co2_ppm       = s.co2_ppm;

    lastReadMs = millis();
  }

  // Ohne WLAN: kein NTP/UDP/mDNS
  if (!wifiMgrIsConnected()) {
    return;
    static bool printed = false;
    if (WiFi.status() == WL_CONNECTED && !printed) {
      printed = true;
      Serial.printf("WLAN verbunden: %s IP: %s\n",
                WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  }
  if (WiFi.status() != WL_CONNECTED) printed = false;

  }

  // mDNS nur einmal nach Connect starten (bei Reconnect erneut)
  static bool mdnsStarted = false;
  static wl_status_t lastWiFiStatus = WL_IDLE_STATUS;

  wl_status_t st = WiFi.status();
  if (st != lastWiFiStatus) {
    lastWiFiStatus = st;
    if (st != WL_CONNECTED) mdnsStarted = false;
  }

  if (!mdnsStarted) {
    String host = WiFi.getHostname(); // oder dein gHost
    if (MDNS.begin(host.c_str())) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("mDNS aktiv: http://" + host + ".local/");
      mdnsStarted = true;
    } else {
      Serial.println("mDNS Start fehlgeschlagen");
    }
  }

  // NTP/UDP wie früher
  ntpLoop();

  if (millis() - lastSend >= cfg.send_interval_ms) {
    lastSend = millis();
    SendUDP(cfg, liveData);
    lastSendMs = millis();
  }
}
