#pragma once
#include "settings.h"
#include "sensor_data.h"

// (Re-)Initialisierung. Darf nach jeder Einstellungsänderung erneut aufgerufen werden:
// trennt eine bestehende Verbindung sauber und übernimmt Host/Port/TLS/Topics neu.
void mqttBegin(const AppConfig &cfg);
void mqttLoop(const AppConfig &cfg);
bool mqttIsConnected();
bool mqttEnsureConnected(const AppConfig &cfg);

// Publiziert unter <topic_base>/<sensor_id>/<subtopic>
bool mqttPublish(const AppConfig &cfg,
                 const String &subtopic,
                 const String &payload,
                 bool retainOverride = false);

// Publiziert alle in mqtt_pub_mask ausgewählten Messwerte (NaN wird übersprungen)
void mqttPublishSensors(const AppConfig &cfg, const SensorData &d);

// Vollständiges Topic für ein Subtopic (für Anzeige in der Weboberfläche)
String mqttTopicFor(const AppConfig &cfg, const String &subtopic);
