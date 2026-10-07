#include "CrashLog.h"
#include <Arduino.h>
#include <FFat.h>
#include <esp_system.h>
#include <esp_attr.h>
#include <exception>
#include <lvgl.h>

struct CrashCtx {
    uint32_t magic, boots, t_ms, free_heap, min_heap;
    uint8_t  lv_used_pct;
    char     where[40];
};
static RTC_NOINIT_ATTR CrashCtx g_ctx;
static const uint32_t MAGIC = 0xC0FFEE42;
static String s_report;
static bool   s_bad = false;

static const char* reason_name(esp_reset_reason_t r)
{
    switch (r) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_SW:        return "SW (esp_restart)";
    case ESP_RST_PANIC:     return "PANIC (exception/abort)";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP wake";
    case ESP_RST_BROWNOUT:  return "BROWNOUT (питание!)";
    default:                return "OTHER";
    }
}

void crash_mark(const char* where)
{
    strlcpy(g_ctx.where, where, sizeof(g_ctx.where));
    g_ctx.t_ms = millis();
}

void crash_init()
{
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 1500) delay(10);   // дать USB-CDC подняться

    esp_reset_reason_t r = esp_reset_reason();
    bool had_ctx = (g_ctx.magic == MAGIC) && r != ESP_RST_POWERON;
    s_bad = (r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT ||
             r == ESP_RST_WDT   || r == ESP_RST_BROWNOUT);

    s_report  = "\n===== BOOT REPORT =====\nreset reason: ";
    s_report += reason_name(r);
    s_report += "\n";
    if (had_ctx) {
        s_report += "boots: "      + String(g_ctx.boots) + "\n";
        s_report += "last mark: "  + String(g_ctx.where) + " @ " + String(g_ctx.t_ms) + " ms\n";
        s_report += "free heap: "  + String(g_ctx.free_heap) + ", min: " + String(g_ctx.min_heap) + "\n";
        s_report += "lvgl mem used: " + String(g_ctx.lv_used_pct) + "%\n";
    } else {
        g_ctx = CrashCtx{};
        g_ctx.magic = MAGIC;
    }
    g_ctx.boots++;
    s_report += "=======================\n";
    Serial.print(s_report);

    std::set_terminate([] {
        Serial.println("\n[CRASH] uncaught C++ exception -> abort");
        crash_mark("terminate");
        abort();
    });
}

void crash_persist()
{
    if (!s_bad) return;
    File f = FFat.open("/crash.log", FILE_APPEND);
    if (f) {
        if (f.size() > 8192) { f.close(); FFat.remove("/crash.log"); f = FFat.open("/crash.log", FILE_WRITE); }
        if (f) { f.print(s_report); f.close(); }
    }
}

void crash_tick()
{
    static uint32_t last = 0, late = 0;
    uint32_t now = millis();

    if (!late && now > 5000) {              // повторить отчёт, когда монитор уже переподключился
        late = 1;
        if (s_bad) Serial.print(s_report);
        if (FFat.exists("/crash.log")) {
            File f = FFat.open("/crash.log");
            Serial.println("--- /crash.log ---");
            while (f && f.available()) Serial.write(f.read());
            Serial.println("--- end ---");
            if (f) f.close();
        }
    }
    if (now - last < 5000) return;
    last = now;

    lv_mem_monitor_t m; lv_mem_monitor(&m);
    g_ctx.free_heap   = ESP.getFreeHeap();
    g_ctx.min_heap    = ESP.getMinFreeHeap();
    g_ctx.lv_used_pct = m.used_pct;
    Serial.printf("[HB] up=%lus heap=%u min=%u lv=%u%% mark=%s\n",
                  now / 1000, g_ctx.free_heap, g_ctx.min_heap, m.used_pct, g_ctx.where);
}