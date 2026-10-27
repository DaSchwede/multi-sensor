#pragma once
#include <Arduino.h>

// Eindeutige Geräte-IDs aus der Fabrik-MAC (eFuse).
// ESP.getEfuseMac() liefert die MAC mit Byte 0 im niederwertigsten Byte.
// Die ersten 3 Bytes sind der Hersteller (OUI) und damit bei allen Boards gleich,
// eindeutig sind nur die letzten 3 Bytes.

String deviceShortId();  // 6 Hex-Zeichen, z.B. "A1B2C3" (letzte 3 MAC-Bytes)
String deviceMacHex();   // 12 Hex-Zeichen, komplette MAC "AABBCCDDEEFF"
