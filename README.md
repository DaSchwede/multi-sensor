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
```bash
git clone https://github.com/<dein-user>/multi-sensor.git
cd multi-sensor
