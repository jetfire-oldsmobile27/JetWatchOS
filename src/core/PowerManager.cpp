#include "PowerManager.h"
#include "AppManager.h"
#include "Database.h"
#include "core/DataCache.hpp"
#include "ui/ui_common.h"

static const uint8_t LOW_BATT_BRIGHTNESS = 20;  

PowerManager& PowerManager::inst() { static PowerManager m; return m; }

void PowerManager::begin()
{
    _timeout_ms = Database::inst().config().disp_timeout_s * 1000UL;   // 0 = никогда
    if (Database::inst().config().power_save) setPowerSave(true);
}

void PowerManager::resetActivity() { UI_TRIGGER_ACTIVITY(); }

void PowerManager::setBtActive(bool on) { _bt_active = on; }

void PowerManager::screenOff()
{
    if (_screen_off) return;
    _screen_off = true;
    hw_set_disp_backlight(0);

    // Спать самому ST7789 (SLPIN 0x10) — экономит ещё 5–10 мА
    hw_set_display_power(false);

    // Стоп рендера, но НЕ инпута
    lv_display_t* d = lv_display_get_default();
    if (d) {
        lv_timer_t* t = lv_display_get_refr_timer(d);
        if (t) lv_timer_pause(t);
    }
    setCpuFrequencyMhz(40);
}

void PowerManager::wake()
{
    if (!_screen_off) return;
    _screen_off = false;

    lv_display_t* d = lv_display_get_default();
    if (d) {
        lv_timer_t* t = lv_display_get_refr_timer(d);
        if (t) lv_timer_resume(t);
    }

    hw_set_display_power(true);
    for (lv_indev_t* i = lv_indev_get_next(nullptr); i; i = lv_indev_get_next(i))
        lv_indev_wait_release(i);

    hw_set_disp_backlight(_low_state ? LOW_BATT_BRIGHTNESS
                                     : Database::inst().config().brightness);
    UI_TRIGGER_ACTIVITY();
    setCpuFrequencyMhz(240);
}

// Дёргать на каждой итерации loop(): касание тоже будит
void PowerManager::poll()
{
    if (_screen_off && UI_INACTIVE_MS() < 200) wake();
}

void PowerManager::tick()  
{
    if (!datacache::monitor_valid()) return;
    const monitor_params_t& mp = datacache::monitor();
    _batt_pct = mp.battery_percent;

    // --- Детектор старта/остановки зарядки ---
    bool now_chg = mp.is_charging && mp.battery_present;
    if (now_chg && !_was_charging) {
        charging_overlay_show();
    }
    _was_charging = now_chg;
    _had_battery  = mp.battery_present;

    // --- Low-battery подсветка ---
    if (_batt_pct < 20 && !_low_state) {
        _low_state = true;
        if (!_screen_off) hw_set_disp_backlight(LOW_BATT_BRIGHTNESS);
    } else if (_batt_pct >= 25 && _low_state) {
        _low_state = false;
        if (!_screen_off) hw_set_disp_backlight(Database::inst().config().brightness);
    }

    // --- Таймаут экрана ---
    if (!_screen_off && _timeout_ms > 0 && UI_INACTIVE_MS() > _timeout_ms)
        screenOff();
}

void PowerManager::setPowerSave(bool on)
{
    if (_power_save == on) return;
    _power_save = on;

    if (on) {
        setCpuFrequencyMhz(80);
        if (!_screen_off) {
            uint8_t cfg_b = Database::inst().config().brightness;
            hw_set_disp_backlight(cfg_b > 60 ? 60 : cfg_b);
        }
        uint32_t base = Database::inst().config().disp_timeout_s * 1000UL;
        _timeout_ms = base ? (base / 2 < 5000 ? 5000 : base / 2) : 0;
        Serial.println("[PWR] power-save ON");
    } else {
        setCpuFrequencyMhz(240);
        if (!_screen_off) {
            hw_set_disp_backlight(Database::inst().config().brightness);
        }
        _timeout_ms = Database::inst().config().disp_timeout_s * 1000UL;
        Serial.println("[PWR] power-save OFF");
    }
}