// src/core/GpsLog.cpp
#include "GpsLog.h"
#include "hal/hw.h"
#include "core/Database.h"
#include <deque>
#include <cstdarg>
#include <cstdio>

namespace gpslog {

static std::deque<Entry> s_entries;
static uint32_t s_last_poll    = 0;
static uint32_t s_last_pos_log = 0;
static bool     s_had_fix      = false;
static uint8_t  s_last_sats    = 0xFF;

static void add(const char* fmt, ...)
{
    Entry e{};
    e.ms = millis();
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e.text, sizeof(e.text), fmt, ap);
    va_end(ap);
    s_entries.push_front(e);
    while ((int)s_entries.size() > MAX_ENTRIES) s_entries.pop_back();
}

void begin()
{
    s_entries.clear();
    s_had_fix      = false;
    s_last_sats    = 0xFF;
    s_last_poll    = 0;
    s_last_pos_log = 0;
}

void poll()
{
    // Если GPS-модуля физически нет (T-Watch S3) — тихо выходим,
    // в настройках лог будет пустой и напишет "No GPS hardware".
    if (!(hw_get_device_online() & HW_GPS_ONLINE)) return;
    if (!Database::inst().config().gps_on) return;

    uint32_t now = millis();
    if (now - s_last_poll < 2000) return;   // раз в 2 с — этого хватает
    s_last_poll = now;

    gps_params_t p = {};
    bool ok = hw_get_gps_info(p);           // внутри уже троттлинг 1 Гц

    if (ok && !s_had_fix) {
        add("Fix acquired  %.5f, %.5f  sats=%u",
            p.lat, p.lng, (unsigned)p.satellite);
        s_had_fix = true;
    } else if (!ok && s_had_fix) {
        add("Fix lost");
        s_had_fix = false;
    }

    // Скачки количества спутников — но только от 2+, чтобы не спамить 0↔1
    if (p.satellite >= 2 && p.satellite != s_last_sats) {
        add("Sats: %u", (unsigned)p.satellite);
        s_last_sats = p.satellite;
    } else if (p.satellite < 2 && s_last_sats != 0xFF) {
        s_last_sats = 0;
    }

    // Периодический отчёт о позиции, раз в 30 с
    if (ok && now - s_last_pos_log > 30000) {
        s_last_pos_log = now;
        add("Pos %.5f, %.5f  v=%.1f km/h",
            p.lat, p.lng, p.speed);
    }
}

int count() { return (int)s_entries.size(); }

const Entry& at(int i) { return s_entries[i]; }

} // namespace gpslog