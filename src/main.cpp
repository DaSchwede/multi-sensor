#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include "pins.h"
#include <LittleFS.h>
#include "wifi_mgr.h"

#include "settings.h"
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

static bool ipPrinted = false;

static bool netStarted   = false;   // MQTT/NTP starten wir einmal pro Connect
static bool mdnsStarted  = false;
static String gHost;

static uint32_t lastSend    = 0;
static uint32_t lastRead    = 0;
static uint32_t lastReadMs  = 0;
static uint32_t lastSendMs  = 0;

static uint32_t netStableSince = 0;
static uint32_t wifiLostSince = 0;

static String makeShortId() {
  uint64_t mac = ESP.getEfuseMac();
  uint32_t low = (uint32_t)(mac & 0xFFFFFFFF);
  String chip = String(low, HEX);
  chip.toUpperCase();
  while (chip.length() < 8) chip = "0" + chip;
  return chip.substring(chip.length() - 4); // z.B. A1B2
}

static bool wifiReallyConnected() {
  return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0,0,0,0);
}

void wifiResetAndReboot() {
  delay(200);
  ESP.restart();
}

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
  gHost = "multi-sensor-" + shortId;

  // Hostname setzen vor WiFi Start
  WiFi.setHostname(gHost.c_str());
  WiFi.setSleep(false);

  // WiFi Manager initialisieren (STA oder Setup-AP+Portal)
  wifiMgrBegin(server, "Multi-Sensor");

  // MQTT Client-ID automatisch setzen, falls leer
  if (cfg.mqtt_client_id.length() == 0) {
    cfg.mqtt_client_id = gHost;
    saveConfig(cfg);
  }

  // ----------------------------
  // Sensoren / I2C (dürfen immer)
  // ----------------------------
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  delay(50);

  if (!bmeBegin()) Serial.println("BME280 nicht gefunden!");
  if (!scdBegin()) Serial.println("SCD40/41 nicht gefunden!");

  // ----------------------------
  // Webserver NACH WLAN Setup
  // ----------------------------
  webServerBegin(server, cfg, &liveData, &lastReadMs, &lastSendMs);
  Serial.println("Webserver gestartet.");

  // Logger init (falls der intern FS nutzt etc.)
  loggerBegin(cfg);

  // mDNS/NTP/MQTT starten wir kontrolliert in loop() nach "stabil"
  netStarted = false;
  mdnsStarted = false;
  netStableSince = 0;
}

void loop() {
  server.handleClient();
  wifiMgrLoop();

  // Sensor live lesen
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

    // Wenn nicht verbunden: keine Netzwerk-Subsysteme laufen lassen,
  // aber Webserver + Portal müssen weiter laufen (handleClient läuft ja)
  if (!wifiMgrIsConnected()) {
    netStarted = false;
    mdnsStarted = false;
    return;
  }

  if (!ipPrinted && wifiMgrIsConnected()) {
  ipPrinted = true;
  Serial.printf("[NET] STA verbunden: SSID=%s IP=%s RSSI=%d dBm\n",
                WiFi.SSID().c_str(),
                WiFi.localIP().toString().c_str(),
                WiFi.RSSI());
}
if (!wifiMgrIsConnected()) ipPrinted = false;
  
  if (consumeSensorRescanRequest()) {
  sensorsRescanNow();   // <-- Funktion aus sensors_ctrl
  }

  // Einmal nach Connect: MQTT/NTP starten
  if (!netStarted) {
    netStarted = true;
    mqttBegin(cfg);
    ntpBegin(cfg);
    Serial.println("Netzwerk-Subsysteme gestartet (MQTT/NTP).");
  }

  // mDNS einmal starten
  if (!mdnsStarted) {
    if (MDNS.begin(gHost.c_str())) {
      MDNS.addService("http", "tcp", 80);
      mdnsStarted = true;
      Serial.println("mDNS aktiv: http://" + gHost + ".local/");
    } else {
      Serial.println("mDNS Start fehlgeschlagen");
    }
  }

  // Netzwerk-Loops
  mqttLoop(cfg);
  ntpLoop();

  // Logger (wenn du es online loggen willst: hier ist passend)
  loggerLoop(cfg, liveData);

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
