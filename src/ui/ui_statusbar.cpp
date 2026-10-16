#include "ui_common.h"
#include "core/PowerManager.h"
#include "core/DataCache.hpp"
#include <ctime>

struct SbCtx {
    lv_obj_t*  time{nullptr};
    lv_obj_t*  bt{nullptr};
    lv_obj_t*  batt_tx{nullptr};
    lv_obj_t*  batt_fill{nullptr};
    lv_obj_t*  bolt{nullptr};
    lv_timer_t* timer{nullptr};

    // Кэш предыдущих значений — не дёргаем LVGL без нужды
    int  last_pct{-999};
    bool last_chg{false};
    bool last_pres{false};
    bool last_bt{false};
    bool was_charging{false};   // для детектора старта зарядки
};

static void sb_refresh(SbCtx* c)
{
    if (!datacache::time_valid()) return;
    const struct tm& t = datacache::time();
    lv_label_set_text_fmt(c->time, "%02d:%02d", t.tm_hour, t.tm_min);

    if (!datacache::monitor_valid()) return;
    const monitor_params_t& p = datacache::monitor();

    bool bt = PowerManager::inst().isBtActive();

    if (p.battery_percent == c->last_pct &&
        p.is_charging     == c->last_chg &&
        p.battery_present == c->last_pres &&
        bt                == c->last_bt) {
        return;
    }
    c->last_pct  = p.battery_percent;
    c->last_chg  = p.is_charging;
    c->last_pres = p.battery_present;
    c->last_bt   = bt;

    bool have_batt = p.battery_present && (p.battery_voltage > 2500);
    bool on_usb    = p.usb_voltage > 4500;

    // Текст
    if (have_batt) {
        lv_label_set_text_fmt(c->batt_tx, "%d%%", p.battery_percent);
    } else if (on_usb) {
        lv_label_set_text(c->batt_tx, "USB");
    } else {
        lv_label_set_text(c->batt_tx, "--");
    }

    // Заливка
    lv_color_t fill = ui::text();
    if (p.is_charging && have_batt) {
        fill = ui::green();
    } else if (have_batt && p.battery_percent < 20) {
        fill = ui::red();
    } else if (!have_batt) {
        fill = ui::dot();
    }
    lv_obj_set_style_bg_color(c->batt_fill, fill, 0);

    int w = 2;
    if (have_batt) {
        w = p.battery_percent <= 0 ? 2 : p.battery_percent * 14 / 100;
        if (w < 2) w = 2;
        if (p.battery_percent >= 100) w = 14;
    } else if (on_usb) {
        w = 14;
    }
    lv_obj_set_width(c->batt_fill, w);

    // Молния
    if (p.is_charging && have_batt) {
        if (!c->bolt) {
            c->bolt = lv_label_create(lv_obj_get_parent(c->batt_fill));
            lv_label_set_text(c->bolt, LV_SYMBOL_CHARGE);
            lv_obj_set_style_text_font(c->bolt, &lv_font_montserrat_12, 0);
            lv_obj_align(c->bolt, LV_ALIGN_CENTER, 0, 0);
        }
        lv_obj_set_style_text_color(c->bolt, ui::green(), 0);
        lv_obj_clear_flag(c->bolt, LV_OBJ_FLAG_HIDDEN);
    } else if (c->bolt) {
        lv_obj_add_flag(c->bolt, LV_OBJ_FLAG_HIDDEN);
    }

    // BT
    lv_obj_set_style_text_color(c->bt, bt ? ui::blue() : ui::dot(), 0);
}

static void sb_timer(lv_timer_t* t) {
    sb_refresh(static_cast<SbCtx*>(lv_timer_get_user_data(t)));
}

static void sb_on_delete(lv_event_t* e) {
    auto* c = static_cast<SbCtx*>(lv_event_get_user_data(e));
    if (c->timer) lv_timer_del(c->timer);
    delete c;
}

lv_obj_t* ui_make_statusbar(lv_obj_t* parent)
{
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LV_PCT(100), ui::STATUSBAR_H);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(bar);

    auto* c = new SbCtx{};

    c->time = ui_make_label(bar, F_TINY, ui::text(), "--:--");
    lv_obj_align(c->time, LV_ALIGN_LEFT_MID, 18, 3);

    c->bt = ui_make_label(bar, &lv_font_montserrat_12, ui::dot(),
                          LV_SYMBOL_BLUETOOTH);
    lv_obj_align(c->bt, LV_ALIGN_CENTER, 0, 3);

    c->batt_tx = ui_make_label(bar, F_TINY, ui::text(), "--");
    lv_obj_align(c->batt_tx, LV_ALIGN_RIGHT_MID, -42, 3);

    lv_obj_t* b = lv_obj_create(bar);
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, 18, 9);
    lv_obj_align(b, LV_ALIGN_RIGHT_MID, -18, 3);
    lv_obj_set_style_border_color(b, ui::text2(), 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_side(b, LV_BORDER_SIDE_FULL, 0);
    lv_obj_set_style_radius(b, 3, 0);
    lv_obj_set_style_pad_all(b, 1, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);

    c->batt_fill = lv_obj_create(b);
    lv_obj_remove_style_all(c->batt_fill);
    lv_obj_set_size(c->batt_fill, 4, LV_PCT(100));
    lv_obj_set_style_radius(c->batt_fill, 1, 0);
    lv_obj_set_style_bg_color(c->batt_fill, ui::text(), 0);
    lv_obj_set_style_bg_opa(c->batt_fill, LV_OPA_COVER, 0);
    lv_obj_align(c->batt_fill, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_clear_flag(c->batt_fill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(c->batt_fill, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* nub = lv_obj_create(bar);
    lv_obj_remove_style_all(nub);
    lv_obj_set_size(nub, 2, 3);
    lv_obj_align_to(nub, b, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
    lv_obj_set_style_bg_color(nub, ui::text2(), 0);
    lv_obj_set_style_bg_opa(nub, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(nub, 1, 0);
    lv_obj_clear_flag(nub, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(nub, LV_OBJ_FLAG_SCROLLABLE);

    // 10 с — достаточно, снижает I²C-нагрузку PMU/RTC
    c->timer = lv_timer_create(sb_timer, 10000, c);
    lv_obj_add_event_cb(bar, sb_on_delete, LV_EVENT_DELETE, c);
    sb_refresh(c);
    return bar;
}