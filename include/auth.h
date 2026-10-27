#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "settings.h"


void authAttach(WebServer &server, AppConfig &cfg);
bool isAuthenticated(WebServer &server);
bool requireAuth(WebServer &server,  AppConfig &cfg); // sendet redirect wenn nicht eingeloggt
void handleForcePassword(WebServer &server, AppConfig &cfg);
bool passwordIsDefault(const AppConfig &cfg);
void handleLogin(WebServer &server, AppConfig &cfg);
void handleLogout(WebServer &server);

// Admin-Passwort prüfen – mit Bremse gegen Durchprobieren (gemeinsam für Login, OTA, Passwort ändern).
// Stellt einen alten SHA-1-Hash nach erfolgreicher Prüfung automatisch auf PBKDF2 um.
// errOut enthält bei false eine anzeigbare Meldung.
bool authCheckAdminPassword(AppConfig &cfg, const String &password, String &errOut);

// Neues Admin-Passwort setzen (PBKDF2) und speichern
bool authSetAdminPassword(AppConfig &cfg, const String &password);

// Alle anderen Sitzungen abmelden (z.B. nach Passwortänderung), die aktuelle bleibt
void authKeepOnlyCurrentSession(WebServer &server);
