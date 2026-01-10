#include "wifi_mgr.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <DNSServer.h>
#include <WebServer.h>


static bool wmInitialized = false;

bool wifiEnsureConnected() {
  WiFi.mode(WIFI_STA);

  // Bereits verbunden → nichts tun
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  // WiFiManager nur EINMAL verwenden
  if (!wmInitialized) {
    wmInitialized = true;

    WiFiManager wm;
    wm.setConfigPortalTimeout(180); // 3 Minuten
    wm.setAPCallback([](WiFiManager*) {
      Serial.println("Captive Portal aktiv (AP).");
    });

    if (!wm.autoConnect("Multi-Sensor-Setup")) {
      Serial.println("WLAN nicht verbunden (Portal Timeout).");
      return false;
    }

    Serial.print("WLAN verbunden, IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  // Späterer Reconnect OHNE Portal
  Serial.println("WLAN getrennt, versuche Reconnect...");
  WiFi.reconnect();
  delay(500);
  return (WiFi.status() == WL_CONNECTED);
}
