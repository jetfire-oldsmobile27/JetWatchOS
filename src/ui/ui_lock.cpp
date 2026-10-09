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
    // Контейнер, в который FaceManager монтирует активный face
    lv_obj_t* holder = ui_make_screen(parent);

    FaceManager::inst().begin();
    FaceManager::inst().mountCurrent(holder);

    // Прозрачный «щуп» поверх ВСЕГО содержимого holder.
    // Он — самый последний ребёнок, значит поверх всех картинок face,
    // и получает все тапы/лонг-тапы.
    lv_obj_t* sense = lv_obj_create(holder);
    lv_obj_remove_style_all(sense);                 // ни bg, ни border, ни pad
    lv_obj_set_size(sense, LV_PCT(100), LV_PCT(100));
    lv_obj_center(sense);
    lv_obj_clear_flag(sense, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sense, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(sense);

    lv_obj_add_event_cb(sense, on_lock_long_press,
                        LV_EVENT_LONG_PRESSED, nullptr);

    // При желании можно и короткий тап сюда повесить,
    // чтобы будить экран от касания (сейчас это делает PowerManager::poll()).
    // lv_obj_add_event_cb(sense, [](lv_event_t*){ PowerManager::inst().resetActivity(); },
    //                     LV_EVENT_CLICKED, nullptr);

    return parent;
}