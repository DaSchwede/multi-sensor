# Multi-Sensor (ESP8266)

Ein modularer, webbasierter **Multi-Sensor** auf Basis eines **ESP8266 (D1 mini / lite)**  
mit **BME280**, Weboberfläche, Login-System, NTP-Zeit, UDP-Ausgabe (z.B. für Loxone)  
und Dateisystem (LittleFS).

---

## ✨ Features

- 🌡️ **BME280**: Temperatur, Luftfeuchte, Luftdruck
- 🌐 **Weboberfläche**
  - Startseite mit Live-Daten (Auto-Refresh)
  - Einstellungen
  - System- & Netzwerk-Infos
  - About-Seite
- 🔐 **Login-System**
  - Admin-Login
  - erzwungene Passwortänderung beim Erststart
  - Session-Cookie
- ⏱️ **NTP-Zeit**
  - automatische Zeitzone **Europe/Berlin**
  - Sommerzeit automatisch
- 📡 **UDP-Ausgabe**
  - CSV oder JSON
  - feste Keys / Reihenfolge
  - ideal für **Loxone**
- 💾 **LittleFS**
  - Konfigurationsdatei (`config.json`)
  - Backup / Restore über Web
- ♻️ **Factory Reset**
- 🔎 **Auffindbarkeit**
  - Hostname & mDNS: `http://multi-sensor.local/`
- 🧱 **Saubere Architektur**
  - `web_server` → Routing
  - `pages` → HTML & Seiten
  - `auth` → Login / Security
  - `api` → JSON-Endpunkte

---

## 🧰 Hardware

- ESP8266 (z.B. **Wemos D1 mini / lite**)
- **BME280** (I²C, 3.3 V)
- Pullups für I²C (meist auf Breakout vorhanden)

**I²C-Pins (Standard):**
- SDA → D2
- SCL → D1

---

## 🧪 Software / Toolchain

- **PlatformIO**
- **Framework:** Arduino (ESP8266 Core)
- **Libraries:**
  - WiFiManager
  - ArduinoJson
  - Adafruit BME280
  - NTPClient

---

## 🚀 Installation

### 1️⃣ Projekt klonen

git clone https://github.com/<dein-user>/multi-sensor.git
cd multi-sensor

---

## 🌐 Ersteinrichtung

  1. Gerät starten

  2. WLAN Config Portal öffnet sich automatisch

  3. WLAN auswählen & verbinden

  4. Weboberfläche aufrufen:

    http://multi-sensor.local/

    oder IP-Adresse aus Router

  5. Beim ersten Seitenaufruf:

    admin Passwort festlegen

  6. Weiterleitung zur Login-Seite

    Benutzer: admin

    Passwort: (zuvor festgelegtes Passwort)

---

## 🔧 Konfiguration

Alle Einstellungen erfolgen über die Weboberfläche:

  - Loxone IP & UDP-Port
  - Sendeintervall
  - Sensor-ID
  - NTP-Server
  - UDP-Format (CSV / JSON)
  - Backup / Restore
  - Factory Reset

---

## 🔐 Sicherheit

  - Passwort wird gehasht (SHA1) gespeichert
  - Zwang zur Passwortänderung bei Default
  - Session-Cookie (HttpOnly, Path=/)
  - Kein Klartext-Passwort im Flash

---

## 🛠️ Roadmap / Ideen

🔄 OTA-Update über WebUI

📊 Live-Grafen (JS)

🚨 Health-Checks (Heap, Sensor, WLAN)

📦 /api/info (JSON)

🧪 Entwickler-/Debug-Modus

---

👤 Autor

Da Schwede
© 2025

---
## License  EN/DE


- EN 
Multi-Sensor License – Private Use Only

Copyright (c) 2025 Da Schwede

Permission is hereby granted to use this software and its source code
for PRIVATE and NON-COMMERCIAL purposes only.

The following is NOT permitted without explicit written permission
from the copyright holder:

- Any commercial use
- Selling, sublicensing, or distributing this software or parts of it
- Using this software or parts of it in commercial products or services
- Using this software or parts of it for paid services, installations,
  or consulting
- Republishing modified or unmodified versions for commercial purposes

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties of
merchantability, fitness for a particular purpose and noninfringement.
In no event shall the authors be liable for any claim, damages or other
liability, whether in an action of contract, tort or otherwise,
arising from, out of or in connection with the software or the use or
other dealings in the software.

For commercial licensing, please contact:
Da Schwede

--

- DE

Multi-Sensor Lizenz – Nur private Nutzung

Copyright (c) 2025 Da Schwede

Hiermit wird die Erlaubnis erteilt, diese Software und ihren Quellcode
ausschließlich für PRIVATE und NICHT-KOMMERZIELLE Zwecke zu nutzen.

Folgendes ist ohne ausdrückliche schriftliche Genehmigung
des Urhebers NICHT gestattet:

- Jegliche kommerzielle Nutzung
- Verkauf, Unterlizenzierung oder Weiterverbreitung dieser Software
  oder von Teilen davon
- Nutzung dieser Software oder von Teilen davon in kommerziellen
  Produkten oder Dienstleistungen
- Nutzung dieser Software oder von Teilen davon für entgeltliche
  Dienstleistungen, Installationen oder Beratungen
- Veröffentlichung, Weitergabe oder Verwertung von veränderten oder
  unveränderten Versionen zu kommerziellen Zwecken

Diese Software wird „wie sie ist“ bereitgestellt, ohne jegliche
Gewährleistung, ausdrücklich oder stillschweigend, einschließlich,
aber nicht beschränkt auf die Gewährleistung der Marktgängigkeit,
Eignung für einen bestimmten Zweck und Nichtverletzung von Rechten.

In keinem Fall haften der Autor oder die Mitwirkenden für Ansprüche,
Schäden oder sonstige Verpflichtungen, gleichgültig ob aus Vertrag,
unerlaubter Handlung oder anderweitig, die aus der Nutzung dieser
Software oder im Zusammenhang mit dieser Software entstehen.

Für eine kommerzielle Lizenzierung wenden Sie sich bitte an:
Da Schwede



```bash