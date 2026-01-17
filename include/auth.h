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
