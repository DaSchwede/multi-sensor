#include "wifi_mgr.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>

bool wifiEnsureConnected() {
  WiFi.mode(WIFI_STA);

  WiFiManager wm;
  wm.setConfigPortalTimeout(180); // 3 Minuten
  wm.setAPCallback([](WiFiManager*) {
    Serial.println("Captive Portal aktiv (AP).");
  });

  bool ok = wm.autoConnect("Multi-Sensor-Setup"); // optional: passwort setzen
  if (!ok) {
    Serial.println("WLAN nicht verbunden.");
    return false;
  }

  Serial.print("WLAN verbunden, IP: ");
  Serial.println(WiFi.localIP());
  return true;
}
