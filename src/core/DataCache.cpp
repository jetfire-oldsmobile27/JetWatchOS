#include "DataCache.hpp"
#include "hal/hw.h"
#include "core/Database.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace datacache {

// Критическая секция только вокруг memcpy готового результата —
// несколько сотен наносекунд. I²C идёт ВНЕ секции.
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static struct tm        s_time     {};
static bool             s_time_ok  = false;

static monitor_params_t s_mon      {};
static bool             s_mon_ok   = false;

static uint32_t         s_steps    = 0;
static bool             s_steps_ok = false;

static float            s_temp     = 0.f;
static bool             s_temp_ok  = false;

// ------------------------------------------------------------------
//  Writer (задача на core 0)
// ------------------------------------------------------------------

static void refresh_time()
{
    uint32_t t0 = millis();
    struct tm t{};
    hw_get_date_time(t);
    uint32_t dt = millis() - t0;
    if (dt > 30) Serial.printf("[CACHE] rtc read took %u ms\n", dt);

    if (t.tm_year + 1900 >= 2020) {
        portENTER_CRITICAL(&s_mux);
        s_time    = t;
        s_time_ok = true;
        portEXIT_CRITICAL(&s_mux);
    }
}

static void refresh_monitor()
{
    uint32_t t0 = millis();
    monitor_params_t m{};
    hw_get_monitor_params(m);
    uint32_t dt = millis() - t0;
    if (dt > 30) Serial.printf("[CACHE] pmu read took %u ms\n", dt);

    portENTER_CRITICAL(&s_mux);
    s_mon    = m;
    s_mon_ok = true;
    portEXIT_CRITICAL(&s_mux);
}

static void refresh_steps()
{
    // Обрати внимание: stepsToday() внутри вызывает _rolloverDay(),
    // который читает RTC. Это ДВА I²C-захода, а не один.
    uint32_t t0 = millis();
    uint32_t s  = Database::inst().stepsToday();
    uint32_t dt = millis() - t0;
    if (dt > 30) Serial.printf("[CACHE] steps read took %u ms\n", dt);

    portENTER_CRITICAL(&s_mux);
    s_steps    = s;
    s_steps_ok = true;
    portEXIT_CRITICAL(&s_mux);
}

static void refresh_temp()
{
    if (!(hw_get_device_online() & HW_BMA423_ONLINE)) {
        portENTER_CRITICAL(&s_mux);
        s_temp_ok = false;
        portEXIT_CRITICAL(&s_mux);
        return;
    }
    uint32_t t0 = millis();
    float c = instance.sensor.getTemperature(SensorBMA423::TEMP_DEG);
    uint32_t dt = millis() - t0;
    if (dt > 30) Serial.printf("[CACHE] bma temp took %u ms\n", dt);

    portENTER_CRITICAL(&s_mux);
    s_temp    = c;
    s_temp_ok = true;
    portEXIT_CRITICAL(&s_mux);
}

static void cache_task(void *)
{
    uint32_t last_t = 0, last_m = 0, last_s = 0, last_tp = 0;

    while (true) {
        uint32_t now = millis();

        if (now - last_t  >= 1000) { last_t  = now; refresh_time();    }
        if (now - last_m  >= 5000) { last_m  = now; refresh_monitor(); }
        if (now - last_s  >= 2000) { last_s  = now; refresh_steps();   }
        if (now - last_tp >= 60000){ last_tp = now; refresh_temp();    }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void begin()
{
    static bool started = false;
    if (started) return;
    started = true;

    // core 0 — Arduino loopTask крутится на core 1.
    // Стек 4К хватает: только struct-копии, никаких String.
    xTaskCreatePinnedToCore(cache_task, "datacache", 4 * 1024,
                            nullptr, /*prio*/ 2, nullptr, /*core*/ 0);

    Serial.println("[CACHE] task started on core 0");
}

void poll() {}   // legacy, ничего не делает

// ------------------------------------------------------------------
//  Readers — никогда не блокируются
// ------------------------------------------------------------------

bool time_valid()
{
    portENTER_CRITICAL(&s_mux);
    bool ok = s_time_ok;
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

struct tm time()
{
    struct tm t;
    portENTER_CRITICAL(&s_mux);
    t = s_time;
    portEXIT_CRITICAL(&s_mux);
    return t;
}

bool monitor_valid()
{
    portENTER_CRITICAL(&s_mux);
    bool ok = s_mon_ok;
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

monitor_params_t monitor()
{
    monitor_params_t m;
    portENTER_CRITICAL(&s_mux);
    m = s_mon;
    portEXIT_CRITICAL(&s_mux);
    return m;
}

bool steps_valid()
{
    portENTER_CRITICAL(&s_mux);
    bool ok = s_steps_ok;
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

uint32_t steps()
{
    portENTER_CRITICAL(&s_mux);
    uint32_t s = s_steps;
    portEXIT_CRITICAL(&s_mux);
    return s;
}

bool temperature_valid()
{
    portENTER_CRITICAL(&s_mux);
    bool ok = s_temp_ok;
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

float temperature()
{
    portENTER_CRITICAL(&s_mux);
    float t = s_temp;
    portEXIT_CRITICAL(&s_mux);
    return t;
}

} // namespace datacache