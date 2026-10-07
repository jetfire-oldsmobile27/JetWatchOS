#pragma once

#include "hal/hw.h"
#include <utility>

namespace ui {

template<typename T, typename... Args>
T* attach_ctx(lv_obj_t* obj, lv_event_code_t code,
              lv_event_cb_t cb, Args&&... args)
{
    T* ctx = new T{ std::forward<Args>(args)... };
    if (cb) lv_obj_add_event_cb(obj, cb, code, ctx);
    lv_obj_add_event_cb(obj, [](lv_event_t* e){
        delete static_cast<T*>(lv_event_get_user_data(e));
    }, LV_EVENT_DELETE, ctx);
    return ctx;
}
} // namespace ui

#include <lvgl.h>
#include <LilyGoLib.h>

#if LVGL_VERSION_MAJOR >= 9
  #define UI_INACTIVE_MS()        lv_display_get_inactive_time(nullptr)
  #define UI_TRIGGER_ACTIVITY()   lv_display_trigger_activity(nullptr)
#else
  #define UI_INACTIVE_MS()        lv_disp_get_inactive_time(nullptr)
  #define UI_TRIGGER_ACTIVITY()   lv_disp_trigger_activity(nullptr)
#endif

// V8 / V9 shims
#if LVGL_VERSION_MAJOR == 9
  #define UI_SCR_ACT()           lv_screen_active()
  #define UI_OBJ_DEL(o)          lv_obj_delete(o)
  #define UI_IMG_CREATE(p)       lv_image_create(p)
  #define UI_IMG_SET_SRC(o,s)    lv_image_set_src(o,s)
  #define UI_LABEL_LONG_WRAP     LV_LABEL_LONG_WRAP
#else
  #define UI_SCR_ACT()           lv_scr_act()
  #define UI_OBJ_DEL(o)          lv_obj_del(o)
  #define UI_IMG_CREATE(p)       lv_img_create(p)
  #define UI_IMG_SET_SRC(o,s)    lv_img_set_src(o,s)
#endif

LV_FONT_DECLARE(font_alibaba_12);
LV_FONT_DECLARE(font_alibaba_24);
LV_FONT_DECLARE(font_alibaba_40);
LV_FONT_DECLARE(font_alibaba_60);
LV_FONT_DECLARE(font_alibaba_100);

LV_IMG_DECLARE(img_notify);
LV_IMG_DECLARE(img_weather);
LV_IMG_DECLARE(img_run);
LV_IMG_DECLARE(img_alarm);
LV_IMG_DECLARE(img_timer);
LV_IMG_DECLARE(img_flash);
LV_IMG_DECLARE(img_settings);
LV_IMG_DECLARE(img_wifi);
LV_IMG_DECLARE(img_bluetooth);
LV_IMG_DECLARE(img_music);
LV_IMG_DECLARE(img_gps);
LV_IMG_DECLARE(img_monitoring);
LV_IMG_DECLARE(img_test);

void      ui_init_theme();
lv_obj_t* ui_make_screen(lv_obj_t* parent, lv_color_t bg = lv_color_black());
lv_obj_t* ui_make_label(lv_obj_t* parent, const lv_font_t* f,
                        lv_color_t color, const char* txt);
lv_obj_t* ui_make_btn(lv_obj_t* parent, const char* txt,
                      lv_event_cb_t cb, void* ud = nullptr,
                      int w = 100, int h = 44);

// Верхний статус-бар (время + батарея)
lv_obj_t* ui_make_statusbar(lv_obj_t* parent);

void set_default_group(lv_group_t* group);

void ui_add_screen_corner_mask(uint8_t corner_radius = 24, uint8_t mask_width = 10);

void ui_enable_gesture_bubble(lv_obj_t* root);