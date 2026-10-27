#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <LittleFS.h>
#include <math.h>

#include "pins.h"
#include "settings.h"
#include "wifi_mgr.h"
#include "bme280_sensor.h"
#include "scd40_sensor.h"
#include "send_udp.h"
#include "web_server.h"
#include "ntp_time.h"
#include "sensors_ctrl.h"
#include "mqtt_client.h"
#include "logger.h"
#include "device_id.h"

static WebServer server(80);
static AppConfig cfg;
static SensorData liveData;

static bool netStarted  = false;   // NTP/MQTT einmal nach dem ersten Connect starten
static bool mdnsStarted = false;
static String gHost;

static uint32_t lastSend = 0;
static uint32_t lastRead = 0;
static uint32_t lastReadMs = 0;
static uint32_t lastSendMs = 0;
static uint32_t netStableSince = 0;

static bool wifiReallyConnected() {
  return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0,0,0,0);
}

// ===== Sensor-Rescan (wird aus dem Webserver angefordert, im Loop ausgeführt) =====
static volatile bool gRequestSensorRescan = false;
void requestSensorRescan() { gRequestSensorRescan = true; }

// ===== Sensor-Zustand =====
static uint32_t bmeLastOkMs = 0, scdLastOkMs = 0;   // letzter gültiger Messwert
static bool     bmeSeen = false, scdSeen = false;   // seit Start schon einmal gültige Werte?
static float    bmeRawTemp = NAN, bmeRawHum = NAN;
static uint32_t lastAutoRescanMs = 0;
static uint32_t lastPressureCompMs = 0;

static constexpr uint32_t AUTO_RESCAN_MS    = 60000;  // bei Ausfall max. 1x pro Minute neu suchen
static constexpr uint32_t PRESSURE_COMP_MS  = 60000;  // Luftdruck 1x pro Minute an den SCD40

// Plausibilitätsbereiche laut Datenblatt (außerhalb = Lese-/Busfehler)
static bool plausibleTemp(float t)  { return !isnan(t) && t > -40.0f && t < 85.0f; }
static bool plausibleHum(float h)   { return !isnan(h) && h >= 0.0f && h <= 100.0f; }
static bool plausiblePress(float p) { return !isnan(p) && p > 300.0f && p < 1100.0f; }
static bool plausibleCo2(float c)   { return !isnan(c) && c >= 250.0f && c <= 40000.0f; }

// Sättigungsdampfdruck (Magnus-Formel), für die Feuchtekorrektur
static float magnus(float tC) { return 6.112f * expf(17.62f * tC / (243.12f + tC)); }

// Temperatur-Offset anwenden und relative Feuchte passend umrechnen
// (absolute Feuchte bleibt gleich, nur die Temperatur der Luft ist eine andere)
static void applyBmeCorrection(float rawT, float rawH, float &outT, float &outH) {
  const float off = cfg.bme_temp_offset_c;
  outT = rawT + off;
  outH = rawH;
  if (off != 0.0f && !isnan(rawH)) {
    outH = rawH * magnus(rawT) / magnus(outT);
    if (outH > 100.0f) outH = 100.0f;
    if (outH < 0.0f) outH = 0.0f;
  }
}

SensorStatus sensorsGetStatus() {
  SensorStatus st;
  const uint32_t now = millis();
  st.bmeFound = bmeIsOk();
  st.scdFound = scdIsOk();
  st.bmeAgeMs = bmeSeen ? (int32_t)(now - bmeLastOkMs) : -1;
  st.scdAgeMs = scdSeen ? (int32_t)(now - scdLastOkMs) : -1;
  st.bmeRawTempC = bmeRawTemp;
  st.bmeRawHum = bmeRawHum;
  st.scdRunningMs = scdRunningMs();
  st.scdPressureHpa = scdLastPressureHpa();
  return st;
}

static void readSensors() {
  const uint32_t now = millis();

  // --- BME280 ---
  SensorData b = bmeRead();
  if (plausibleTemp(b.temperature_c) && plausibleHum(b.humidity_rh) && plausiblePress(b.pressure_hpa)) {
    bmeRawTemp = b.temperature_c;
    bmeRawHum  = b.humidity_rh;
    applyBmeCorrection(b.temperature_c, b.humidity_rh, liveData.temperature_c, liveData.humidity_rh);
    liveData.pressure_hpa = b.pressure_hpa;
    bmeLastOkMs = now;
    bmeSeen = true;
  }

  // --- SCD40 (liefert nur alle ~5 s neue Werte) ---
  SensorData s = scdRead();
  if (plausibleCo2(s.co2_ppm)) {
    liveData.co2_ppm = s.co2_ppm;
    scdLastOkMs = now;
    scdSeen = true;
  }

  // --- Veraltete Werte verwerfen: lieber "keine Daten" als ein eingefrorener Wert ---
  const bool bmeStale = !bmeSeen || (now - bmeLastOkMs) > SENSOR_STALE_MS;
  const bool scdStale = !scdSeen || (now - scdLastOkMs) > SENSOR_STALE_MS;
  if (bmeStale) {
    liveData.temperature_c = NAN;
    liveData.humidity_rh   = NAN;
    liveData.pressure_hpa  = NAN;
  }
  if (scdStale) liveData.co2_ppm = NAN;

  // --- Ausgefallenen Sensor automatisch neu suchen (nur wenn er schon mal da war) ---
  if (((bmeSeen && bmeStale) || (scdSeen && scdStale)) && (now - lastAutoRescanMs) >= AUTO_RESCAN_MS) {
    lastAutoRescanMs = now;
    Serial.printf("Sensor liefert keine Werte mehr (BME %s, SCD %s) -> automatischer Rescan\n",
                  bmeStale ? "weg" : "ok", scdStale ? "weg" : "ok");
    gRequestSensorRescan = true;
  }

  // --- Luftdruck-Kompensation für den SCD40 ---
  if (!bmeStale && scdIsOk() &&
      (isnan(scdLastPressureHpa()) || (now - lastPressureCompMs) >= PRESSURE_COMP_MS)) {
    if (scdSetAmbientPressure(liveData.pressure_hpa)) lastPressureCompMs = now;
  }
}

static void doSensorRescan() {
  Serial.println("Rescan: I2C/Sensoren neu initialisieren...");

  Wire.end();
  delay(50);
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  delay(50);

  const bool okBme = bmeBegin();
  const bool okScd = scdBegin();

  Serial.printf("Rescan: BME280 %s, SCD40/41 %s\n",
                okBme ? "gefunden" : "NICHT gefunden", okScd ? "gefunden" : "NICHT gefunden");
}

void setup() {
  Serial.begin(115200);
  // ESP32-C3 mit USB im Chip: Nach einem Reset meldet sich der COM-Port neu an.
  // Bis zu 3 s auf den seriellen Monitor warten, damit die Boot-Meldungen nicht verloren gehen.
  // Ohne angeschlossenen Monitor geht es nach 3 s normal weiter.
  const uint32_t serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart) < 3000) delay(10);
  delay(200);
  Serial.println("Boot: serial ok");

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS MOUNT FAIL!");
  } else {
    Serial.printf("LittleFS OK total=%u used=%u\n", LittleFS.totalBytes(), LittleFS.usedBytes());
  }

  settingsBegin();
  loadConfig(cfg);

  // Gerätespezifische IDs (aus den letzten 3 MAC-Bytes -> eindeutig pro Board)
  gHost = "multi-sensor-" + deviceShortId();
  if (settingsApplyDeviceDefaults(cfg)) saveConfig(cfg);
  Serial.println("Sensor-ID: " + cfg.sensor_id + "  Host: " + gHost);

  // Zeitzone sofort setzen (auch ohne NTP stimmt localtime() dann, sobald Zeit da ist)
  ntpApplyTimezone(cfg);

  // Hostname setzen BEVOR WiFi startet
  WiFi.setHostname(gHost.c_str());

  // WiFi-Manager (registriert /wifi + /api/wifi/* auf dem Server)
  wifiMgrBegin(server, "Multi-Sensor");

  // I2C + Sensoren immer initialisieren (unabhängig vom WLAN)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  delay(50);

  if (!bmeBegin()) Serial.println("BME280 nicht gefunden!");
  scdSetAsc(cfg.scd_asc_enabled);   // vor scdBegin(): wird beim Start übernommen
  if (!scdBegin()) Serial.println("SCD40/41 nicht gefunden!");

  webServerBegin(server, cfg, &liveData, &lastReadMs, &lastSendMs);
  Serial.println("Webserver gestartet (inkl. /wifi).");

  loggerBegin(cfg);
}

void loop() {
  wifiMgrLoop();
  server.handleClient();

  if (gRequestSensorRescan) {
    gRequestSensorRescan = false;
    doSensorRescan();
  }

  // Sensoren laufen immer
  if (millis() - lastRead >= 1000) {
    lastRead = millis();
    readSensors();
    lastReadMs = millis();
  }

  // Logger läuft auch offline weiter (Systemzeit bleibt nach einmaligem NTP gültig)
  loggerLoop(cfg, liveData);

  // Ab hier nur mit Netzwerk
  if (wifiReallyConnected()) {
    if (netStableSince == 0) netStableSince = millis();
  } else {
    netStableSince = 0;
    return;
  }

  if (millis() - netStableSince < 5000) return;

  if (!netStarted) {
    netStarted = true;
    mqttBegin(cfg);
    ntpBegin(cfg);
    Serial.printf("WLAN verbunden: %s IP: %s\n",
                  WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str());
  }

  if (!mdnsStarted) {
    if (MDNS.begin(gHost.c_str())) {
      MDNS.addService("http", "tcp", 80);
      mdnsStarted = true;
      Serial.println("mDNS aktiv: http://" + gHost + ".local/");
    }
  }

  mqttLoop(cfg);

  if (millis() - lastSend >= cfg.send_interval_ms) {
    lastSend = millis();
    if (cfg.udp_enabled) SendUDP(cfg, liveData);
    mqttPublishSensors(cfg, liveData);
    lastSendMs = millis();
  }
}
