#include "mqtt_client.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <math.h>
#include "version.h"

static WiFiClient        net;
static WiFiClientSecure* netSecure = nullptr;   // wird bei jeder Neu-Konfiguration neu angelegt
static PubSubClient      mqtt;

static const AppConfig* gCfg = nullptr;

// PubSubClient und WiFiClientSecure speichern nur Zeiger auf Host/CA.
// Deshalb eigene Kopien halten, die nur in mqttBegin() (nach disconnect) ersetzt werden.
static String gHost;
static String gCa;
static String gWillTopic;
static String gWillOffline;
static bool   gWillRetain = false;
static bool   gWillEnabled = false;

static uint32_t lastReconnect = 0;
static uint32_t backoffMs = 5000;
static bool     reconnectNow = true;
static bool     discoveryPending = false;

static constexpr uint32_t BACKOFF_MIN_MS = 5000;
static constexpr uint32_t BACKOFF_MAX_MS = 60000;

// ---------- Topics ----------
static String safeSegment(String s) {
  s.trim();
  s.replace(" ", "_");
  s.replace("/", "_");
  s.replace("#", "_");
  s.replace("+", "_");
  return s;
}

static String safeSensorId(const AppConfig &cfg) {
  String sid = safeSegment(cfg.sensor_id);
  if (sid.length() == 0) sid = "sensor";
  return sid;
}

static String cleanBase(const AppConfig &cfg) {
  String b = cfg.mqtt_topic_base;
  b.trim();
  while (b.startsWith("/")) b.remove(0, 1);
  while (b.endsWith("/")) b.remove(b.length() - 1);
  if (b.length() == 0) b = "multisensor";
  return b;
}

// <topic_base>/<sensor_id>  (Default: multisensor/<sensor_id>, wie bisher)
static String baseTopic(const AppConfig &cfg) {
  return cleanBase(cfg) + "/" + safeSensorId(cfg);
}

static String willTopic(const AppConfig &cfg) {
  String t = cfg.mqtt_lwt_topic;
  t.trim();
  while (t.startsWith("/")) t.remove(0, 1);
  if (t.length() == 0) t = "status";
  return baseTopic(cfg) + "/" + t;
}

static String haPrefix(const AppConfig &cfg) {
  String p = cfg.mqtt_ha_prefix;
  p.trim();
  if (p.length() == 0) p = "homeassistant";
  return p;
}

static String haStatusTopic(const AppConfig &cfg) {
  return haPrefix(cfg) + "/status";
}

String mqttTopicFor(const AppConfig &cfg, const String &subtopic) {
  return baseTopic(cfg) + "/" + subtopic;
}

// ---------- Discovery ----------
static bool haPublishConfig(const AppConfig &cfg,
                            const String &component,
                            const String &objectId,
                            JsonDocument &doc) {
  const String topic = haPrefix(cfg) + "/" + component + "/" + objectId + "/config";
  String payload;
  serializeJson(doc, payload);
  const bool ok = mqtt.publish(topic.c_str(), payload.c_str(), cfg.mqtt_ha_retain);
  if (!ok) Serial.printf("MQTT: Discovery fehlgeschlagen (%u Bytes): %s\n",
                         (unsigned)payload.length(), topic.c_str());
  return ok;
}

static bool mqttPublishHADiscovery(const AppConfig &cfg) {
  if (!cfg.mqtt_enabled || !cfg.mqtt_ha_discovery || !mqtt.connected()) return false;

  const String sid   = safeSensorId(cfg);
  const String devId = "multisensor_" + sid;
  const String base  = baseTopic(cfg);
  const String avail = willTopic(cfg);

  auto common = [&](JsonDocument &doc, const char *name, const String &uniq, const String &sub) {
    doc["name"] = name;
    doc["object_id"] = uniq;
    doc["unique_id"] = uniq;
    doc["state_topic"] = base + "/" + sub;
    doc["state_class"] = "measurement";
    if (cfg.mqtt_lwt_enabled) {   // ohne LWT gibt es kein Status-Topic -> sonst "nicht verfügbar"
      doc["availability_topic"] = avail;
      doc["payload_available"] = cfg.mqtt_lwt_online;
      doc["payload_not_available"] = cfg.mqtt_lwt_offline;
    }
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = devId;
    device["name"] = (cfg.device_name.length() ? cfg.device_name : sid);
    device["manufacturer"] = "Multi-Sensor";
    device["model"] = "ESP32-C3 SuperMini";
    device["sw_version"] = FW_VERSION;
    device["configuration_url"] = "http://" + WiFi.localIP().toString() + "/";
  };

  struct Ent { uint32_t bit; const char *name; const char *sub; const char *unit; const char *cls; };
  static const Ent ents[] = {
    { MQTT_PUB_TEMP,  "Temperatur",  "temperature", "°C",  "temperature"    },
    { MQTT_PUB_HUM,   "Luftfeuchte", "humidity",    "%",   "humidity"       },
    { MQTT_PUB_PRESS, "Luftdruck",   "pressure",    "hPa", "pressure"       },
    { MQTT_PUB_CO2,   "CO₂",         "co2",         "ppm", "carbon_dioxide" },
  };

  bool allOk = true;
  for (const auto &e : ents) {
    const String uniq = devId + "_" + e.sub;
    if (cfg.mqtt_pub_mask & e.bit) {
      JsonDocument doc;
      common(doc, e.name, uniq, e.sub);
      doc["unit_of_measurement"] = e.unit;
      doc["device_class"] = e.cls;
      allOk &= haPublishConfig(cfg, "sensor", uniq, doc);
    } else {
      // abgewählte Werte in HA entfernen (leere retained Config)
      const String topic = haPrefix(cfg) + "/sensor/" + uniq + "/config";
      mqtt.publish(topic.c_str(), "", true);
    }
    mqtt.loop();
  }
  return allOk;
}

static void onMessage(char *topic, byte *payload, unsigned int len) {
  if (!gCfg) return;
  if (haStatusTopic(*gCfg) != topic) return;
  String p;
  p.reserve(len);
  for (unsigned int i = 0; i < len; i++) p += (char)payload[i];
  if (p == "online") discoveryPending = true;  // HA wurde neu gestartet -> Discovery erneut senden
}

// ---------- Verbindung ----------
void mqttBegin(const AppConfig &cfg) {
  gCfg = &cfg;

  // bestehende Verbindung sauber beenden (Offline-Status selbst senden, LWT greift nur bei Abbruch)
  if (mqtt.connected()) {
    if (gWillEnabled && gWillTopic.length())
      mqtt.publish(gWillTopic.c_str(), gWillOffline.c_str(), gWillRetain);
    mqtt.disconnect();
  }

  gHost = cfg.mqtt_host;
  gHost.trim();
  gCa = cfg.mqtt_tls_ca;
  gCa.trim();

  WiFiClientSecure *oldSecure = netSecure;
  netSecure = nullptr;

  if (cfg.mqtt_tls_enabled) {
    // Neues Objekt: WiFiClientSecure kann von setInsecure() nicht zurück auf CA-Prüfung
    netSecure = new WiFiClientSecure();
    if (gCa.length() > 0) {
      netSecure->setCACert(gCa.c_str());
    } else {
      netSecure->setInsecure(); // ohne CA keine Zertifikatsprüfung (nur für Tests!)
    }
    mqtt.setClient(*netSecure);
  } else {
    mqtt.setClient(net);
  }

  if (oldSecure) {
    oldSecure->stop();
    delete oldSecure;
  }
  net.stop();

  mqtt.setServer(gHost.c_str(), cfg.mqtt_port);
  mqtt.setKeepAlive(cfg.mqtt_keepalive);
  mqtt.setSocketTimeout(5);
  mqtt.setBufferSize(1024);           // Default 256 Bytes ist zu klein für HA Discovery
  mqtt.setCallback(onMessage);

  backoffMs = BACKOFF_MIN_MS;
  reconnectNow = true;
}

static bool mqttConnect(const AppConfig &cfg) {
  String cid = cfg.mqtt_client_id;
  cid.trim();
  if (cid.length() == 0) cid = "multi-sensor-" + safeSensorId(cfg);

  gWillTopic   = willTopic(cfg);
  gWillOffline = cfg.mqtt_lwt_offline;
  gWillRetain  = cfg.mqtt_lwt_retain;
  gWillEnabled = cfg.mqtt_lwt_enabled;

  const char *user = cfg.mqtt_user.length() ? cfg.mqtt_user.c_str() : nullptr;
  const char *pass = cfg.mqtt_user.length() ? cfg.mqtt_pass.c_str() : nullptr;

  bool ok;
  if (cfg.mqtt_lwt_enabled) {
    ok = mqtt.connect(cid.c_str(), user, pass,
                      gWillTopic.c_str(), cfg.mqtt_lwt_qos, cfg.mqtt_lwt_retain,
                      cfg.mqtt_lwt_offline.c_str(), cfg.mqtt_clean_session);
  } else {
    ok = mqtt.connect(cid.c_str(), user, pass,
                      nullptr, 0, false, nullptr, cfg.mqtt_clean_session);
  }

  if (!ok) {
    Serial.printf("MQTT: Verbindung zu %s:%u fehlgeschlagen (state=%d)\n",
                  gHost.c_str(), cfg.mqtt_port, mqtt.state());
    return false;
  }

  Serial.println("MQTT verbunden: " + gHost);
  if (cfg.mqtt_lwt_enabled) {
    mqtt.publish(gWillTopic.c_str(), cfg.mqtt_lwt_online.c_str(), cfg.mqtt_lwt_retain);
  }
  if (cfg.mqtt_ha_discovery) {
    mqtt.subscribe(haStatusTopic(cfg).c_str());
    discoveryPending = true;
  }
  return true;
}

bool mqttEnsureConnected(const AppConfig &cfg) {
  if (!cfg.mqtt_enabled || gHost.length() == 0) {
    if (mqtt.connected()) mqtt.disconnect();
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) return false;
  if (mqtt.connected()) return true;

  const uint32_t now = millis();
  if (!reconnectNow && (now - lastReconnect) < backoffMs) return false;
  reconnectNow = false;
  lastReconnect = now;

  const bool ok = mqttConnect(cfg);
  if (ok) {
    backoffMs = BACKOFF_MIN_MS;
  } else {
    backoffMs = backoffMs * 2;
    if (backoffMs > BACKOFF_MAX_MS) backoffMs = BACKOFF_MAX_MS;
  }
  return ok;
}

void mqttLoop(const AppConfig &cfg) {
  if (!mqttEnsureConnected(cfg)) return;
  mqtt.loop();

  if (discoveryPending) {
    discoveryPending = false;   // nur einmal pro Verbindung bzw. HA-Neustart versuchen
    if (cfg.mqtt_ha_discovery) mqttPublishHADiscovery(cfg);
  }
}

bool mqttIsConnected() {
  return mqtt.connected();
}

bool mqttPublish(const AppConfig &cfg,
                 const String &subtopic,
                 const String &payload,
                 bool retainOverride) {
  if (!cfg.mqtt_enabled) return false;
  if (!mqtt.connected()) return false;

  const String topic = mqttTopicFor(cfg, subtopic);
  const bool retain = retainOverride ? true : cfg.mqtt_retain;
  return mqtt.publish(topic.c_str(), payload.c_str(), retain);
}

void mqttPublishSensors(const AppConfig &cfg, const SensorData &d) {
  if (!cfg.mqtt_enabled || !mqtt.connected()) return;

  auto pub = [&](uint32_t bit, const char *sub, float v, unsigned int decimals) {
    if (!(cfg.mqtt_pub_mask & bit)) return;
    if (isnan(v)) return;                       // kein "nan" an HA/Loxone schicken
    mqttPublish(cfg, sub, String(v, decimals));
  };

  pub(MQTT_PUB_TEMP,  "temperature", d.temperature_c, 1);
  pub(MQTT_PUB_HUM,   "humidity",    d.humidity_rh,   1);
  pub(MQTT_PUB_PRESS, "pressure",    d.pressure_hpa,  1);
  pub(MQTT_PUB_CO2,   "co2",         d.co2_ppm,       0);
}
