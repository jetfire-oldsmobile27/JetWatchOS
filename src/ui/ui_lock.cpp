#include "ui_common.h"
#include "core/PowerManager.h"
#include "faces/face_manager.hpp"
#include "faces/face_carousel.hpp"

static void on_lock_long_press(lv_event_t*)
{
    PowerManager::inst().resetActivity();
    face_carousel_show();
}

lv_obj_t* ui_build_lock(lv_obj_t* parent)
{
    // Контейнер, куда FaceManager монтирует активный циферблат
    lv_obj_t* holder = ui_make_screen(parent, ui::bg());

    FaceManager::inst().begin();
    FaceManager::inst().mountCurrent(holder);

    // Прозрачный "щуп" поверх всего — ловит long-press для выбора face
    lv_obj_t* sense = lv_obj_create(holder);
    lv_obj_remove_style_all(sense);
    lv_obj_set_size(sense, LV_PCT(100), LV_PCT(100));
    lv_obj_center(sense);
    lv_obj_clear_flag(sense, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sense, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(sense);

    lv_obj_add_event_cb(sense, on_lock_long_press,
                        LV_EVENT_LONG_PRESSED, nullptr);
    return parent;
}