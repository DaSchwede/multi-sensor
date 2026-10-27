# Multi-Sensor 1.3.1 – Änderungen

Geprüft: Kompiliert und linkt fehlerfrei gegen Arduino-ESP32 2.0.17 (ESP32-C3, gcc 8.4),
ohne Warnungen mit `-Wall -Wextra`. Firmware ≈ 1,27 MB von 1,44 MB OTA-Partition.
Auf echter Hardware noch **nicht** getestet → Checkliste unten.

## Behobene Fehler

| Bereich | Problem | Lösung |
|---|---|---|
| Logger | `loggerLoop()` lief nur einmal nach dem WLAN-Connect | läuft jetzt in jedem `loop()` (auch offline), schreibt an festen Uhrzeit-Rastern (z.B. xx:00 / xx:30) |
| Logger | Schrieb auch bei deaktiviertem Logging | prüft `log_enabled` |
| Logger | Spalten hingen von der Auswahl ab → Diagramm falsch nach Änderung | feste Spalten `epoch,temp_c,hum_rh,press_hpa,co2_ppm`, abgewählte Werte bleiben leer |
| Logger | Button „SD aktualisieren“ tat nichts | ruft `loggerRescan()` auf |
| Sensoren | „Sensoren neu erkennen“ wurde nie ausgeführt | Flag wird im `loop()` ausgewertet |
| UDP | Hostnamen wurden nie aufgelöst (`dnsLocked`) | DNS mit Cache (5 min) und Retry (1 min); IP-Adressen wie bisher |
| MQTT | HA Discovery > 256 Byte → Publish schlug still fehl | Puffer 1024 Byte, Fehler werden geloggt |
| MQTT | Werte-Auswahl ignoriert, `"nan"` gesendet | `mqtt_pub_mask` wird beachtet (neu in der Oberfläche), NaN wird übersprungen |
| MQTT | `mqtt_topic_base` / LWT-Topic ignoriert | Topic = `<Topic Base>/<Sensor-ID>/<Wert>` (Default unverändert `multisensor/<id>/…`) |
| MQTT | Änderungen erst nach Neustart; hängender Zeiger auf Host/CA | `mqttBegin()` wird nach dem Speichern neu aufgerufen, Host/CA als eigene Kopien |
| MQTT | Discovery nur einmal pro Laufzeit | bei jedem Connect + nach HA-Neustart (`homeassistant/status`), `sw_version`/`configuration_url` ergänzt |
| MQTT | Ohne LWT zeigte HA die Sensoren als „nicht verfügbar“ | Availability nur bei aktivem LWT |
| MQTT | Reconnect blockierte alle 5 s | Backoff 5 s → 60 s, Socket-Timeout 5 s |
| IDs | `sensor_id` = Platzhalter `Sensor-<chipid>` auf allen Geräten | wird beim Start durch `Sensor-<letzte 3 MAC-Bytes>` ersetzt |
| IDs | Hostname/AP-Name aus Hersteller-Bytes der MAC (auf allen Boards gleich) | aus den letzten 3 MAC-Bytes |
| Zeit | Keine Zeitzone für `localtime()` → Logger/Diagramme in UTC | SNTP via `configTzTime()` mit POSIX-Regel (EU-Sommerzeit), NTPClient entfernt |
| Zeit | NTP-Server nicht einstellbar, Änderungen erst nach Neustart | Feld in der Oberfläche, sofort aktiv, Anzeige von Ortszeit/Sommerzeit/Regel |
| Verlauf | 12h/24h nur „heute“, 1h ab voller Stunde, 7 Tage/Monat = Fehler 400 | echte Zeitfenster über Tagesgrenzen, 7 Tage/Monat als Tagesmittel, max. 240 Punkte (RAM) |
| Menü | Link `/info/time` führte ins Leere | entfernt |
| Build | `#include "arduino.h"` scheitert auf Linux/CI | `<Arduino.h>` |
| Build | Plattform nicht festgelegt | `espressif32@6.9.0` (Arduino 2.0.17) |

## Sicherheit

- **WLAN-API** `/api/wifi/scan|save|status` nur noch bei aktivem Portal, ohne gespeicherte WLAN-Daten oder im 5-Minuten-Fenster aus „Einstellungen → WLAN“. Vorher konnte jeder im LAN die WLAN-Daten ändern.
- **Setup-AP** ist nach Verbindungsverlust jetzt **WPA2-geschützt**. Das Passwort wird einmalig zufällig erzeugt und steht unter *Einstellungen → WLAN* sowie im seriellen Log. Nur bei der Ersteinrichtung (noch keine WLAN-Daten) ist der AP offen.
- **Neue WLAN-Daten** werden sofort angewendet (vorher blieb das Gerät im alten WLAN, solange es verbunden war).
- **Konfiguration** wird atomar gespeichert (`.tmp` → Umbenennen, alte Version bleibt als `config.json.bak`). Eine defekte `config.json` wird automatisch aus dem Backup wiederhergestellt.
- **Restore** prüft die Datei vor dem Übernehmen (gültiges JSON, Admin-Hash, max. 32 KB). Eine ungültige Datei bleibt ohne Wirkung. Danach werden MQTT und NTP neu gestartet.
- **OTA/Restore-Upload** antworten jetzt immer, auch bei ungültigem Token, Abbruch oder Fehler. Vorher hing der Browser. Das OTA-Token hat 128 Bit Zufall.
- **Session-Cookie** hat 128 Bit Zufall und `HttpOnly`.
- **HTML-Escaping** in den Formularen für MQTT, UDP, Zeit und WLAN sowie in den Info-Karten.

## Bitte beachten beim Update

1. **Hostname und AP-Name ändern sich** einmalig (z.B. `multi-sensor-A1B2` → `multi-sensor-3C4D5E`). Die IP-Adresse bleibt gleich, nur die mDNS-Adresse ist neu.
2. Falls die Sensor-ID noch auf dem Default stand, heißt sie jetzt `Sensor-XXXXXX`. Dadurch ändern sich die **MQTT-Topics**, und in Home Assistant erscheint ein **neues Gerät**. Das alte kannst du löschen. Eine selbst vergebene Sensor-ID bleibt unverändert.
3. **Setup-AP-Passwort notieren** (Einstellungen → WLAN).
4. Die QoS-Auswahl für Publishes wurde entfernt, weil PubSubClient nur QoS 0 senden kann. LWT-QoS bleibt.
5. Die Log-Datei vom Update-Tag kann gemischte Spalten haben, wenn vorher nicht alle 4 Werte ausgewählt waren. Ab dem Folgetag ist alles einheitlich.

## Test-Checkliste am Gerät

- [ ] Flashen: `pio run -t upload` (Firmware) und `pio run -t uploadfs` (unverändert, nur falls nötig)
- [ ] Serieller Monitor: Sensor-ID, Host, Setup-AP-Passwort, `NTP: Server=… TZ=STD-1DST-2,…`
- [ ] Info → Zeit zeigt die richtige Ortszeit, Einstellungen → Zeit zeigt „Sommerzeit aktiv“ korrekt
- [ ] Logger mit 1-Minuten-Intervall: nach einigen Minuten neue Zeilen in `/log/JJJJ-MM-TT.csv`
- [ ] Verlauf 1h / 24h / 7 Tage / Monat lädt ohne Fehler
- [ ] „Sensoren neu erkennen“: Meldung „Rescan …“ im seriellen Log
- [ ] UDP mit Hostname statt IP (DNS-Name aus dem Router; `.local`-Namen werden nicht aufgelöst)
- [ ] MQTT: Topics im Broker prüfen, HA-Geräte erscheinen, Werte-Auswahl wirkt, Host-Änderung ohne Neustart
- [ ] `curl -X POST http://<ip>/api/wifi/save -d "ssid=x"` → **403**
- [ ] Router aus → nach ca. 30–45 s erscheint der Setup-AP **mit Passwort**
- [ ] Backup herunterladen → Restore mit derselben Datei → OK; Restore mit beliebiger anderer Datei → abgelehnt
- [ ] OTA-Update mit `firmware.bin`

## Noch offen (nicht Teil dieses Updates)

- Passwort-Hash: SHA-1 ohne Salt → SHA-256 mit Salt (erfordert Migration)
- Kein Schutz gegen Brute-Force beim Login
- Diagramm: gemeinsame Y-Achse für alle Werte (Luftdruck „plättet“ Temperatur)
- SCD40: Luftdruck-Kompensation aus dem BME280, ASC/Kalibrierung in der Oberfläche
- Captive-Portal-DNS, damit sich die Setup-Seite am Handy automatisch öffnet
- VEML7700 ist in `lib_deps`, aber noch nicht eingebaut
