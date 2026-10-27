# Multi-Sensor 1.5.0 – Änderungen

Baut auf 1.4.0 auf. Kompiliert ohne Fehler gegen Arduino-ESP32 2.0.17 (ESP32-C3).

**Update:** Nur die Firmware hochladen (Upload bzw. OTA). Kein „Upload Filesystem Image“ nötig –
das neue Design kommt über `/ui.css` aus der Firmware. Nach dem Update ggf. einmal Strg+F5 im Browser.

## Neu
- **Raumluft-Karte** auf der Startseite: großer CO₂-Wert, Klartext „Luft ist gut“ (< 1000 ppm),
  „Bald lüften“ (< 1400 ppm), „Jetzt lüften“ (darüber), Ampel-Skala 400–2000 ppm mit Markierung
- Temperatur, Luftfeuchte, Luftdruck mit Einordnung (kühl/angenehm/warm, trocken/angenehm/feucht, tief/normal/hoch)
- Abzeichen zeigt, welcher Sensor fehlt (BME280 / SCD40)
- Zahlen mit Dezimalkomma, Uhrzeit der letzten Aktualisierung

## Design (alle Seiten)
- Schmalerer grüner Kopf, Menüleiste anthrazit, aktiver Punkt grün unterstrichen
- Flachere Karten, Tabellen ohne Rahmen mit rechtsbündigen Werten
- Überschriften in Bahnschrift (Windows) bzw. DIN-ähnlicher Systemschrift
- Am Handy: Raumklima untereinander, Menü mit grüner Markierung

## Hinweis Darstellung
- Die Raumluft-Karte („live“) lässt sich unter Einstellungen → Darstellung weiterhin ein- und ausblenden,
  steht aber immer oben über die ganze Breite. Die übrigen Karten folgen der eingestellten Reihenfolge.

## Test-Checkliste
- [ ] Startseite: CO₂-Wert, Skala und Klartext passen; anhauchen → Wert steigt, Punkt wandert
- [ ] Rescan-Knopf funktioniert, bleibt auf der Startseite
- [ ] SCD40 abziehen → nach ~30 s „Keine Messwerte vom CO₂-Sensor“, Abzeichen „SCD40 fehlt“
- [ ] Info, Einstellungen, Tools, Logger: Layout in Ordnung
- [ ] Am Handy: Menü, Startseite
