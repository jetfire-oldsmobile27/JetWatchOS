#include "ui_common.h"
#include "core/PowerManager.h"
#include <ctime>

struct SbCtx {
    lv_obj_t* time;
    lv_obj_t* bt;
    lv_obj_t* batt;
    lv_timer_t* timer;
};

static void sb_refresh(SbCtx* c)
{
    struct tm t; hw_get_date_time(t);
    lv_label_set_text_fmt(c->time, "%02d:%02d", t.tm_hour, t.tm_min);

    monitor_params_t p; hw_get_monitor_params(p);
    lv_label_set_text_fmt(c->batt, "%d%%", p.battery_percent);

    bool bt = PowerManager::inst().isBtActive();
    lv_obj_set_style_text_color(c->bt,
        bt ? lv_color_hex(0x3A9EFF) : lv_color_hex(0x444444), 0);
}

static void sb_timer(lv_timer_t* t) { sb_refresh((SbCtx*)lv_timer_get_user_data(t)); }

static void sb_on_delete(lv_event_t* e)
{
    auto* c = (SbCtx*)lv_event_get_user_data(e);
    if (c->timer) lv_timer_del(c->timer);
    delete c;
}

lv_obj_t* ui_make_statusbar(lv_obj_t* parent)
{
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LV_PCT(100), 24);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_hor(bar, 8, 0);
    lv_obj_set_style_pad_ver(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);

    auto* c = new SbCtx{};
    c->time = ui_make_label(bar, &font_alibaba_12, lv_color_white(), "--:--");
    lv_obj_align(c->time, LV_ALIGN_LEFT_MID, 0, 0);

    // Montserrat содержит LV_SYMBOL_BLUETOOTH; alibaba_12 — только ASCII
    c->bt = ui_make_label(bar, &lv_font_montserrat_12,
                          lv_color_hex(0x444444), LV_SYMBOL_BLUETOOTH);
    lv_obj_align(c->bt, LV_ALIGN_CENTER, 0, 0);

    c->batt = ui_make_label(bar, &font_alibaba_12, lv_color_white(), "--%");
    lv_obj_align(c->batt, LV_ALIGN_RIGHT_MID, 0, 0);

    c->timer = lv_timer_create(sb_timer, 5000, c);
    lv_obj_add_event_cb(bar, sb_on_delete, LV_EVENT_DELETE, c);
    sb_refresh(c);
    return bar;
}