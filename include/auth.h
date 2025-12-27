#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include "settings.h"


void authAttach(ESP8266WebServer &server, AppConfig &cfg);
bool isAuthenticated(ESP8266WebServer &server);
bool requireAuth(ESP8266WebServer &server,  AppConfig &cfg); // sendet redirect wenn nicht eingeloggt
void handleForcePassword(ESP8266WebServer &server, AppConfig &cfg);
bool passwordIsDefault(AppConfig &cfg);
void handleLogin(ESP8266WebServer &server, AppConfig &cfg);
void handleLogout(ESP8266WebServer &server);


// Hilfsfunktion
String sha1Hex(const String &in);
