#include "ui_common.h"
#include "core/AppManager.h"
#include "core/PowerManager.h"

// ---- коллбэк клика по иконке приложения ----
static void item_cb(lv_event_t* e)
{
    size_t idx = (size_t)(intptr_t)lv_event_get_user_data(e);
    auto& apps = AppManager::inst().apps();
    if (idx >= apps.size()) return;
    PowerManager::inst().resetActivity();
    AppManager::inst().launchApp(apps[idx].factory());
}

// ---- контекст меню ----
struct MenuCtx {
    lv_obj_t* tv       {nullptr};
    lv_obj_t* dots[16] = {nullptr};
    int       count   = 0;      // количество реальных приложений
    int       cur     = 0;      // 0..count-1

    lv_point_t press_pt{};
    uint32_t   press_ms = 0;
    bool       press_seen = false;
};

static void update_dots(MenuCtx* c)
{
    for (int i = 0; i < c->count; ++i) {
        bool active = (i == c->cur);
        lv_obj_set_style_bg_color(c->dots[i],
            active ? lv_color_white() : lv_color_hex(0x444444), 0);
        lv_obj_set_style_width (c->dots[i], active ? 8 : 6, 0);
        lv_obj_set_style_height(c->dots[i], active ? 8 : 6, 0);
    }
}

static void menu_go_to(MenuCtx* c, int real_idx, bool animate)
{
    if (!c || !c->tv || c->count <= 1) return;
    if (real_idx < 0) real_idx = c->count - 1;
    if (real_idx >= c->count) real_idx = 0;

    c->cur = real_idx;
    lv_coord_t w = lv_obj_get_width(c->tv);
    lv_obj_scroll_to_x(c->tv, (real_idx + 1) * w,
                       animate ? LV_ANIM_ON : LV_ANIM_OFF);
    update_dots(c);
}
static inline void menu_next(MenuCtx* c) { menu_go_to(c, c->cur + 1, true); }
static inline void menu_prev(MenuCtx* c) { menu_go_to(c, c->cur - 1, true); }

// ---- SCROLL: точки в движении ----
static void on_scroll(lv_event_t* e)
{
    MenuCtx* c = (MenuCtx*)lv_event_get_user_data(e);
    if (!c || !c->tv || c->count <= 1) return;

    lv_coord_t w = lv_obj_get_width(c->tv);
    lv_coord_t x = lv_obj_get_scroll_x(c->tv);
    if (w <= 0) return;

    int col = (x + w / 2) / w;
    int idx = col - 1;
    if (idx < 0) idx = 0;
    if (idx >= c->count) idx = c->count - 1;
    if (idx != c->cur) {
        c->cur = idx;
        update_dots(c);
    }
}

// ---- SCROLL_END: бесшовный wrap ----
static void on_scroll_end(lv_event_t* e)
{
    MenuCtx* c = (MenuCtx*)lv_event_get_user_data(e);
    if (!c || !c->tv || c->count <= 1) return;

    lv_coord_t w = lv_obj_get_width(c->tv);
    lv_coord_t x = lv_obj_get_scroll_x(c->tv);
    if (w <= 0) return;

    int col = (x + w / 2) / w;

    if (col <= 0) {
        lv_obj_scroll_to_x(c->tv, c->count * w, LV_ANIM_OFF);
        c->cur = c->count - 1;
    } else if (col >= c->count + 1) {
        lv_obj_scroll_to_x(c->tv, 1 * w, LV_ANIM_OFF);
        c->cur = 0;
    } else {
        c->cur = col - 1;
    }
    update_dots(c);
}

// ---- PRESSED / RELEASED: ----
static void on_pressed(lv_event_t* e)
{
    MenuCtx* c = (MenuCtx*)lv_event_get_user_data(e);
    if (!c) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) { c->press_seen = false; return; }
    lv_indev_get_point(indev, &c->press_pt);
    c->press_ms   = lv_tick_get();
    c->press_seen = true;
}

static void on_released(lv_event_t* e)
{
    MenuCtx* c = (MenuCtx*)lv_event_get_user_data(e);
    if (!c || !c->tv || c->count <= 1 || !c->press_seen) return;
    c->press_seen = false;

    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) return;

    lv_point_t p;
    lv_indev_get_point(indev, &p);
    int dx = p.x - c->press_pt.x;
    uint32_t dt = lv_tick_get() - c->press_ms;
    if (dt == 0) dt = 1;

    int abs_dx = dx < 0 ? -dx : dx;

    if (abs_dx < 25) return;
    uint32_t speed = (uint32_t)abs_dx * 1000u / dt;
    if (speed < 180) return;

    if (dx < 0) menu_next(c);
    else        menu_prev(c);
}

// ---- создание одного тайла ----
static void make_tile(lv_obj_t* tv, lv_group_t* g, int tile_col, int app_idx)
{
    auto& apps = AppManager::inst().apps();

    lv_obj_t* tile = lv_tileview_add_tile(tv, tile_col, 0, LV_DIR_HOR);
    lv_obj_set_style_bg_color(tile, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tile, 0, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btn = lv_btn_create(tile);
    lv_obj_set_size(btn, LV_PCT(100), LV_PCT(100));
    lv_obj_center(btn);

    lv_obj_set_style_bg_color(btn, lv_color_black(), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A1A1A), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_outline_width(btn, 0, 0);
    lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(btn, 0, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_border_width (btn, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_shadow_width (btn, 0, LV_STATE_FOCUSED);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(btn, item_cb, LV_EVENT_CLICKED,
                        (void*)(intptr_t)app_idx);
    lv_group_add_obj(g, btn);

    if (apps[app_idx].icon) {
        lv_obj_t* img = UI_IMG_CREATE(btn);
        UI_IMG_SET_SRC(img, apps[app_idx].icon);
        lv_obj_align(img, LV_ALIGN_CENTER, 0, -36);
#if LVGL_VERSION_MAJOR >= 9
        // lv_image_set_scale(img, 256);
#else
        // lv_img_set_zoom(img, 256);
#endif
    }
    lv_obj_t* l = ui_make_label(btn, &font_alibaba_40,
                                lv_color_white(), apps[app_idx].name);
    lv_obj_align(l, LV_ALIGN_CENTER, 0, 56);
}

// ============================================================================
lv_obj_t* ui_build_menu(lv_obj_t* parent, lv_group_t* g)
{
    ui_make_screen(parent);

    auto& apps = AppManager::inst().apps();
    const int n = (int)apps.size();
    if (n == 0) return parent;

    auto* ctx = new MenuCtx{};
    ctx->count = n;

    // ---- tileview ----
    lv_obj_t* tv = lv_tileview_create(parent);
    ctx->tv = tv;
    lv_obj_set_size(tv, LV_PCT(100), LV_PCT(100));
    lv_obj_center(tv);
    lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(tv, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(tv, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tv, 0, 0);
    lv_obj_set_style_border_width(tv, 0, 0);
    lv_obj_set_style_pad_all(tv, 0, 0);

    // Раскладка: [clone n-1] [real 0] [real 1] ... [real n-1] [clone 0]
    if (n > 1) {
        make_tile(tv, g, 0, n - 1);                     // клон последнего
        for (int i = 0; i < n; ++i) make_tile(tv, g, i + 1, i);
        make_tile(tv, g, n + 1, 0);                     // клон первого
    } else {
        make_tile(tv, g, 0, 0);
    }

    // ---- точки-индикатор ----
    if (n > 1) {
        lv_obj_t* dots = lv_obj_create(parent);
        lv_obj_set_size(dots, LV_SIZE_CONTENT, 12);
        lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, -8);
        lv_obj_set_style_bg_opa(dots, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(dots, 0, 0);
        lv_obj_set_style_pad_all(dots, 0, 0);
        lv_obj_set_style_pad_column(dots, 6, 0);
        lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(dots, LV_OBJ_FLAG_SCROLLABLE);

        for (int i = 0; i < n; ++i) {
            lv_obj_t* d = lv_obj_create(dots);
            lv_obj_set_size(d, 6, 6);
            lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(d, lv_color_hex(0x444444), 0);
            lv_obj_set_style_border_width(d, 0, 0);
            lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
            ctx->dots[i] = d;
        }
        lv_obj_move_foreground(dots);
    }

    lv_obj_update_layout(tv);
    if (n > 1) {
        lv_coord_t w = lv_obj_get_width(tv);
        lv_obj_scroll_to_x(tv, w, LV_ANIM_OFF);
    }
    ctx->cur = 0;
    update_dots(ctx);


    lv_obj_add_flag(tv, LV_OBJ_FLAG_EVENT_BUBBLE);   //  PRESSED/RELEASED 
    lv_obj_add_event_cb(tv, on_scroll,      LV_EVENT_SCROLL,     ctx);
    lv_obj_add_event_cb(tv, on_scroll_end,  LV_EVENT_SCROLL_END, ctx);
    lv_obj_add_event_cb(tv, on_pressed,     LV_EVENT_PRESSED,    ctx);
    lv_obj_add_event_cb(tv, on_released,    LV_EVENT_RELEASED,   ctx);

    lv_obj_add_event_cb(parent, [](lv_event_t* e){
        delete (MenuCtx*)lv_event_get_user_data(e);
    }, LV_EVENT_DELETE, ctx);

    // ---- статус-бар ----
    lv_obj_t* sb = ui_make_statusbar(parent);
    lv_obj_move_foreground(sb);

    return parent;
}