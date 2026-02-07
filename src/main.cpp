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
#include "mqtt_client.h"
#include "logger.h"

static WebServer server(80);
static AppConfig cfg;
static SensorData liveData;
static bool sensorsReady = false;
static bool printed = false;
static bool netStarted = false;   // NTP/MQTT/mDNS einmal starten pro Connect
static bool mdnsStarted = false;
static String gHost;
static bool wifiReallyConnected() {
  return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0,0,0,0);
}

static uint32_t lastSend = 0;
static uint32_t lastRead = 0;

static uint32_t lastReadMs = 0;
static uint32_t lastSendMs = 0;

static uint32_t netStableSince = 0;

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
    if (!LittleFS.begin(true)) {
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

   // Webserver starten (damit /wifi erreichbar ist!)
  webServerBegin(server, cfg, &liveData, &lastReadMs, &lastSendMs);
  Serial.println("Webserver gestartet (inkl. /wifi).");

  loggerBegin(cfg);
  
}

void loop() {
  wifiMgrLoop();
  server.handleClient();

  // Sensoren dürfen immer laufen
  if (millis() - lastRead >= 1000) {
    lastRead = millis();
    SensorData b = bmeRead();
    if (!isnan(b.temperature_c)) liveData.temperature_c = b.temperature_c;
    if (!isnan(b.humidity_rh))   liveData.humidity_rh   = b.humidity_rh;
    if (!isnan(b.pressure_hpa))  liveData.pressure_hpa  = b.pressure_hpa;
    SensorData s = scdRead();
    if (!isnan(s.co2_ppm)) liveData.co2_ppm = s.co2_ppm;
    lastReadMs = millis();
  }

  // Offline: Netzwerk-Subsysteme stoppen/auslassen
  if (wifiReallyConnected()) {
  if (netStableSince == 0) netStableSince = millis();
  } else {
    netStableSince = 0;
    return;
  }

  // erst nach z.B. 5 Sekunden
  if (millis() - netStableSince < 5000) return;

  // EINMAL nach Connect: MQTT/NTP/mDNS starten
  if (!netStarted) {
    netStarted = true;

    // MQTT/NTP jetzt erst starten
    mqttBegin(cfg);
    ntpBegin(cfg);
    
    loggerLoop(cfg, liveData);

    Serial.printf("WLAN verbunden: %s IP: %s\n",
                  WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str());
  }

  // mDNS nur einmal, wenn wirklich connected
  if (!mdnsStarted) {
    if (MDNS.begin(gHost.c_str())) {
      MDNS.addService("http", "tcp", 80);
      mdnsStarted = true;
      Serial.println("mDNS aktiv: http://" + gHost + ".local/");
    }
  }

  // Netzwerk-Loops
  mqttLoop(cfg);
  ntpLoop();

  // UDP/MQTT Publish nur wenn connected
  if (millis() - lastSend >= cfg.send_interval_ms) {
    lastSend = millis();

    if (cfg.udp_enabled) SendUDP(cfg, liveData);

    mqttPublish(cfg, "temperature", String(liveData.temperature_c, 1));
    mqttPublish(cfg, "humidity",    String(liveData.humidity_rh, 1));
    mqttPublish(cfg, "pressure",    String(liveData.pressure_hpa, 1));
    mqttPublish(cfg, "co2",         String((int)lroundf(liveData.co2_ppm)));

    lastSendMs = millis();
  }
}
