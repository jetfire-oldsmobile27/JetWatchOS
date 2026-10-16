#include "ui_common.h"
#include "core/PowerManager.h"
#include "core/AppManager.h"
#include "faces/face_manager.hpp"
#include "faces/face_carousel.hpp"
#include <cstring>

static void launch_app_by_name(const char* name)
{
    auto& apps = AppManager::inst().apps();
    for (auto& a : apps) {
        if (a.name && strcmp(a.name, name) == 0) {
            Serial.printf("[LOCK] launch '%s'\n", name);
            AppManager::inst().launchApp(a.factory());
            return;
        }
    }
    Serial.printf("[LOCK] app '%s' not registered\n", name);
}

// ---------- Long press: карусель циферблатов ----------
static void on_lock_long_press(lv_event_t*)
{
    PowerManager::inst().resetActivity();
    face_carousel_show();
}

// ---------- Ручной детектор свайпов ----------
struct LockGestureCtx {
    lv_point_t start{};
    uint32_t   start_ms{0};
    bool       armed{false};
};

static void on_lock_pressed(lv_event_t* e)
{
    auto* c = static_cast<LockGestureCtx*>(lv_event_get_user_data(e));
    if (!c) return;

    if (face_carousel_is_open()) { c->armed = false; return; }

    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) { c->armed = false; return; }
    lv_indev_get_point(indev, &c->start);
    c->start_ms = lv_tick_get();
    c->armed    = true;
}

static void on_lock_released(lv_event_t* e)
{
    auto* c = static_cast<LockGestureCtx*>(lv_event_get_user_data(e));
    if (!c || !c->armed) return;
    c->armed = false;

    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t end;
    lv_indev_get_point(indev, &end);

    int dx  = end.x - c->start.x;
    int dy  = end.y - c->start.y;
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;

    uint32_t dt = lv_tick_get() - c->start_ms;
    if (dt == 0) dt = 1;

    // Горизонталь должна доминировать и пройти хотя бы 40 px
    if (adx < 40) { Serial.printf("[LOCK] no swipe adx=%d\n", adx); return; }
    if (adx < ady) { Serial.printf("[LOCK] too vertical adx=%d ady=%d\n", adx, ady); return; }

    uint32_t speed = (uint32_t)adx * 1000u / dt;   // px/s
    if (speed < 120) { Serial.printf("[LOCK] slow swipe v=%u\n", speed); return; }

    Serial.printf("[LOCK] SWIPE dx=%d dy=%d v=%u dt=%ums\n", dx, dy, speed, dt);

    PowerManager::inst().resetActivity();
    if (dx < 0) launch_app_by_name("Music");   // влево  → музыка
    else        launch_app_by_name("Steps");   // вправо → шаги
}

lv_obj_t* ui_build_lock(lv_obj_t* parent)
{
    lv_obj_t* holder = ui_make_screen(parent, ui::bg());

    FaceManager::inst().begin();
    FaceManager::inst().mountCurrent(holder);

    lv_obj_t* sense = lv_obj_create(holder);
    lv_obj_remove_style_all(sense);
    lv_obj_set_size(sense, LV_PCT(100), LV_PCT(100));
    lv_obj_center(sense);
    lv_obj_clear_flag(sense, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sense, LV_OBJ_FLAG_CLICKABLE);
    // НЕ ставим GESTURE_BUBBLE — иначе правый свайп улетит
    // в AppManager::on_gesture() и уведёт в меню вместо Steps.
    lv_obj_move_foreground(sense);

    lv_obj_add_event_cb(sense, on_lock_long_press, LV_EVENT_LONG_PRESSED, nullptr);

    auto* g = new LockGestureCtx{};
    lv_obj_add_event_cb(sense, on_lock_pressed,  LV_EVENT_PRESSED,  g);
    lv_obj_add_event_cb(sense, on_lock_released, LV_EVENT_RELEASED, g);
    lv_obj_add_event_cb(sense, [](lv_event_t* e){
        delete static_cast<LockGestureCtx*>(lv_event_get_user_data(e));
    }, LV_EVENT_DELETE, g);

    return parent;
}