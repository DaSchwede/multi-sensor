#pragma once
#include <ESP8266WebServer.h>
#include "pages.h"
#include "auth.h"
#include "settings.h"

static inline int toIntSafe(const String& s, int defVal){
  if (!s.length()) return defVal;
  return s.toInt();
}

static inline AppConfig* settingsRequireCfgAndAuth(ESP8266WebServer &server){
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return nullptr; }
  if (!requireAuth(server, *cfg)) return nullptr;
  return cfg;
}

static inline void settingsSendOkBadge(String &html, const String &msg){
  if (msg.length()) {
    html += "<div class='card'><p class='badge badge-ok' style='display:inline-block;'>" + msg + "</p></div>";
  }
}
