#pragma once
#include <Arduino.h>

String sha1Hex(const String &in);

// ===== Passwort-Hashing =====
// Neues Format:  "pbkdf2$<iterationen>$<salt hex>$<hash hex>"  (PBKDF2-HMAC-SHA256, 16 Byte Salt)
// Altes Format:  40 Hex-Zeichen SHA-1 ohne Salt (wird beim nächsten Login automatisch umgestellt)

String passwordHash(const String &password);
bool   passwordVerify(const String &password, const String &stored);
bool   passwordHashIsLegacy(const String &stored);
bool   passwordHashLooksValid(const String &stored);   // für Restore-Prüfung
