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

static WebServer server(80);
static AppConfig cfg;
static SensorData liveData;

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

void setup() {
  Serial.begin(115200);
    for (int i=0; i<30; i++) {   // 3 Sekunden
  Serial.printf("BOOT tick %d\n", i);
  delay(100);
    }

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

  // MQTT Client-ID automatisch setzen, falls leer
  if (cfg.mqtt_client_id.length() == 0) {
    cfg.mqtt_client_id = host;   // sauber: gleiche ID wie Hostname
    saveConfig(cfg);
  }

  // WLAN
  WiFi.setHostname(host.c_str());
  wifiEnsureConnected();

  // mDNS NUR EINMAL
  if (MDNS.begin(host.c_str())) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS aktiv: http://" + host + ".local/");
  } else {
    Serial.println("mDNS Start fehlgeschlagen");
  }

  // I2C zentral initialisieren (WICHTIG)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  delay(50);

  // Sensoren (dürfen KEIN Wire.begin mehr machen)
  if (!bmeBegin()) {
    Serial.println("BME280 nicht gefunden (Adresse 0x76/0x77 prüfen)!");
  }

  if (!scdBegin()) {
    Serial.println("SCD40/41 nicht gefunden (Adresse 0x62 prüfen)!");
  }

  ntpBegin(cfg);

  webServerBegin(server, cfg, &liveData, &lastReadMs, &lastSendMs);
  Serial.println("Webserver gestartet.");
}

void loop() {
  webServerLoop(server);
  ntpLoop();

  // Sensor live lesen (z.B. 1x pro Sekunde)
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

  // UDP senden
  if (millis() - lastSend >= cfg.send_interval_ms) {
    lastSend = millis();
    SendUDP(cfg, liveData);
    lastSendMs = millis();
  }

  // WLAN-Reconnect
  if (WiFi.status() == WL_CONNECTED) {
  if (millis() - lastSend >= cfg.send_interval_ms) {
    lastSend = millis();
    SendUDP(cfg, liveData);
    lastSendMs = millis();
  }
}

}
