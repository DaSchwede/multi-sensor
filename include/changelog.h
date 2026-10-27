#pragma once
#include <Arduino.h>

// Versionshistorie für die Über-Seite (in der Firmware, damit kein LittleFS-Upload nötig ist).
// Format: Versionszeile, darunter Punkte mit "- ", Leerzeile zwischen Versionen.
static const char CHANGELOG_TXT[] PROGMEM = R"CL(
1.5.0
- Neues Design: ruhigere Farben, schmalerer Kopf, aktiver Menüpunkt grün unterstrichen, flache Karten, Werte rechtsbündig
- Startseite: Raumluft-Karte mit großem CO₂-Wert, Lüftungs-Ampel (gut / bald lüften / jetzt lüften) und Skala
- Temperatur, Luftfeuchte und Luftdruck mit Einordnung in Worten (z.B. angenehm, trocken)
- Zahlen mit Dezimalkomma, Anzeige welcher Sensor fehlt
- Bessere Darstellung am Handy

1.4.0
- Neue Seite Sensoren: Temperatur-Offset, CO₂-Kalibrierung an Frischluft, Luftdruck-Kompensation, automatische Selbstkalibrierung
- Ausgefallene Sensoren werden erkannt (keine eingefrorenen Werte mehr) und automatisch neu gesucht
- Verlauf neu: ein Diagramm pro Messwert, Einzelwerte per Maus/Finger, Download als Excel oder CSV
- Logger schreibt zuverlässig zu festen Uhrzeiten, Zeitbereiche 1 h bis Monat
- Info → Zeit / NTP mit manueller Synchronisation, Zeitzone über Systemzeit (Sommerzeit automatisch)
- MQTT: eigene Geräte-ID, Topic Base und LWT-Topic werden verwendet, Werte-Auswahl, keine "nan"-Werte, Statusanzeige, Home Assistant Discovery zuverlässig
- UDP: Hostnamen als Ziel, Format Text oder JSON wählbar mit Vorschau
- Tools-Seite neu mit Passwort ändern
- Sicherheit: sichere Passwort-Speicherung (PBKDF2), Sperre nach Fehlversuchen, WLAN-Einstellungen nur noch mit Freigabe, Setup-WLAN mit zufälligem Passwort, mehrere Anmeldungen gleichzeitig
- Captive Portal: Einrichtungsseite öffnet sich im Setup-WLAN automatisch
- Handy-Menü, Menüs per Klick, Seitenüberschriften
- Eindeutige Geräte-IDs aus der MAC-Adresse
- Einstellungen werden ausfallsicher gespeichert, Wiederherstellen prüft die Datei vorher
- Viele Fehlerbehebungen

1.3.2
- MQTT optimiert, verbessert
- NTP erweitert
- div. Design anpassungen

1.3.1
- Logger mit Tagesdateien
- Exportfunktion
- Historie Seite
- API für Live- und Historische Daten
- Fehlerbehebungen
- Codeoptimierungen
- kleine UI Verbesserungen

1.3.0
- SD Logging und Auswertung
- Web UI Verbesserungen
- Bugfixes und Stabilitätsverbesserungen
- Menüs überarbeitet

1.2.2
- MQTT Home Assistant Discovery
- TLS Support
- LWT

1.2.1
- Bugfixes und kleine Verbesserungen
- I2C Rescan Button
- UDP ausschaltbar
- Udp Werte wählbar

1.2.0
- Code Refactoring und Stabilitätsverbesserungen

1.1.0
- eigenen Wifi Manager hinzugefügt der stabiler läuft

1.0.0
- Umstieg auf ESP32-C3 da Leistungsstärker

0.5.0
- MQTT hinzugefügt
- Code aufgeräumt

0.4.1
- Einstellungsmenü auf Pull-Down umgebaut

0.4.0
- CO2 Sensor mit wert eingefügt

0.3.0
- Loxone aus udp gelöscht
- OTA Updatefunktion eingefügt
)CL";
