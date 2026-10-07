#include "PowerManager.h"
#include "AppManager.h"
#include "Database.h"
#include "ui/ui_common.h"

static const uint8_t LOW_BATT_BRIGHTNESS = 20;  

PowerManager& PowerManager::inst() { static PowerManager m; return m; }

void PowerManager::begin()
{
    _timeout_ms = Database::inst().config().disp_timeout_s * 1000UL;   // 0 = никогда
}

void PowerManager::resetActivity() { UI_TRIGGER_ACTIVITY(); }

void PowerManager::setBtActive(bool on) { _bt_active = on; }

void PowerManager::screenOff()
{
    if (_screen_off) return;
    _screen_off = true;
    hw_set_disp_backlight(0);
}

void PowerManager::wake()
{
    if (!_screen_off) return;
    _screen_off = false;
    // съесть касание, разбудившее экран, чтобы оно не нажало кнопку под пальцем
    for (lv_indev_t* i = lv_indev_get_next(nullptr); i; i = lv_indev_get_next(i))
        lv_indev_wait_release(i);
    hw_set_disp_backlight(_low_state ? LOW_BATT_BRIGHTNESS
                                     : Database::inst().config().brightness);
    UI_TRIGGER_ACTIVITY();
}

// Дёргать на каждой итерации loop(): касание тоже будит
void PowerManager::poll()
{
    if (_screen_off && UI_INACTIVE_MS() < 200) wake();
}

void PowerManager::tick()   // раз в секунду
{
    monitor_params_t mp; hw_get_monitor_params(mp);
    _batt_pct = mp.battery_percent;

    if (_batt_pct < 20 && !_low_state) {
        _low_state = true;
        if (!_screen_off) hw_set_disp_backlight(LOW_BATT_BRIGHTNESS);
    } else if (_batt_pct >= 25 && _low_state) {
        _low_state = false;
        if (!_screen_off) hw_set_disp_backlight(Database::inst().config().brightness);
    }

    if (!_screen_off && _timeout_ms > 0 && UI_INACTIVE_MS() > _timeout_ms)
        screenOff();
}