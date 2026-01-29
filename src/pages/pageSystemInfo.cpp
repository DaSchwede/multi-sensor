#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <WiFi.h>

#include "pages.h"
#include "auth.h"
#include "version.h"

#include <esp_system.h>
#include <esp_chip_info.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static String progressBar(int pct) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  String h;
  h += "<div class='progress' title='" + String(pct) + "%'>";
  h += "<div class='bar' style='width:" + String(pct) + "%'></div>";
  h += "</div>";
  return h;
}

static String fmtBytes(uint64_t b) {
  char buf[32];
  if (b < 1024) snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)b);
  else if (b < 1024ULL*1024ULL) snprintf(buf, sizeof(buf), "%.1f kB", (double)b/1024.0);
  else snprintf(buf, sizeof(buf), "%.2f MB", (double)b/(1024.0*1024.0));
  return String(buf);
}

static String resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "Power on reset";
    case ESP_RST_SW:        return "Software reset";
    case ESP_RST_PANIC:     return "Panic/Exception";
    case ESP_RST_INT_WDT:   return "Interrupt WDT";
    case ESP_RST_TASK_WDT:  return "Task WDT";
    case ESP_RST_WDT:       return "Other WDT";
    case ESP_RST_DEEPSLEEP: return "Deep Sleep";
    case ESP_RST_BROWNOUT:  return "Brownout";
    case ESP_RST_SDIO:      return "SDIO reset";
    default:                return "Unbekannt";
  }
}

static String cardFirmware() {
  AppConfig* cfg = pagesCfg();

  String h;
  h += "<div class='card'><h2>Firmwareinformationen</h2><table class='tbl'>";
  h += "<tr><th>Firmware</th><td>" + String(FW_NAME) + "</td></tr>";
  h += "<tr><th>Version</th><td>" + String(FW_VERSION) + "</td></tr>";
  h += "<tr><th>Build</th><td>" + String(FW_DATE) + "</td></tr>";
  h += "<tr><th>Hostname</th><td>" + String(WiFi.getHostname() ? WiFi.getHostname() : "—") + "</td></tr>";
  h += "<tr><th>ESP-IDF</th><td>" + String(esp_get_idf_version()) + "</td></tr>";
  h += "<tr><th>Reset Reason</th><td>" + resetReasonStr(esp_reset_reason()) + "</td></tr>";
  if (cfg) {
    h += "<tr><th>Sensor-ID</th><td>" + cfg->sensor_id + "</td></tr>";
  }
  h += "<tr><th>Betriebszeit</th><td>" + pagesUptimeString() + "</td></tr>";
  h += "</table></div>";
  return h;
}

static String cardHardware() {
  esp_chip_info_t info{};
  esp_chip_info(&info);

  uint32_t id = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF);
  String chip = String(id, HEX);
  chip.toUpperCase();
  while (chip.length() < 8) chip = "0" + chip;

  uint32_t flashBytes = ESP.getFlashChipSize();

  float cpuTemp = NAN;
  #if defined(ESP32)
  cpuTemp = temperatureRead();
  #endif

  String h;
  h += "<div class='card'><h2>Hardwareinformationen</h2><table class='tbl'>";
  h += "<tr><th>Chip-ID</th><td>" + chip + "</td></tr>";
  h += "<tr><th>Kerne</th><td>" + String(info.cores) + "</td></tr>";
  h += "<tr><th>CPU-Frequenz</th><td>" + String(ESP.getCpuFreqMHz()) + " MHz</td></tr>";
  h += "<tr><th>CPU-Temperatur</th><td>" + (isnan(cpuTemp) ? String("—") : String(cpuTemp, 1) + " °C") + "</td></tr>";
  h += "<tr><th>Flash Größe</th><td>" + fmtBytes(flashBytes) + "</td></tr>";
  h += "</table></div>";
  return h;
}

static String cardMemoryOverview() {
  // Heap
  uint32_t heapFree = ESP.getFreeHeap();
  uint32_t heapMin  = ESP.getMinFreeHeap();
  uint32_t heapMax  = heapFree + (heapMin > heapFree ? (heapMin - heapFree) : 0); // nur grobe Orientierung

  // LittleFS
  uint64_t fsTotal = 0, fsUsed = 0;
  if (LittleFS.begin(true)) {
    fsTotal = LittleFS.totalBytes();
    fsUsed  = LittleFS.usedBytes();
  }

  // Sketch
  uint32_t sketchSize = ESP.getSketchSize();
  uint32_t freeSketch = ESP.getFreeSketchSpace();
  uint32_t sketchTotal = sketchSize + freeSketch;

  auto pct = [](uint64_t used, uint64_t total)->int {
    if (!total) return 0;
    return (int)lround((double)used * 100.0 / (double)total);
  };

  int heapUsedPct   = 0;
  // Heap total kann man nicht exakt “gesamt” angeben, aber wir können “Max usage seit Start” über MinFree nähern:
  // MaxUsed ≈ (CurrentFree + (CurrentFree-MinFree)?). Wir zeigen einfach “frei” + “min frei” im Detail unten.
  // Für die Übersicht: Prozent = ( (freeHeap - minFreeHeap) / freeHeap?? ) ist unsinnig -> deshalb: nur Balken “frei”.
  // -> Wir machen Balken “frei” (je mehr, desto besser) mit einer Schätzung:
  heapUsedPct = 100; // fallback
  // Wenn du lieber “frei” visualisieren willst:
  int heapFreePct = 0;
  if (heapFree + (heapFree - heapMin) > 0) {
    uint32_t approxTotal = heapFree + (heapFree - heapMin);
    heapFreePct = (int)lround((double)heapFree * 100.0 / (double)approxTotal);
    heapUsedPct = 100 - heapFreePct;
  }

  int fsUsedPct     = pct(fsUsed, fsTotal);
  int sketchUsedPct = pct(sketchSize, sketchTotal);

  String h;
  h += "<div class='card'><h2>Speicherinformationen</h2>";
  h += "<table class='tbl'>";
  h += "<tr><th>Typ</th><th>Verwendung</th><th>Frei</th><th>Benutzt</th><th>Größe</th></tr>";

  h += "<tr>";
  h += "<td><b>Heap</b></td>";
  h += "<td>" + progressBar(heapUsedPct) + "<div class='small mt-8'>" + String(heapUsedPct) + "%</div></td>";
  h += "<td>" + fmtBytes(heapFree) + "</td>";
  h += "<td>" + fmtBytes((heapFree > heapMin) ? (heapFree - heapMin) : 0) + "</td>";
  h += "<td>—</td>";
  h += "</tr>";

  h += "<tr>";
  h += "<td><b>LittleFS</b></td>";
  h += "<td>" + progressBar(fsUsedPct) + "<div class='small mt-8'>" + String(fsUsedPct) + "%</div></td>";
  h += "<td>" + fmtBytes(fsTotal - fsUsed) + "</td>";
  h += "<td>" + fmtBytes(fsUsed) + "</td>";
  h += "<td>" + fmtBytes(fsTotal) + "</td>";
  h += "</tr>";

  h += "<tr>";
  h += "<td><b>Sketch</b></td>";
  h += "<td>" + progressBar(sketchUsedPct) + "<div class='small mt-8'>" + String(sketchUsedPct) + "%</div></td>";
  h += "<td>" + fmtBytes(freeSketch) + "</td>";
  h += "<td>" + fmtBytes(sketchSize) + "</td>";
  h += "<td>" + fmtBytes(sketchTotal) + "</td>";
  h += "</tr>";

  h += "</table></div>";
  return h;
}

static String cardHeapDetails() {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t largest  = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  uint32_t minFree  = ESP.getMinFreeHeap();

  // Fragmentierung grob: 1 - largest/free
  int frag = 0;
  if (freeHeap > 0 && largest <= freeHeap) {
    frag = (int)lround((1.0 - (double)largest / (double)freeHeap) * 100.0);
    if (frag < 0) frag = 0;
    if (frag > 100) frag = 100;
  }

  String h;
  h += "<div class='card'><h2>Detailinformationen zum Heap</h2><table class='tbl'>";
  h += "<tr><th>Insgesamt frei</th><td>" + fmtBytes(freeHeap) + "</td></tr>";
  h += "<tr><th>Größter zusammenhängender freier Block</th><td>" + fmtBytes(largest) + "</td></tr>";
  h += "<tr><th>Grad der Fragmentierung</th><td>" + String(frag) + " %</td></tr>";
  h += "<tr><th>Min. freier Heap seit Start</th><td>" + fmtBytes(minFree) + "</td></tr>";
  h += "</table></div>";
  return h;
}

static String cardTasks() {
  String h;
  h += "<div class='card'><h2>Detailinformationen zu Tasks</h2>";

#if defined(configUSE_TRACE_FACILITY) && (configUSE_TRACE_FACILITY == 1) && \
    defined(configUSE_STATS_FORMATTING_FUNCTIONS) && (configUSE_STATS_FORMATTING_FUNCTIONS == 1)

  char buf[1024];
  memset(buf, 0, sizeof(buf));
  vTaskList(buf);

  h += "<pre style='white-space:pre; overflow:auto; border:1px solid var(--border); border-radius:12px; padding:12px; background:#f9fafb;'>";
  h += String(buf);
  h += "</pre>";

#else
//  h += "<div class='hint'>Task-Liste ist nicht verfügbar. "
//       "Aktiviere configUSE_TRACE_FACILITY und configUSE_STATS_FORMATTING_FUNCTIONS.</div>";
#endif

  h += "</div>";
  return h;
}


void pageSystemInfo(WebServer &server) {
  AppConfig* cfg = pagesCfg();
  if (!cfg) { server.send(500, "text/plain", "cfg missing"); return; }
  if (!requireAuth(server, *cfg)) return;

  String html = pagesHeaderAuth("Info – Systeminfo", "/info/system");

  // Headerzeile wie in deinen Screens (Titel + Refresh)
  html += "<div style='display:flex; align-items:center; justify-content:space-between; gap:10px;'>";
  html += "<h1 style='margin:0;'>Systeminformationen</h1>";
  html += "<button class='icon-btn' type='button' title='Aktualisieren' onclick='location.reload()'>&#x21bb;</button>";
  html += "</div>";

  html += cardFirmware();
  html += cardHardware();
  html += cardMemoryOverview();
  html += cardHeapDetails();
  html += cardTasks();

  html += pagesFooter();
  server.send(200, "text/html; charset=utf-8", html);
}
