#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include "settings.h"
#include "wifi_mgr.h"
#include "bme280_sensor.h"
#include "loxone_udp.h"
#include "web_server.h"
#include "ntp_time.h"

static ESP8266WebServer server(80);
static AppConfig cfg;
static SensorData liveData;

static uint32_t lastSend = 0;
static uint32_t lastRead = 0;

static uint32_t lastReadMs = 0;
static uint32_t lastSendMs = 0;

void setup() {
  Serial.begin(115200);
  delay(200);

  if (!settingsBegin()) {
    Serial.println("LittleFS init fehlgeschlagen!");
  }
  loadConfig(cfg);

  wifiEnsureConnected();

  WiFi.hostname("multi-sensor");

if (MDNS.begin("multi-sensor")) {
  MDNS.addService("http", "tcp", 80);
  Serial.println("mDNS aktiv: http://multi-sensor.local/");
} else {
  Serial.println("mDNS Start fehlgeschlagen");
}


  if (!bmeBegin()) {
    Serial.println("BME280 nicht gefunden (Adresse 0x76/0x77 prüfen)!");
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
    liveData = bmeRead();
    lastReadMs = millis();
  }

  // UDP senden
  if (millis() - lastSend >= cfg.send_interval_ms) {
    lastSend = millis();
    loxoneSendUDP(cfg, liveData);
    lastSendMs = millis();
  }

  // WLAN-Reconnect (einfach)
  if (WiFi.status() != WL_CONNECTED) {
    wifiEnsureConnected();
  }

  MDNS.update();

}
