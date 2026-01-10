#include "auth.h"
#include "crypto_utils.h"
#include <esp_system.h>

static const char* COOKIE_NAME = "LOXSESS";
static String sessionToken; // sehr simpel (RAM). Nach Reboot neu login.


static String randomToken() {
  uint64_t mac = ESP.getEfuseMac();
  uint32_t low = (uint32_t)(mac & 0xFFFFFFFF);
  uint32_t r = (uint32_t)esp_random();
  return String(low, HEX) + "-" + String(millis(), HEX) + "-" + String(r, HEX);
}

static String uiHeader(const String& title, bool showLogout = false) {
  String html =
    "<!doctype html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<link rel='stylesheet' href='/style.css'>"
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
  if (!server.hasHeader("Cookie")) return false;
  String cookie = server.header("Cookie");
  int idx = cookie.indexOf(String(COOKIE_NAME) + "=");
  if (idx < 0) return false;
  String value = cookie.substring(idx + strlen(COOKIE_NAME) + 1);
  int end = value.indexOf(';');
  if (end >= 0) value = value.substring(0, end);
  value.trim();
  return (value.length() > 0 && value == sessionToken);
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


bool passwordIsDefault(AppConfig &cfg) {
  return cfg.admin_pass_hash == defaultAdminHash();
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

    if (p1.length() < 6) {
      msg = "Passwort muss mindestens 6 Zeichen lang sein.";
    } else if (p1 != p2) {
      msg = "Passwörter stimmen nicht überein.";
    } else {
      cfg.admin_pass_hash = sha1Hex(p1);
      cfg.force_pw_change = false;
      saveConfig(cfg);

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

    // 1) Benutzer/Passwort prüfen
    if (user == cfg.admin_user && sha1Hex(pass) == cfg.admin_pass_hash) {
      doLogin = true;
    } else {
      err = "Login fehlgeschlagen (User/Passwort falsch).";
    }

    // Lizenz MUSS akzeptiert werden
  if (!server.hasArg("license_ok")) {
      err = "Bitte Lizenz akzeptieren, um fortzufahren.";
      doLogin = false;
    }
    // 3) Erfolgreich → Session setzen
    if (doLogin) {
      sessionToken = randomToken();
      server.sendHeader(
        "Set-Cookie",
        String(COOKIE_NAME) + "=" + sessionToken + "; Path=/; Max-Age=86400; SameSite=Lax"
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
  sessionToken = "";
  server.sendHeader("Set-Cookie", String(COOKIE_NAME) + "=; Path=/; Max-Age=0");
  server.sendHeader("Location", "/login", true);
  server.send(302, "text/plain", "");
}
