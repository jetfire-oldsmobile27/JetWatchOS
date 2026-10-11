#pragma once

#include "hal/hw.h"
#include "ui/ui_tokens.h"
#include <utility>

// ---------------------------------------------------------------------
//  LVGL 9 / 8 shims
// ---------------------------------------------------------------------
#if LVGL_VERSION_MAJOR >= 9
  #define UI_INACTIVE_MS()        lv_display_get_inactive_time(nullptr)
  #define UI_TRIGGER_ACTIVITY()   lv_display_trigger_activity(nullptr)
  #define UI_SCR_ACT()            lv_screen_active()
  #define UI_OBJ_DEL(o)           lv_obj_delete(o)
  #define UI_OBJ_DEL_ASYNC(o)     lv_obj_delete_async(o)      
  #define UI_IMG_CREATE(p)        lv_image_create(p)
  #define UI_IMG_SET_SRC(o,s)     lv_image_set_src(o,s)
#else
  #define UI_INACTIVE_MS()        lv_disp_get_inactive_time(nullptr)
  #define UI_TRIGGER_ACTIVITY()   lv_disp_trigger_activity(nullptr)
  #define UI_SCR_ACT()            lv_scr_act()
  #define UI_OBJ_DEL(o)           lv_obj_del(o)
  #define UI_OBJ_DEL_ASYNC(o)     lv_obj_del_async(o)          
  #define UI_IMG_CREATE(p)        lv_img_create(p)
  #define UI_IMG_SET_SRC(o,s)     lv_img_set_src(o,s)
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

// ---------------------------------------------------------------------
//  ctx-хелпер: кладёт произвольную структуру в user_data, сама чистит
// ---------------------------------------------------------------------
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

// ---------------------------------------------------------------------
//  Базовые фабрики
// ---------------------------------------------------------------------
void      ui_init_theme();

lv_obj_t* ui_make_screen (lv_obj_t* parent,
                          lv_color_t bg = ui::bg());

lv_obj_t* ui_make_label  (lv_obj_t* parent, const lv_font_t* f,
                          lv_color_t color, const char* txt);

// Старая кнопка (совместимость) — но теперь в стиле HTML
lv_obj_t* ui_make_btn    (lv_obj_t* parent, const char* txt,
                          lv_event_cb_t cb, void* ud = nullptr,
                          int w = 140, int h = 44);

// ---------------------------------------------------------------------
//  Компоненты 1:1 из HTML-прототипа
// ---------------------------------------------------------------------

// 28px бар: время | BT | батарея
lv_obj_t* ui_make_statusbar(lv_obj_t* parent);

// Список: клипается под статус-бар, скроллится, без скроллбара
lv_obj_t* ui_make_list(lv_obj_t* parent, lv_coord_t top = ui::STATUSBAR_H);

// Строка 44px: [name] [value] [>] — фон surface, radius 12, press = surface_2
lv_obj_t* ui_make_row(lv_obj_t* parent,
                      const char* name, const char* value = nullptr,
                      bool chevron = true,
                      lv_event_cb_t cb = nullptr, void* ud = nullptr);

// Строка-info 30px: name слева, value справа (мелкая, не кликается)
lv_obj_t* ui_make_row_info(lv_obj_t* parent, const char* name,
                           lv_obj_t** out_value);

// Заголовок секции — accent, uppercase, f12
lv_obj_t* ui_make_separator(lv_obj_t* parent, const char* title);

// Switch 40×22, зелёный при on
lv_obj_t* ui_make_switch(lv_obj_t* parent, bool on,
                         lv_event_cb_t cb = nullptr, void* ud = nullptr);

// Круглая кнопка (primary / red / green)
lv_obj_t* ui_make_btn_pri(lv_obj_t* parent, const char* txt,
                          lv_event_cb_t cb, void* ud, int w = 140, int h = 44);
lv_obj_t* ui_make_btn_red(lv_obj_t* parent, const char* txt,
                          lv_event_cb_t cb, void* ud, int w = 140, int h = 44);
lv_obj_t* ui_make_btn_green(lv_obj_t* parent, const char* txt,
                            lv_event_cb_t cb, void* ud, int w = 140, int h = 44);

// Круг с иконкой — для меню (диаметр 96)
lv_obj_t* ui_make_disc(lv_obj_t* parent, const void* icon_src,
                       const char* caption);

// Dots-индикатор карусели (6 px, активный = 14×6 pill)
lv_obj_t* ui_make_dots(lv_obj_t* parent, int count,
                       lv_coord_t bottom = 14);
void      ui_dots_set(lv_obj_t* dots, int idx);

// Баннер-уведомление (выезжает сверху, через 4 сек исчезает)
void      ui_banner(const char* app, const char* title, const char* body);

void charging_overlay_show();

// ---------------------------------------------------------------------
//  Служебные
// ---------------------------------------------------------------------
void set_default_group(lv_group_t* group);
void ui_add_screen_corner_mask(uint8_t corner_radius = 24,
                               uint8_t mask_width = 10);
void ui_enable_gesture_bubble(lv_obj_t* root);

void ui_msg_pop_up(const char* title, const char* msg);