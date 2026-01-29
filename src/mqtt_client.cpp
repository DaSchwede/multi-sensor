#include "mqtt_client.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Forward declarations (wichtig für C++)
static String safeSensorId(const AppConfig &cfg);
static String willTopic(const AppConfig &cfg);
static void mqttPublishHADiscovery(const AppConfig &cfg);

static WiFiClient net;
static WiFiClientSecure netSecure;
static PubSubClient mqtt;

static bool useTls = false;
static unsigned long lastReconnect = 0;

static String safeSensorId(const AppConfig &cfg) {
  String sid = cfg.sensor_id;
  sid.trim();
  sid.replace(" ", "_");
  sid.replace("/", "_");
  if (sid.length() == 0) sid = "sensor";
  return sid;
}

static String haPrefix(const AppConfig &cfg) {
  String p = cfg.mqtt_ha_prefix;
  p.trim();
  if (p.length() == 0) p = "homeassistant";
  return p;
}

static String baseTopic(const AppConfig &cfg) {
  // multisensor/<sensor_id>
  return String("multisensor/") + safeSensorId(cfg);
}

static String availabilityTopic(const AppConfig &cfg) {
  // identisch zu LWT-Topic
  return willTopic(cfg);
}

static String willTopic(const AppConfig &cfg) {
  return String("multisensor/") + safeSensorId(cfg) + "/status";
}

void mqttBegin(const AppConfig &cfg) {
  useTls = cfg.mqtt_tls_enabled;

  if (useTls) {
    // Wenn CA vorhanden: verifizieren, sonst unsicher (für Tests praktisch)
    if (cfg.mqtt_tls_ca.length() > 0) {
      netSecure.setCACert(cfg.mqtt_tls_ca.c_str());
    } else {
      netSecure.setInsecure(); // unsicher, aber verhindert "Connect geht nie"
    }
    mqtt.setClient(netSecure);
  } else {
    mqtt.setClient(net);
  }

  mqtt.setServer(cfg.mqtt_host.c_str(), cfg.mqtt_port);

  // Optional: Keepalive aus Config setzen (PubSubClient default 15s)
  mqtt.setKeepAlive(cfg.mqtt_keepalive);
}

void mqttLoop(const AppConfig &cfg) {
  if (!mqttEnsureConnected(cfg)) return;
  mqtt.loop();
}

static bool mqttConnect(const AppConfig &cfg) {
  if (!cfg.mqtt_enabled) return false;

  String cid = cfg.mqtt_client_id;
  cid.trim();
  if (cid.length() == 0) {
    cid = "multi-sensor-" + safeSensorId(cfg);
  }

  String wt = willTopic(cfg);

  bool ok;
  if (cfg.mqtt_lwt_enabled) {
    ok = mqtt.connect(
      cid.c_str(),
      cfg.mqtt_user.c_str(),
      cfg.mqtt_pass.c_str(),
      wt.c_str(),
      cfg.mqtt_lwt_qos,
      cfg.mqtt_lwt_retain,
      cfg.mqtt_lwt_offline.c_str()
    );
  } else {
    ok = mqtt.connect(
      cid.c_str(),
      cfg.mqtt_user.c_str(),
      cfg.mqtt_pass.c_str()
    );
  }

    if (ok && cfg.mqtt_lwt_enabled) {
    mqtt.publish(wt.c_str(), cfg.mqtt_lwt_online.c_str(), cfg.mqtt_lwt_retain);
  }

  if (ok) {
    mqttPublishHADiscovery(cfg);
  }
  

  return ok;
}

bool mqttEnsureConnected(const AppConfig &cfg) {
  if (!cfg.mqtt_enabled) return false;

  if (cfg.mqtt_host.length() == 0) return false;

  if (mqtt.connected()) return true;

  const unsigned long now = millis();
  if (now - lastReconnect < 5000) {
    return false; // Reconnect-Backoff
  }
  lastReconnect = now;

  return mqttConnect(cfg);
}


bool mqttIsConnected() {
  return mqtt.connected() && mqtt.state() == MQTT_CONNECTED;
}

bool mqttPublish(const AppConfig &cfg,
                 const String &subtopic,
                 const String &payload,
                 bool retainOverride) {

  if (!cfg.mqtt_enabled) return false;
  if (!mqtt.connected()) return false;

  String topic = "multisensor/";
  topic += safeSensorId(cfg);
  topic += "/";
  topic += subtopic;

  const bool retain = retainOverride ? true : cfg.mqtt_retain;
  return mqtt.publish(topic.c_str(), payload.c_str(), retain);
}

static void haPublishConfig(const AppConfig &cfg,
                            const String &component,
                            const String &objectId,
                            JsonDocument &doc) {
  String topic = haPrefix(cfg) + "/" + component + "/" + objectId + "/config";

  String payload;
  serializeJson(doc, payload);

  // Discovery immer retain (oder per cfg steuerbar)
  mqtt.publish(topic.c_str(), payload.c_str(), cfg.mqtt_ha_retain);
}

static void mqttPublishHADiscovery(const AppConfig &cfg) {
    bool sentAny = false;
    static bool discoverySent = false;
    if (discoverySent) return;

  if (!cfg.mqtt_enabled) return;
  if (!cfg.mqtt_ha_discovery) return;
  if (!mqtt.connected()) return;

  const String sid = safeSensorId(cfg);
  const String devId = "multisensor_" + sid;        // eindeutige Geräte-ID
  const String base  = baseTopic(cfg);              // multisensor/<sid>
  const String avail = availabilityTopic(cfg);      // multisensor/<sid>/status

  // Gemeinsamer "device"-Block für alle Entitäten
  auto fillDevice = [&](JsonObject device){
    device["identifiers"][0] = devId;
    device["name"] = (cfg.device_name.length() ? cfg.device_name : sid);
    device["manufacturer"] = "Multi-Sensor";
    device["model"] = "ESP32-C3 SuperMini";
    // optional, wenn du eine Version hast:
    // device["sw_version"] = "1.0.0";
  };

  auto common = [&](JsonDocument &doc, const String &name, const String &uniq){
    doc["name"] = name;
    doc["object_id"] = uniq;
    doc["unique_id"] = uniq;
    doc["availability_topic"] = avail;
    doc["payload_available"] = cfg.mqtt_lwt_online;
    doc["payload_not_available"] = cfg.mqtt_lwt_offline;

    JsonObject device = doc["device"].to<JsonObject>();
    fillDevice(device);
  };

  // Temperatur
  if (cfg.mqtt_pub_mask & MQTT_PUB_TEMP) {
    JsonDocument doc;

    common(doc, "Temperature", devId + "_temperature");
    doc["state_topic"] = base + "/temperature";
    doc["unit_of_measurement"] = "°C";
    doc["device_class"] = "temperature";
    doc["state_class"] = "measurement";
    haPublishConfig(cfg, "sensor", devId + "_temperature", doc); sentAny = true;
  }

  // Luftfeuchte
  if (cfg.mqtt_pub_mask & MQTT_PUB_HUM) {
    JsonDocument doc;
    common(doc, "Humidity", devId + "_humidity");
    doc["state_topic"] = base + "/humidity";
    doc["unit_of_measurement"] = "%";
    doc["device_class"] = "humidity";
    doc["state_class"] = "measurement";
    haPublishConfig(cfg, "sensor", devId + "_humidity", doc); sentAny = true;
  }

  // Luftdruck
  if (cfg.mqtt_pub_mask & MQTT_PUB_PRESS) {
    JsonDocument doc;
    common(doc, "Pressure", devId + "_pressure");
    doc["state_topic"] = base + "/pressure";
    doc["unit_of_measurement"] = "hPa";
    doc["device_class"] = "pressure";
    doc["state_class"] = "measurement";
    haPublishConfig(cfg, "sensor", devId + "_pressure", doc); sentAny = true;
  }

  // CO2
  if (cfg.mqtt_pub_mask & MQTT_PUB_CO2) {
    JsonDocument doc;
    common(doc, "CO₂", devId + "_co2");
    doc["state_topic"] = base + "/co2";
    doc["unit_of_measurement"] = "ppm";
    doc["device_class"] = "carbon_dioxide";
    doc["state_class"] = "measurement";
    haPublishConfig(cfg, "sensor", devId + "_co2", doc); sentAny = true;
  }
  if (sentAny) discoverySent = true;
}
