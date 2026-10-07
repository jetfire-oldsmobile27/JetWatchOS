#pragma once

#include "hal/hw.h"
#include <LilyGoLib.h>
#include <lvgl.h>

class PowerManager {
public:
    static PowerManager& inst();

    void begin();
    void tick();                     // раз в 1000мс

    void setBtActive(bool on);
    bool isBtActive() const { return _bt_active; }

    void setScreenTimeout(uint32_t ms) { _timeout_ms = ms; }
    uint32_t screenTimeout() const { return _timeout_ms; }

    void resetActivity();
    uint8_t batteryPct() const { return _batt_pct; }
    bool lowBattery() const { return _batt_pct < 20; }

    void poll();                     // каждый проход loop(): тап тоже будит
    void screenOff();
    void wake();
    bool isScreenOff() const { return _screen_off; }

    // Совместимость с двойным кликом в AppManager
    void sleepNow()      { screenOff(); }
    void lightSleepNow() { screenOff(); }

private:
    uint32_t _timeout_ms    = 15000;
    uint8_t  _batt_pct      = 100;
    bool     _bt_active     = false;
    bool     _low_state     = false;
    bool     _screen_off    = false;  
};