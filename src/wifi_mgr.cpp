#include "wifi_mgr.h"
#include <WiFi.h>
#include <WiFiManager.h>

// Verhalten tunen
static constexpr uint32_t CONNECT_TIMEOUT_MS   = 8000;   // schneller "silent" Versuch
static constexpr uint32_t RECONNECT_BACKOFF_MS = 5000;   // nicht dauernd reconnect spammen
static constexpr uint32_t PORTAL_TIMEOUT_S     = 180;    // Captive Portal max. 3 min

static bool wmConfigured = false;
static uint32_t lastReconnectAttemptMs = 0;

// Statisches WiFiManager-Objekt (keine Stack-Allokation bei jedem Call)
static WiFiManager wm;

static bool waitForWifi(uint32_t timeoutMs) {
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(10); // auf ESP32 ok (yield)
  }
  return (WiFi.status() == WL_CONNECTED);
}

bool wifiEnsureConnected() {
  // ESP32-C3: empfohlen, NVS-Schreiberei minimieren
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  // Optional: je nach Projekt kann Sleep aus für stabilere Latenzen sorgen
  // WiFi.setSleep(false);

  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  // 1) Silent reconnect (ohne Portal), aber mit Backoff
  const uint32_t now = millis();
  if (now - lastReconnectAttemptMs >= RECONNECT_BACKOFF_MS) {
    lastReconnectAttemptMs = now;

    // Wenn Credentials im NVS liegen, reicht oft begin() ohne Parameter
    WiFi.begin();

    if (waitForWifi(CONNECT_TIMEOUT_MS)) {
      Serial.print("WLAN verbunden (silent), IP: ");
      Serial.println(WiFi.localIP());
      return true;
    }
  }

  // 2) WiFiManager nur einmal konfigurieren
  if (!wmConfigured) {
    wmConfigured = true;

    wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
    wm.setAPCallback([](WiFiManager*) {
      Serial.println("Captive Portal aktiv (AP).");
    });

    // Optional hübscher Hostname (wirkt sich auch auf DHCP-Hostname aus)
    // wm.setHostname("multi-sensor");

    // (Optional) Portal-IP anpassen, wenn du magst:
    // wm.setAPStaticIPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  }

  // 3) Portal nur starten, wenn wir wirklich nicht verbunden sind
  Serial.println("WLAN nicht verbunden, starte WiFiManager-Portal...");
  if (!wm.autoConnect("Multi-Sensor-Setup")) {
    Serial.println("WLAN nicht verbunden (Portal Timeout).");
    return false;
  }

  Serial.print("WLAN verbunden (Portal), IP: ");
  Serial.println(WiFi.localIP());
  return true;
}
