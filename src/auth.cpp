#include "auth.h"
#include "crypto_utils.h"
#include "version.h"
#include <esp_system.h>

static const char* COOKIE_NAME = "LOXSESS";

// ===== Sitzungen (RAM, nach Neustart neu anmelden) =====
// Mehrere gleichzeitig, damit z.B. Handy und PC parallel angemeldet bleiben.
struct Session { String token; uint32_t lastUseMs = 0; };
static const int MAX_SESSIONS = 4;
static const uint32_t SESSION_IDLE_MS = 24UL * 3600UL * 1000UL;   // 24 h ohne Nutzung -> abgelaufen
static Session sessions[MAX_SESSIONS];

// ===== Bremse gegen Durchprobieren von Passwörtern =====
// Ab dem 3. Fehlversuch wachsende Sperre: 5 s, 10 s, 20 s ... max. 5 min
static uint8_t  failCount = 0;
static uint32_t lockUntilMs = 0;

static uint32_t lockRemainingMs() {
  if (!lockUntilMs) return 0;
  const int32_t d = (int32_t)(lockUntilMs - millis());
  if (d <= 0) { lockUntilMs = 0; return 0; }
  return (uint32_t)d;
}

static void registerFail() {
  if (failCount < 255) failCount++;
  if (failCount >= 3) {
    const uint8_t shift = (failCount - 3 > 6) ? 6 : (failCount - 3);
    uint32_t sec = 5UL << shift;
    if (sec > 300) sec = 300;
    lockUntilMs = millis() + sec * 1000UL;
    if (!lockUntilMs) lockUntilMs = 1;
    Serial.printf("Login: %u Fehlversuche, Sperre %lu s\n", failCount, (unsigned long)sec);
  }
}

static void registerSuccess() {
  failCount = 0;
  lockUntilMs = 0;
}

static String cookieToken(WebServer &server) {
  if (!server.hasHeader("Cookie")) return String();
  String cookie = server.header("Cookie");
  int idx = cookie.indexOf(String(COOKIE_NAME) + "=");
  if (idx < 0) return String();
  String value = cookie.substring(idx + strlen(COOKIE_NAME) + 1);
  int end = value.indexOf(';');
  if (end >= 0) value = value.substring(0, end);
  value.trim();
  return value;
}

static void addSession(const String &token) {
  // freien oder ältesten Platz nehmen
  int slot = 0;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (!sessions[i].token.length()) { slot = i; break; }
    if (sessions[i].lastUseMs < sessions[slot].lastUseMs) slot = i;
  }
  sessions[slot].token = token;
  sessions[slot].lastUseMs = millis();
}


static String randomToken() {
  // 128 Bit aus dem Hardware-Zufallsgenerator
  char buf[33];
  for (int i = 0; i < 4; i++) snprintf(buf + i * 8, 9, "%08x", (unsigned)esp_random());
  return String(buf);
}

static String uiHeader(const String& title, bool showLogout = false) {
  String html =
    "<!doctype html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<link rel='stylesheet' href='/style.css'>"
    "<link rel='stylesheet' href='/ui.css?v=" FW_VERSION "'>"
    "<link rel='icon' type='image/svg+xml' href='/favicon.svg'>"
    "<script defer src='/script.js'></script>"
    "<title>"+ title + "</title>"
    "</head><body>"
    "<div class='topbar'><img src='/logo_name_weiss.svg' alt='Multi Sensors' class='logo'></div>"
    "<div class='menubar'>"
      "<a class='active' href='/login'>Login</a>";

  if (showLogout) {
    html += "<span class='right'><a href='/logout'>Abmelden</a></span>";
  }

  html +=
    "</div>"
    "<div class='content'>"
    "<div class='card'>";

  return html;
}

static String uiFooter() {
  return "</div></div></body></html>";
}

void authAttach(WebServer &server, AppConfig &cfg) {

  server.on("/force_pw", HTTP_GET, [&]() { handleForcePassword(server, cfg); });
  server.on("/force_pw", HTTP_POST, [&]() { handleForcePassword(server, cfg); });

  server.on("/login", HTTP_GET, [&]() { handleLogin(server, cfg); });
  server.on("/login", HTTP_POST, [&]() { handleLogin(server, cfg); });
  server.on("/logout", HTTP_GET, [&]() { handleLogout(server); });
}

bool isAuthenticated(WebServer &server) {
  const String value = cookieToken(server);
  if (value.length() != 32) return false;
  const uint32_t now = millis();
  for (int i = 0; i < MAX_SESSIONS; i++) {
    Session &s = sessions[i];
    if (!s.token.length()) continue;
    if (now - s.lastUseMs > SESSION_IDLE_MS) { s.token = ""; continue; }   // abgelaufen
    if (s.token == value) { s.lastUseMs = now; return true; }
  }
  return false;
}

void authKeepOnlyCurrentSession(WebServer &server) {
  const String value = cookieToken(server);
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (sessions[i].token.length() && sessions[i].token != value) sessions[i].token = "";
  }
}

bool authCheckAdminPassword(AppConfig &cfg, const String &password, String &errOut) {
  const uint32_t wait = lockRemainingMs();
  if (wait) {
    errOut = "Zu viele Fehlversuche – bitte noch " + String((wait + 999) / 1000) + " s warten.";
    return false;
  }
  if (!password.length() || !passwordVerify(password, cfg.admin_pass_hash)) {
    registerFail();
    errOut = "Passwort falsch.";
    return false;
  }
  registerSuccess();

  // alten SHA-1-Hash still auf PBKDF2 umstellen
  if (passwordHashIsLegacy(cfg.admin_pass_hash) && !passwordIsDefault(cfg)) {
    const String h = passwordHash(password);
    if (h.length()) {
      cfg.admin_pass_hash = h;
      saveConfig(cfg);
      Serial.println("Admin-Passwort auf PBKDF2 umgestellt.");
    }
  }
  return true;
}

bool authSetAdminPassword(AppConfig &cfg, const String &password) {
  const String h = passwordHash(password);
  if (!h.length()) return false;
  cfg.admin_pass_hash = h;
  cfg.force_pw_change = false;
  return saveConfig(cfg);
}

bool requireAuth(WebServer &server, AppConfig &cfg) {

  // 1) Zwang: Passwort muss gesetzt sein
  if (cfg.force_pw_change || passwordIsDefault(cfg)) {

    // Damit /force_pw selbst nicht in eine Redirect-Schleife läuft:
    if (server.uri() != "/force_pw") {
      server.sendHeader("Location", "/force_pw", true);
      server.send(302, "text/plain", "");
      return false;
    }
    return true; // /force_pw darf angezeigt werden
  }

  // 2) Normaler Login-Check
  if (isAuthenticated(server)) return true;

  server.sendHeader("Location", "/login", true);
  server.send(302, "text/plain", "");
  return false;
}


bool passwordIsDefault(const AppConfig &cfg) {
  // Default-Passwort ist "admin". Ergebnis cachen: requireAuth() fragt das bei jedem Request,
  // und PBKDF2 kostet ~0,1 s.
  static String cachedHash;
  static bool cachedResult = true;
  if (cfg.admin_pass_hash.length() == 0) return true;
  if (cfg.admin_pass_hash != cachedHash) {
    cachedHash = cfg.admin_pass_hash;
    cachedResult = passwordVerify("admin", cfg.admin_pass_hash);
  }
  return cachedResult;
}

void handleForcePassword(WebServer &server, AppConfig &cfg) {

  if (!cfg.force_pw_change && !passwordIsDefault(cfg)) {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
    return;
  }

  String msg = "";

  if (server.method() == HTTP_POST) {
    String p1 = server.arg("pass1");
    String p2 = server.arg("pass2");

    if (p1.length() < 8 || p1.length() > 64) {
      msg = "Passwort muss 8 bis 64 Zeichen lang sein.";
    } else if (p1 != p2) {
      msg = "Passwörter stimmen nicht überein.";
    } else if (p1 == "admin") {
      msg = "Bitte ein anderes Passwort als „admin“ wählen.";
    } else {
      authSetAdminPassword(cfg, p1);

      server.sendHeader("Location", "/login", true);
      server.send(302, "text/plain", "");
      return;
    }
  }

  String html = uiHeader("Passwort festlegen");
  html += "<h2>Admin-Passwort festlegen</h2>";
  html += "<p class='small'>Aus Sicherheitsgründen musst du zuerst ein neues Passwort setzen.</p>";

  if (msg.length()) {
    html += "<p style='color:#b00020; font-weight:700;'>" + msg + "</p>";
  }

  html +=
    "<form method='POST'>"
      "<div class='form-row'><label>Neues Passwort</label>"
      "<input type='password' name='pass1' required></div>"

      "<div class='form-row'><label>Wiederholen</label>"
      "<input type='password' name='pass2' required></div>"

      "<div class='actions'>"
        "<button class='btn-primary' type='submit'>Speichern</button>"
      "</div>"
    "</form>";

  html += uiFooter();

  server.send(200, "text/html; charset=utf-8", html);
}


void handleLogin(WebServer &server, AppConfig &cfg) {
  String err = "";

  bool doLogin = false;

  if (server.method() == HTTP_POST) {
    String user = server.arg("user");
    String pass = server.arg("pass");

    // 1) Benutzer/Passwort prüfen (mit Fehlversuch-Bremse)
    if (!server.hasArg("license_ok")) {
      err = "Bitte Lizenz akzeptieren, um fortzufahren.";
      doLogin = false;
    } else {
      String pwErr;
      const uint32_t wait = lockRemainingMs();
      if (wait) {
        err = "Zu viele Fehlversuche – bitte noch " + String((wait + 999) / 1000) + " s warten.";
      } else if (user != cfg.admin_user) {
        registerFail();
        err = "Login fehlgeschlagen (User/Passwort falsch).";
      } else if (authCheckAdminPassword(cfg, pass, pwErr)) {
        doLogin = true;
      } else {
        err = (pwErr == "Passwort falsch.") ? String("Login fehlgeschlagen (User/Passwort falsch).") : pwErr;
      }
    }


    // 3) Erfolgreich → Session setzen
    if (doLogin) {
      const String token = randomToken();
      addSession(token);
      server.sendHeader(
        "Set-Cookie",
        String(COOKIE_NAME) + "=" + token + "; Path=/; Max-Age=86400; HttpOnly; SameSite=Lax"
      );
      server.sendHeader("Location", "/", true);
      server.send(302, "text/plain", "");
      return;
    }
  }

  // ===== Render Login-Seite =====
  String html = uiHeader("Login");
  html += "<h2>Login</h2>";

  if (err.length()) {
    html += "<p style='color:#b00020; font-weight:700;'>" + err + "</p>";
  }

html += "<form method='POST' class='login-form'>";

html +=
  "<div class='form-row'><label>Benutzer</label>"
  "<input name='user' value='admin' required></div>"

  "<div class='form-row'><label>Passwort</label>"
  "<input name='pass' type='password' required></div>";

// ✅ Lizenz-Checkbox
html +=
  "<div class='form-row login-license-row'>"
    "<div class='checkline login-checkline'>"
      "<input type='checkbox' id='license_ok' name='license_ok' required>"
      "<label for='license_ok'>Ich akzeptiere die <a href='/license'>Lizenzbedingungen</a>.</label>"
    "</div>"
    "<div class='small login-license-hint'>Ohne Zustimmung ist kein Login möglich.</div>"
  "</div>";

  html +=
  "<div class='actions'>"
    "<button class='btn-primary' type='submit'>Anmelden</button>"
  "</div>"
  "</form>";


  html += uiFooter();

  server.send(200, "text/html; charset=utf-8", html);
}

void handleLogout(WebServer &server) {
  const String value = cookieToken(server);
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (value.length() && sessions[i].token == value) sessions[i].token = "";
  }
  server.sendHeader("Set-Cookie", String(COOKIE_NAME) + "=; Path=/; Max-Age=0; HttpOnly; SameSite=Lax");
  server.sendHeader("Location", "/login", true);
  server.send(302, "text/plain", "");
}
