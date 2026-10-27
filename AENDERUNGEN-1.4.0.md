# Multi-Sensor 1.4.0 – Änderungen

Version 1.4.0 führt zwei Entwicklungsstränge zusammen:
- **GitHub-Stand 1.3.2 vom 25.02.2026** (MQTT-Geräte-ID, NTP-Seite, Klick-Menü, Changelog, WLAN-Manager-Überarbeitung)
- **Überarbeitung vom 27.10.2026** (Fehlerkorrekturen, Sicherheit, neue Seiten – unten im Detail)

Kompiliert ohne Fehler und Warnungen gegen Arduino-ESP32 2.0.17 (ESP32-C3).

**Update:** Firmware hochladen genügt. Ein „Upload Filesystem Image“ ist **nicht nötig** und würde
`config.json` überschreiben. Ausnahme: Wer die neuen Darstellungs-Presets (Karten „udp“/„mqtt“) in
*Einstellungen → Darstellung* nutzen will, braucht die neue `data/script.js` – vorher unbedingt ein Backup der Konfiguration machen.

## Aus dem GitHub-Stand (Feb 2026) übernommen
- Getrennte IDs: **UDP Sensor-ID** (`udp_sensor_id`) und **MQTT Geräte-ID** (`mqtt_device_id`); bestehende `config.json` beider Stände werden gelesen
- MQTT-Status im Klartext und **MQTT-Karte** auf Info/Startseite (Karten-ID `mqtt`, UDP-Karte jetzt `udp`)
- **Info → Zeit / NTP** mit „Jetzt synchronisieren“; Zeiteinstellungen mit Zeitzonen-Auswahl und Angaben in Stunden
- **Menüs per Klick öffnen** (Touch), Hover weiterhin mit Maus
- **Changelog** als Liste auf der Über-Seite (jetzt in der Firmware: `include/changelog.h`, `data/changelog.txt` entfällt)
- WLAN: Setup-AP erst nach **3 Minuten** ohne Verbindung; Aktion „WLAN ändern im Heimnetz“ (10 min, ohne Setup-WLAN)

Nicht übernommen, weil in 1.4.0 anders gelöst: eigener WLAN-Manager (1.4.0 sperrt die WLAN-API ohne Freigabe und nutzt ein
zufälliges Setup-Passwort statt Passwort = SSID), `sensors_ctrl.cpp` (Rescan liegt in `main.cpp`), NTPClient (ersetzt durch SNTP).

**Geändertes Verhalten:** „Fester Offset“ addiert jetzt den Sommerzeit-Zuschlag dauerhaft – so wie es die Oberfläche beschreibt
(vorher wurde er in diesem Modus ignoriert).

## Überarbeitung 27.10.2026

### Behobene Fehler

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

### Sicherheit

- **WLAN-API** `/api/wifi/scan|save|status` nur noch bei aktivem Portal, ohne gespeicherte WLAN-Daten oder im 5-Minuten-Fenster aus „Einstellungen → WLAN“. Vorher konnte jeder im LAN die WLAN-Daten ändern.
- **Setup-AP** ist nach Verbindungsverlust jetzt **WPA2-geschützt**. Das Passwort wird einmalig zufällig erzeugt und steht unter *Einstellungen → WLAN* sowie im seriellen Log. Nur bei der Ersteinrichtung (noch keine WLAN-Daten) ist der AP offen.
- **Neue WLAN-Daten** werden sofort angewendet (vorher blieb das Gerät im alten WLAN, solange es verbunden war).
- **Konfiguration** wird atomar gespeichert (`.tmp` → Umbenennen, alte Version bleibt als `config.json.bak`). Eine defekte `config.json` wird automatisch aus dem Backup wiederhergestellt.
- **Restore** prüft die Datei vor dem Übernehmen (gültiges JSON, Admin-Hash, max. 32 KB). Eine ungültige Datei bleibt ohne Wirkung. Danach werden MQTT und NTP neu gestartet.
- **OTA/Restore-Upload** antworten jetzt immer, auch bei ungültigem Token, Abbruch oder Fehler. Vorher hing der Browser. Das OTA-Token hat 128 Bit Zufall.
- **Session-Cookie** hat 128 Bit Zufall und `HttpOnly`.
- **HTML-Escaping** in den Formularen für MQTT, UDP, Zeit und WLAN sowie in den Info-Karten.

### Neu

##### Seite „Einstellungen → Sensoren“
- Status beider Sensoren mit Alter des letzten Messwerts
- **Temperatur-Offset** für den BME280 (Eigenerwärmung durch den ESP32); die Luftfeuchte wird
  passend umgerechnet (Magnus-Formel). Rohwert und korrigierter Wert werden nebeneinander angezeigt.
- **Luftdruck-Kompensation:** der Luftdruck des BME280 geht jede Minute an den SCD40
- **Automatische Selbstkalibrierung (ASC)** des SCD40 ein/aus
- **Kalibrierung an Frischluft (FRC)** auf einen Referenzwert (Standard 420 ppm); erst möglich,
  wenn der Sensor 3 Minuten ununterbrochen misst

##### Ausgefallene Sensoren werden erkannt
- Werte außerhalb des Messbereichs gelten als Lesefehler
- Nach 30 s ohne gültigen Wert: „keine Daten“ statt eingefrorenem Wert – nicht per UDP/MQTT
  gesendet, im Logger leer
- Ein ausgefallener Sensor wird jede Minute automatisch neu gesucht

##### Verlauf (Info → Logger)
- Ein Diagramm pro Messwert mit eigener Achse, aktuellem Wert und min/Ø/max
- Einzelwerte per Maus/Finger, Messlücken werden als Lücke gezeigt
- Liste der Log-Dateien mit Download als **Excel** (lesbare Zeit, Semikolon, Dezimalkomma) oder Original-CSV
- Aktualisiert sich jede Minute

##### Tools-Seite neu
- Sicherung, Wiederherstellen, Firmware-Update und Werksreset übersichtlich auf einer Seite
- **Administrator-Passwort ändern** (8–64 Zeichen)

##### Sicherheit
- Passwörter werden mit **PBKDF2-SHA256 und Salt** gespeichert statt SHA-1 ohne Salt.
  Bestehende Passwörter werden beim nächsten Login automatisch umgestellt.
- **Sperre nach Fehlversuchen:** ab dem 3. falschen Passwort 5 s, dann jeweils doppelt so lange (max. 5 min)
  – für Login, OTA und Passwort ändern
- Bis zu **4 Geräte gleichzeitig angemeldet**; nach Passwortänderung werden die anderen abgemeldet
- „admin“ ist als neues Passwort nicht mehr erlaubt

##### Bedienung
- **Handy-Menü:** unter 700 px Breite ein Menü-Knopf statt überlaufender Menüleiste
- **Captive Portal:** im Setup-WLAN öffnet sich die Einrichtungsseite automatisch
- **UDP-Format** (Text/CSV oder JSON) wählbar, mit Vorschau des aktuellen Pakets
- Überschrift mit Untertitel auf allen Info- und Einstellungsseiten
- Über-Seite mit Hinweis zur KI-Unterstützung

### Behoben
- „Passwort ändern“ auf der Tools-Seite lieferte „Not found“ (Formular-Ziel war nicht registriert)
- „Sensoren neu erkennen“ springt nicht mehr auf die Tools-Seite, sondern bleibt auf der aktuellen Seite
- Boot-Meldungen gingen im seriellen Monitor verloren (ESP32-C3 USB) – Start wartet jetzt bis zu 3 s auf den Monitor
- Fehlermeldung `config.json.tmp does not exist` beim Start
- Hinweis-Boxen und roter Knopf hatten kein Styling (Klassen fehlten in `style.css`)
- Nach einem Update zeigte der Browser teils alte Styles – CSS/JS-Adressen enthalten jetzt die Firmware-Version

## Bitte beachten beim Update
1. **Hostname/Setup-WLAN** heißen jetzt nach den letzten 3 MAC-Bytes (z.B. `multi-sensor-D3CA2C`). Die IP bleibt gleich.
2. **IDs prüfen:** *Einstellungen → UDP* (Sensor-ID für Loxone) und *Einstellungen → MQTT* (Geräte-ID).
   Die MQTT-Topics lauten `<Topic Base>/<Geräte-ID>/<Wert>`. Wer die Topics vom Februar-Stand (`MultiSensor/<ID>/…`)
   behalten will, setzt Topic Base auf `MultiSensor` und die Geräte-ID auf den alten Wert.
3. **Setup-WLAN-Passwort notieren** (*Einstellungen → WLAN*).
4. Nach dem Update einmal **Strg+F5** im Browser.

## Test-Checkliste
- [ ] Mit bisherigem Passwort anmelden → Monitor: `Admin-Passwort auf PBKDF2 umgestellt.`
- [ ] Einstellungen → UDP / MQTT: IDs stimmen, Loxone und Home Assistant empfangen Werte
- [ ] Info → Zeit / NTP: Uhrzeit, Sommerzeit „Nein“, „Jetzt synchronisieren“ aktualisiert „Letzte Synchronisation“
- [ ] Info → Übersicht: Karten UDP und MQTT
- [ ] Menü am PC (Hover und Klick) und am Handy (Menü-Knopf)
- [ ] Einstellungen → Sensoren: beide „OK“, Luftdruck-Kompensation aktiv
- [ ] Info → Logger: Diagramme, Excel-Download
- [ ] Über: Changelog als Liste
