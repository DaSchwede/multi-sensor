#pragma once
#include <Arduino.h>

// Für Text in HTML (auch in Attributen mit ' oder ")
static inline String htmlEscape(const String &s) {
  String o;
  o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    const char c = s[i];
    switch (c) {
      case '&':  o += F("&amp;");  break;
      case '<':  o += F("&lt;");   break;
      case '>':  o += F("&gt;");   break;
      case '"':  o += F("&quot;"); break;
      case '\'': o += F("&#39;");  break;
      default:   o += c;           break;
    }
  }
  return o;
}

// Für String-Werte in JSON (ohne umschließende Anführungszeichen)
static inline String jsonEscape(const String &s) {
  String o;
  o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    const char c = s[i];
    switch (c) {
      case '"':  o += F("\\\""); break;
      case '\\': o += F("\\\\"); break;
      case '\n': o += F("\\n");  break;
      case '\r': o += F("\\r");  break;
      case '\t': o += F("\\t");  break;
      default:
        if ((uint8_t)c < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)(uint8_t)c);
          o += buf;
        } else {
          o += c;
        }
    }
  }
  return o;
}
