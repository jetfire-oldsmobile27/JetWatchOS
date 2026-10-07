#include "ui_common.h"

void ui_init_theme()
{
    lv_theme_default_init(nullptr,
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_RED),
        true, LV_FONT_DEFAULT);
}

lv_obj_t* ui_make_screen(lv_obj_t* parent, lv_color_t bg)
{
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_set_size(c, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_radius(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_set_style_bg_color(c, bg, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_center(c);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);   // ← добавлено
    return c;
}

lv_obj_t* ui_make_label(lv_obj_t* parent, const lv_font_t* f,
                        lv_color_t color, const char* txt)
{
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, txt);
    return l;
}

lv_obj_t* ui_make_btn(lv_obj_t* parent, const char* txt,
                      lv_event_cb_t cb, void* ud, int w, int h)
{
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x2A2A2A), 0);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    return b;
}

void set_default_group(lv_group_t* group)
{
    lv_indev_t* cur_drv  {nullptr};
    for (;;) {
        cur_drv = lv_indev_get_next(cur_drv);
        if (!cur_drv) break;
        lv_indev_type_t t = lv_indev_get_type(cur_drv);
        if (t == LV_INDEV_TYPE_KEYPAD ||
            t == LV_INDEV_TYPE_ENCODER ||
            t == LV_INDEV_TYPE_POINTER) {
            lv_indev_set_group(cur_drv, group);
        }
    }
    lv_group_set_default(group);
}

void ui_add_screen_corner_mask(uint8_t corner_radius, uint8_t mask_width)
{
    lv_obj_t* mask = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(mask);

    // Размер — во весь экран
    lv_obj_set_size(mask, LV_PCT(100), LV_PCT(100));
    lv_obj_center(mask);

    // Прозрачный фон, только рамка красит
    lv_obj_set_style_bg_opa(mask, LV_OPA_TRANSP, 0);

    // Чёрная рамка нужной толщины
    lv_obj_set_style_border_color(mask, lv_color_black(), 0);
    lv_obj_set_style_border_opa(mask, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mask, mask_width, 0);
    lv_obj_set_style_border_side(mask, LV_BORDER_SIDE_FULL, 0);

    // Радиус > ширины рамки — тогда и внешний, и внутренний край скруглены
    lv_obj_set_style_radius(mask, corner_radius, 0);

    // Не мешать кликам и скроллу
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(mask, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_tree_walk_res_t _bubble_cb(lv_obj_t* obj, void*)
{
    lv_obj_add_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);
    return LV_OBJ_TREE_WALK_NEXT;
}

void ui_enable_gesture_bubble(lv_obj_t* root)
{
    if (!root) return;
    lv_obj_add_flag(root, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_tree_walk(root, _bubble_cb, nullptr);
}