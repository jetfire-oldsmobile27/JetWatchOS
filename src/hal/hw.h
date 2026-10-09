// src/hal/hw.h
#pragma once
#include "hal_interface.h"
#include <LilyGoLib.h>
#include <lvgl.h>

// Совместимость с новым RTC API (PCF8563: setDateTime(y,m,d,h,m,s))
static inline void hw_rtc_set_tm(const struct tm& t) {
    if (hw_get_device_online() & HW_RTC_ONLINE) {
        instance.rtc.setDateTime(
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
            t.tm_hour, t.tm_min, t.tm_sec);
    }
}

// LVGL 9.x: lv_event_get_target_obj вместо lv_obj_get_target
#if LVGL_VERSION_MAJOR >= 9
  #define HW_EV_OBJ(e)  lv_event_get_target_obj(e)
#else
  #define HW_EV_OBJ(e)  lv_event_get_target(e)
#endif

static inline void hw_set_display_power(bool on)
{
#ifdef ARDUINO
    if (on) instance.wakeupDisplay();
    else    instance.sleepDisplay();
#endif
}