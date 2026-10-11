#include "ui_common.h"
#include "hal/hw.h"

static lv_obj_t*   s_ov      = nullptr;
static lv_obj_t*   s_pct_lbl = nullptr;
static lv_timer_t* s_upd     = nullptr;

static void upd_cb(lv_timer_t*)
{
    if (!s_ov || !s_pct_lbl) return;
    monitor_params_t p; hw_get_monitor_params(p);
    lv_label_set_text_fmt(s_pct_lbl, "%d%%   %+d mA",
                          p.battery_percent, (int)p.charge_current_ma);
}

static void cleanup()
{
    if (s_upd) { lv_timer_del(s_upd); s_upd = nullptr; }
    if (s_ov)  { UI_OBJ_DEL(s_ov); s_ov = nullptr; s_pct_lbl = nullptr; }
}

void charging_overlay_show()
{
    if (s_ov) return;

    s_ov = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_ov);
    lv_obj_set_size(s_ov, LV_PCT(100), LV_PCT(100));
    lv_obj_center(s_ov);
    lv_obj_set_style_bg_color(s_ov, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ov, LV_OPA_80, 0);
    lv_obj_clear_flag(s_ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_ov, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(s_ov);

    lv_obj_t* circ = lv_obj_create(s_ov);
    lv_obj_remove_style_all(circ);
    lv_obj_set_size(circ, 120, 120);
    lv_obj_center(circ);
    lv_obj_set_style_radius(circ, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(circ, 3, 0);
    lv_obj_set_style_border_color(circ, lv_color_hex(0x66CCFF), 0);
    lv_obj_set_style_border_opa(circ, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(circ, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(circ, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* bolt = lv_label_create(circ);
    lv_label_set_text(bolt, LV_SYMBOL_CHARGE);
    lv_obj_set_style_text_font(bolt, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(bolt, lv_color_hex(0x66CCFF), 0);
    lv_obj_center(bolt);

    s_pct_lbl = lv_label_create(s_ov);
    lv_obj_set_style_text_font(s_pct_lbl, &font_alibaba_24, 0);
    lv_obj_set_style_text_color(s_pct_lbl, lv_color_white(), 0);
    lv_obj_align(s_pct_lbl, LV_ALIGN_CENTER, 0, 85);

    monitor_params_t p; hw_get_monitor_params(p);
    lv_label_set_text_fmt(s_pct_lbl, "%d%%   %+d mA",
                          p.battery_percent, (int)p.charge_current_ma);

    s_upd = lv_timer_create(upd_cb, 500, nullptr);

    lv_timer_t* t = lv_timer_create([](lv_timer_t* tt){
        cleanup();
        lv_timer_del(tt);
    }, 2000, nullptr);
    lv_timer_set_repeat_count(t, 1);
}