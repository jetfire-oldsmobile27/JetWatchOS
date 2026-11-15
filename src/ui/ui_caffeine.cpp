#include "ui_common.h"
#include "core/PowerManager.h"
#include "core/AppManager.h"
#include <stdlib.h>

namespace {

constexpr uint32_t POLL_MS       = 30;
constexpr uint32_t SHOW_AFTER_MS = 400;    // когда появляется оверлей
constexpr uint32_t HOLD_MS       = 4000;   // полное удержание
constexpr uint32_t DONE_SHOW_MS  = 600;
constexpr int      MOVE_TOL      = 24;

enum class St { Idle, Holding, Done, WaitRelease };

St          st        = St::Idle;
lv_indev_t* ptr       = nullptr;
lv_timer_t* tmr       = nullptr;
lv_obj_t*   ov        = nullptr;
lv_obj_t*   arc       = nullptr;
lv_obj_t*   icon      = nullptr;
lv_point_t  p0{};
uint32_t    t0        = 0;
uint32_t    done_at   = 0;
bool        prev_down = false;

lv_indev_t* find_pointer()
{
    for (lv_indev_t* i = lv_indev_get_next(nullptr); i; i = lv_indev_get_next(i))
        if (lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER) return i;
    return nullptr;
}

void overlay_show()
{
    if (ov) return;

    ov = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(ov);
    lv_obj_set_size(ov, LV_PCT(100), LV_PCT(100));
    lv_obj_center(ov);
    lv_obj_set_style_bg_color(ov, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_80, 0);            // затемнение всего
    lv_obj_clear_flag(ov, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_move_foreground(ov);

    arc = lv_arc_create(ov);
    lv_obj_set_size(arc, 160, 160);
    lv_obj_center(arc);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, ui::surface2(), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, ui::accent(),   LV_PART_INDICATOR);

    icon = UI_IMG_CREATE(ov);
    UI_IMG_SET_SRC(icon, &img_coffee);
    lv_obj_set_style_image_recolor(icon, ui::text(), 0);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    lv_obj_center(icon);
}

void overlay_hide()
{
    if (!ov) return;
    UI_OBJ_DEL(ov);
    ov = arc = icon = nullptr;
}

void step(bool down, uint32_t now, bool allowed)
{
    switch (st) {
    case St::Idle:
        if (down && !prev_down && allowed) {          // только фронт нажатия
            lv_indev_get_point(ptr, &p0);
            t0 = now;
            st = St::Holding;
        }
        break;

    case St::Holding: {
        if (!down || !allowed) { overlay_hide(); st = St::Idle; break; }

        lv_point_t p; lv_indev_get_point(ptr, &p);
        if (abs(p.x - p0.x) > MOVE_TOL || abs(p.y - p0.y) > MOVE_TOL) {
            overlay_hide(); st = St::WaitRelease; break;   // это жест/скролл
        }

        uint32_t el = now - t0;
        if (el >= SHOW_AFTER_MS) {
            overlay_show();
            uint32_t span = HOLD_MS - SHOW_AFTER_MS;
            uint32_t v = (el - SHOW_AFTER_MS) * 100 / span;
            lv_arc_set_value(arc, v > 100 ? 100 : (int)v);
        }
        if (el >= HOLD_MS) {
            auto& pm = PowerManager::inst();
            bool on = !pm.isCaffeine();
            pm.setCaffeine(on);
            hw_feedback();
            // Без этого отпускание пальца даст CLICKED на виджете под ним
            lv_indev_wait_release(ptr);

            if (arc) {
                lv_arc_set_value(arc, 100);
                lv_obj_set_style_arc_color(arc, on ? ui::green() : ui::dot(),
                                           LV_PART_INDICATOR);
                lv_obj_set_style_image_recolor(icon, on ? ui::accent() : ui::text3(), 0);
            }
            done_at = now + DONE_SHOW_MS;
            st = St::Done;
        }
        break;
    }

    case St::Done:
        if (now >= done_at) {
            overlay_hide();
            st = down ? St::WaitRelease : St::Idle;
        }
        break;

    case St::WaitRelease:
        if (!down) st = St::Idle;
        break;
    }
}

void poll_cb(lv_timer_t*)
{
    if (!ptr) { ptr = find_pointer(); if (!ptr) return; }

    auto& pm = PowerManager::inst();
    bool down    = lv_indev_get_state(ptr) == LV_INDEV_STATE_PRESSED;
    // На циферблате долгое нажатие занято каруселью -> Caffeine там запрещён
    bool allowed = !pm.isScreenOff() && !AppManager::inst().isLocked();

    if (pm.isScreenOff() && st != St::Idle) { overlay_hide(); st = St::Idle; }

    step(down, lv_tick_get(), allowed);
    prev_down = down;
}

} // namespace

void ui_caffeine_init()
{
    if (!tmr) tmr = lv_timer_create(poll_cb, POLL_MS, nullptr);
}