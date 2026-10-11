#include "ui_common.h"
#include "core/PowerManager.h"
#include <ctime>

// =====================================================================
//  THEME
// =====================================================================
void ui_init_theme() {
    lv_theme_t* th = lv_theme_default_init(nullptr, ui::accent(), ui::red(),
                                           true, LV_FONT_DEFAULT);
    lv_display_set_theme(nullptr, th);

    lv_obj_set_style_anim_duration(lv_screen_active(), 120, 0);
}

// =====================================================================
//  SCREEN / LABEL / BUTTON
// =====================================================================
lv_obj_t* ui_make_screen(lv_obj_t* parent, lv_color_t bg)
{
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_size(c, LV_PCT(100), LV_PCT(100));
    lv_obj_center(c);
    lv_obj_set_style_bg_color(c, bg, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

lv_obj_t* ui_make_label(lv_obj_t* parent, const lv_font_t* f,
                        lv_color_t color, const char* txt)
{
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_label_set_text(l, txt ? txt : "");
    return l;
}

// ---------------------------------------------------------------------
//  Круглая кнопка в стиле .btn (radius:999px, bg surface-2, press press)
// ---------------------------------------------------------------------
static void _style_pill(lv_obj_t* b, lv_color_t bg_col, lv_color_t txt_col)
{
    lv_obj_remove_style_all(b);
    lv_obj_set_style_bg_color(b, bg_col, 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, ui::press(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_text_color(b, txt_col, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t* _mk_pill(lv_obj_t* parent, const char* txt,
                          lv_color_t bg_col, lv_color_t txt_col,
                          lv_event_cb_t cb, void* ud, int w, int h)
{
    lv_obj_t* b = lv_obj_create(parent);
    _style_pill(b, bg_col, txt_col);
    lv_obj_set_size(b, w, h);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);

    lv_obj_t* l = ui_make_label(b, F_BODY, txt_col, txt);
    lv_obj_center(l);
    return b;
}

lv_obj_t* ui_make_btn(lv_obj_t* parent, const char* txt,
                      lv_event_cb_t cb, void* ud, int w, int h)
{
    return _mk_pill(parent, txt, ui::surface2(), ui::text(), cb, ud, w, h);
}

lv_obj_t* ui_make_btn_pri(lv_obj_t* parent, const char* txt,
                          lv_event_cb_t cb, void* ud, int w, int h)
{
    return _mk_pill(parent, txt, ui::accent(), lv_color_hex(0x000000),
                    cb, ud, w, h);
}

lv_obj_t* ui_make_btn_red(lv_obj_t* parent, const char* txt,
                          lv_event_cb_t cb, void* ud, int w, int h)
{
    return _mk_pill(parent, txt, ui::red(), ui::text(), cb, ud, w, h);
}

lv_obj_t* ui_make_btn_green(lv_obj_t* parent, const char* txt,
                            lv_event_cb_t cb, void* ud, int w, int h)
{
    return _mk_pill(parent, txt, ui::green(), lv_color_hex(0x000000),
                    cb, ud, w, h);
}

// =====================================================================
//  LIST  (.list в HTML)
// =====================================================================
lv_obj_t* ui_make_list(lv_obj_t* parent, lv_coord_t top)
{
    lv_obj_t* list = lv_obj_create(parent);
    lv_obj_remove_style_all(list);
    lv_obj_set_size(list, LV_PCT(100), ui::H - top);
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_left(list, 12, 0);
    lv_obj_set_style_pad_right(list, 12, 0);
    lv_obj_set_style_pad_top(list, 4, 0);
    lv_obj_set_style_pad_bottom(list, ui::LIST_PAD_BOTTOM, 0);
    lv_obj_set_style_pad_row(list, ui::ROW_GAP, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    return list;
}

// =====================================================================
//  ROW  (.row в HTML)
// =====================================================================
lv_obj_t* ui_make_row(lv_obj_t* parent,
                      const char* name, const char* value,
                      bool chevron, lv_event_cb_t cb, void* ud)
{
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), ui::ROW_H);
    lv_obj_set_style_radius(row, ui::ROW_RADIUS, 0);
    lv_obj_set_style_bg_color(row, ui::surface(), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(row, ui::surface2(), LV_STATE_PRESSED);
    lv_obj_set_style_pad_left(row, ui::ROW_PAD_H, 0);
    lv_obj_set_style_pad_right(row, ui::ROW_PAD_H, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* nm = ui_make_label(row, F_BODY, ui::text(), name);
    lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(nm, 1);

    if (value && value[0]) {
        lv_obj_t* vl = ui_make_label(row, F_BODY, ui::text2(), value);
        lv_obj_set_style_max_width(vl, LV_PCT(50), 0);
        lv_label_set_long_mode(vl, LV_LABEL_LONG_DOT);
    }
    if (chevron) {
        ui_make_label(row, F_BODY, ui::text3(), ">");
    }

    if (cb) {
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, cb, LV_EVENT_CLICKED, ud);
    }
    return row;
}

lv_obj_t* ui_make_row_info(lv_obj_t* parent, const char* name,
                           lv_obj_t** out_value)
{
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), ui::ROW_INFO_H);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_set_style_bg_color(row, ui::surface(), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(row, 10, 0);
    lv_obj_set_style_pad_right(row, 10, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    ui_make_label(row, F_TINY, ui::text2(), name);

    lv_obj_t* val = ui_make_label(row, F_TINY, ui::text(), "--");
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);
    if (out_value) *out_value = val;
    return row;
}

// =====================================================================
//  SEPARATOR
// =====================================================================
lv_obj_t* ui_make_separator(lv_obj_t* parent, const char* title)
{
    lv_obj_t* s = lv_obj_create(parent);
    lv_obj_remove_style_all(s);
    lv_obj_set_size(s, LV_PCT(100), 18);
    lv_obj_set_style_pad_left(s, 4, 0);
    lv_obj_set_style_pad_top(s, 8, 0);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    ui_make_label(s, F_TINY, ui::accent(), title);
    return s;
}

// =====================================================================
//  SWITCH (.sw в HTML: 40×22, трек DOT, активный — зелёный)
// =====================================================================
lv_obj_t* ui_make_switch(lv_obj_t* parent, bool on,
                         lv_event_cb_t cb, void* ud)
{
    lv_obj_t* sw = lv_obj_create(parent);
    lv_obj_remove_style_all(sw);
    lv_obj_set_size(sw, 40, 22);
    lv_obj_set_style_radius(sw, 11, 0);
    lv_obj_set_style_bg_color(sw, on ? ui::green() : ui::dot(), 0);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
    lv_obj_clear_flag(sw, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sw, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* knob = lv_obj_create(sw);
    lv_obj_remove_style_all(knob);
    lv_obj_set_size(knob, 18, 18);
    lv_obj_set_style_radius(knob, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(knob, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(knob, LV_OPA_COVER, 0);
    lv_obj_align(knob, on ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID,
                 on ? -2 : 2, 0);
    lv_obj_clear_flag(knob, LV_OBJ_FLAG_CLICKABLE);

    if (cb) {
        // Тоггл рисуем сами — удобно, т.к. токены всё равно свои
        struct Ctx { lv_obj_t* self; lv_obj_t* knob; lv_event_cb_t cb; void* ud; };
        auto* c = ui::attach_ctx<Ctx>(sw, LV_EVENT_CLICKED,
            [](lv_event_t* e){
                auto* c = (Ctx*)lv_event_get_user_data(e);
                bool on_now = lv_obj_has_state(c->self, LV_STATE_CHECKED);
                on_now = !on_now;
                if (on_now) lv_obj_add_state(c->self, LV_STATE_CHECKED);
                else        lv_obj_clear_state(c->self, LV_STATE_CHECKED);
                lv_obj_set_style_bg_color(c->self,
                    on_now ? ui::green() : ui::dot(), 0);
                lv_obj_align(c->knob,
                    on_now ? LV_ALIGN_RIGHT_MID : LV_ALIGN_LEFT_MID,
                    on_now ? -2 : 2, 0);
                c->cb(e);
            },
            sw, knob, cb, ud);
        (void)c;

        if (on) lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    return sw;
}

// =====================================================================
//  DISC  (.mdisc в HTML: 96×96 круг с иконкой)
// =====================================================================
lv_obj_t* ui_make_disc(lv_obj_t* parent, const void* icon_src,
                       const char* caption)
{
    lv_obj_t* disc = lv_obj_create(parent);
    lv_obj_remove_style_all(disc);
    lv_obj_set_size(disc, 96, 96);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc, ui::surface(), 0);
    lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);
    lv_obj_clear_flag(disc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(disc, LV_OBJ_FLAG_CLICKABLE);   // пусть тап уходит на btn

    if (icon_src) {
        lv_obj_t* img = UI_IMG_CREATE(disc);
        UI_IMG_SET_SRC(img, icon_src);

        // ВАЖНО: сначала фиксируем габарит, потом масштаб.
        // Иконки в img_*.c — 24×24, при scale=512 получаем 48×48.
        lv_obj_set_size(img, 48, 48);
#if LVGL_VERSION_MAJOR >= 9
        lv_image_set_scale(img, 512);
#else
        lv_img_set_zoom(img, 512);
#endif
        lv_obj_center(img);
    }

    if (caption && caption[0]) {
        lv_obj_t* cap = ui_make_label(parent, F_TITLE, ui::text(), caption);
        lv_obj_align_to(cap, disc, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);
    }
    return disc;
}

// =====================================================================
//  DOTS  (.dots в HTML: 6 px dot, активный — 14×6 pill)
// =====================================================================
lv_obj_t* ui_make_dots(lv_obj_t* parent, int count, lv_coord_t bottom)
{
    if (count <= 1) return nullptr;

    lv_obj_t* d = lv_obj_create(parent);
    lv_obj_remove_style_all(d);
    lv_obj_set_size(d, LV_SIZE_CONTENT, 12);
    lv_obj_align(d, LV_ALIGN_BOTTOM_MID, 0, -bottom);
    lv_obj_set_style_pad_column(d, 6, 0);
    lv_obj_set_flex_flow(d, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(d, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(d, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < count; ++i) {
        lv_obj_t* dot = lv_obj_create(d);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 6, 6);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, ui::dot(), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    }
    return d;
}

void ui_dots_set(lv_obj_t* dots, int idx)
{
    if (!dots) return;
    uint32_t n = lv_obj_get_child_count(dots);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t* d = lv_obj_get_child(dots, i);
        bool on = ((int)i == idx);
        lv_obj_set_style_bg_color(d, on ? ui::text() : ui::dot(), 0);
        if (on) {
            lv_obj_set_width(d, 14);
            lv_obj_set_style_radius(d, 3, 0);
        } else {
            lv_obj_set_width(d, 6);
            lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        }
    }
}

// =====================================================================
//  BANNER  (.banner в HTML)
// =====================================================================
static lv_obj_t* s_banner = nullptr;

void ui_banner(const char* app, const char* title, const char* body)
{
    if (s_banner) { UI_OBJ_DEL(s_banner); s_banner = nullptr; }

    lv_obj_t* b = lv_obj_create(UI_SCR_ACT());
    lv_obj_remove_style_all(b);
    lv_obj_set_size(b, LV_PCT(95), 62);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_radius(b, 14, 0);
    lv_obj_set_style_bg_color(b, ui::pop(), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(b, 10, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(b);

    lv_obj_t* l = ui_make_label(b, F_TINY, ui::text(), "");
    lv_label_set_text_fmt(l, "%s: %s\n%s", app ? app : "",
                          title ? title : "", body ? body : "");
    lv_obj_set_width(l, LV_PCT(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);

    s_banner = b;
    lv_timer_t* t = lv_timer_create([](lv_timer_t* tt){
        if (s_banner) { UI_OBJ_DEL(s_banner); s_banner = nullptr; }
        lv_timer_del(tt);
    }, 4000, nullptr);
    lv_timer_set_repeat_count(t, 1);
}

// =====================================================================
//  UTILS
// =====================================================================
void set_default_group(lv_group_t* group)
{
    lv_indev_t* cur = nullptr;
    for (;;) {
        cur = lv_indev_get_next(cur);
        if (!cur) break;
        lv_indev_type_t t = lv_indev_get_type(cur);
        if (t == LV_INDEV_TYPE_KEYPAD ||
            t == LV_INDEV_TYPE_ENCODER ||
            t == LV_INDEV_TYPE_POINTER) {
            lv_indev_set_group(cur, group);
        }
    }
    lv_group_set_default(group);
}

void ui_add_screen_corner_mask(uint8_t r, uint8_t w)
{
    lv_obj_t* mask = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(mask);
    lv_obj_set_size(mask, LV_PCT(100), LV_PCT(100));
    lv_obj_center(mask);
    lv_obj_set_style_bg_opa(mask, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(mask, ui::bg(), 0);
    lv_obj_set_style_border_opa(mask, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mask, w, 0);
    lv_obj_set_style_border_side(mask, LV_BORDER_SIDE_FULL, 0);
    lv_obj_set_style_radius(mask, r, 0);
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

void ui_msg_pop_up(const char* title, const char* msg)
{
    ui_banner("System", title, msg);
}