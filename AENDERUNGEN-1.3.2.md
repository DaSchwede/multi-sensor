# Multi-Sensor 1.3.2 – Änderungen

Baut auf 1.3.1 auf. Kompiliert ohne Fehler und Warnungen gegen Arduino-ESP32 2.0.17 (ESP32-C3).
Firmware ≈ 1,32 MB von 1,44 MB OTA-Platz (≈ 88 %).

**Update:** Nur die Firmware hochladen (Upload bzw. OTA). Kein „Upload Filesystem Image“ nötig –
CSS und JavaScript für die neuen Seiten liefert die Firmware selbst aus. Ein Filesystem-Upload würde
`config.json` überschreiben.

## Neu

### Seite „Einstellungen → Sensoren“
- Status beider Sensoren mit Alter des letzten Messwerts
- **Temperatur-Offset** für den BME280 (Eigenerwärmung durch den ESP32); die Luftfeuchte wird
  passend umgerechnet (Magnus-Formel). Rohwert und korrigierter Wert werden nebeneinander angezeigt.
- **Luftdruck-Kompensation:** der Luftdruck des BME280 geht jede Minute an den SCD40
- **Automatische Selbstkalibrierung (ASC)** des SCD40 ein/aus
- **Kalibrierung an Frischluft (FRC)** auf einen Referenzwert (Standard 420 ppm); erst möglich,
  wenn der Sensor 3 Minuten ununterbrochen misst

### Ausgefallene Sensoren werden erkannt
- Werte außerhalb des Messbereichs gelten als Lesefehler
- Nach 30 s ohne gültigen Wert: „keine Daten“ statt eingefrorenem Wert – nicht per UDP/MQTT
  gesendet, im Logger leer
- Ein ausgefallener Sensor wird jede Minute automatisch neu gesucht

### Verlauf (Info → Logger)
- Ein Diagramm pro Messwert mit eigener Achse, aktuellem Wert und min/Ø/max
- Einzelwerte per Maus/Finger, Messlücken werden als Lücke gezeigt
- Liste der Log-Dateien mit Download als **Excel** (lesbare Zeit, Semikolon, Dezimalkomma) oder Original-CSV
- Aktualisiert sich jede Minute

### Tools-Seite neu
- Sicherung, Wiederherstellen, Firmware-Update und Werksreset übersichtlich auf einer Seite
- **Administrator-Passwort ändern** (8–64 Zeichen)

### Sicherheit
- Passwörter werden mit **PBKDF2-SHA256 und Salt** gespeichert statt SHA-1 ohne Salt.
  Bestehende Passwörter werden beim nächsten Login automatisch umgestellt.
- **Sperre nach Fehlversuchen:** ab dem 3. falschen Passwort 5 s, dann jeweils doppelt so lange (max. 5 min)
  – für Login, OTA und Passwort ändern
- Bis zu **4 Geräte gleichzeitig angemeldet**; nach Passwortänderung werden die anderen abgemeldet
- „admin“ ist als neues Passwort nicht mehr erlaubt

### Bedienung
- **Handy-Menü:** unter 700 px Breite ein Menü-Knopf statt überlaufender Menüleiste
- **Captive Portal:** im Setup-WLAN öffnet sich die Einrichtungsseite automatisch
- **UDP-Format** (Text/CSV oder JSON) wählbar, mit Vorschau des aktuellen Pakets
- Überschrift mit Untertitel auf allen Info- und Einstellungsseiten
- Über-Seite mit Hinweis zur KI-Unterstützung

## Behoben
- „Passwort ändern“ auf der Tools-Seite lieferte „Not found“ (Formular-Ziel war nicht registriert)
- „Sensoren neu erkennen“ springt nicht mehr auf die Tools-Seite, sondern bleibt auf der aktuellen Seite
- Boot-Meldungen gingen im seriellen Monitor verloren (ESP32-C3 USB) – Start wartet jetzt bis zu 3 s auf den Monitor
- Fehlermeldung `config.json.tmp does not exist` beim Start
- Hinweis-Boxen und roter Knopf hatten kein Styling (Klassen fehlten in `style.css`)
- Nach einem Update zeigte der Browser teils alte Styles – CSS/JS-Adressen enthalten jetzt die Firmware-Version

## Test-Checkliste
- [ ] Nach dem Update mit dem bisherigen Passwort anmelden → Monitor: `Admin-Passwort auf PBKDF2 umgestellt.`
- [ ] 3× falsches Passwort → „bitte noch 5 s warten“
- [ ] Einstellungen → Sensoren: beide Sensoren „OK“, Luftdruck-Kompensation aktiv
- [ ] SCD40 im Betrieb abziehen → nach ~30 s CO₂ „—“, nach ≤ 1 min automatischer Rescan; wieder anstecken → Werte kommen zurück
- [ ] Info → Logger: Diagramme, Excel-Download öffnen
- [ ] Am Handy: Menü-Knopf, Seiten bedienbar
- [ ] Optional: Einstellungen → WLAN → „WLAN ändern“, mit dem Handy ins Setup-WLAN → Seite öffnet sich automatisch
- [ ] Einstellungen → UDP: Format umstellen, Vorschau prüfen, Loxone empfängt weiterhin

## Noch offen
- Lichtsensor VEML7700 (Platzhalter in Logger/Oberfläche vorhanden)
- Automatischer Build auf GitHub (CI) mit fertiger `firmware.bin`
- `data/logger.js` wird nicht mehr verwendet und kann bei Gelegenheit gelöscht werden;
  ebenso `src/pages/pageSettings_alt.cpp` und `include/api_routes.h` (toter Code)
