#include "ui_common.h"
#include "core/AppManager.h"
#include "core/PowerManager.h"

struct MenuCtx {
    lv_obj_t*  tv    {nullptr};
    lv_obj_t*  dots  {nullptr};
    int        count {0};
    int        cur   {0};

    lv_point_t press_pt {};
    uint32_t   press_ms {0};
    bool       press_seen {false};
};

// ---------------------------------------------------------------
//  Тайл: минимум вложенности, иконка и подпись — прямые дети tile
// ---------------------------------------------------------------
static void make_tile(lv_obj_t* tv, lv_group_t* g, int col, int app_idx)
{
    auto& apps = AppManager::inst().apps();

    lv_obj_t* tile = lv_tileview_add_tile(tv, col, 0, LV_DIR_HOR);
    lv_obj_set_style_bg_color(tile, ui::bg(), 0);
    lv_obj_set_style_bg_opa (tile, LV_OPA_COVER, 0);
    lv_obj_set_style_radius  (tile, 0, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_pad_all (tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);

    // SHORT_CLICKED срабатывает даже если палец чуть сдвинулся при тапе.
    // CLICKED в LVGL требует полного отсутствия движения и часто «пропадает»
    // на tileview — из-за этого клики по приложениям не работали.
    lv_obj_add_event_cb(tile, [](lv_event_t* e){
        size_t idx = (size_t)(intptr_t)lv_event_get_user_data(e);
        auto& a = AppManager::inst().apps();
        if (idx >= a.size()) return;
        PowerManager::inst().resetActivity();
        AppManager::inst().launchApp(a[idx].factory());
    }, LV_EVENT_SHORT_CLICKED, (void*)(intptr_t)app_idx);

    lv_group_add_obj(g, tile);

    // ---------- Круг-подложка ----------
    // Экран 240×240, все иконки теперь 96×96 (одинаковые).
    // Диск 116×116 — иконка занимает ~83 % диаметра, кружок читается
    // как «плитка приложения», а не как мелкая кнопка.
    // Центр диска = (120, 90) — как и раньше, чуть выше геометрического
    // центра, чтобы оставить место подписи снизу.
    constexpr int DISC = 116;
    constexpr int DISC_X = (240 - DISC) / 2;   // = 62
    constexpr int DISC_Y = 90 - DISC / 2;      // = 32

    lv_obj_t* disc = lv_obj_create(tile);
    lv_obj_remove_style_all(disc);
    lv_obj_set_size(disc, DISC, DISC);
    lv_obj_set_pos(disc, DISC_X, DISC_Y);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc, lv_color_hex(0x050505), 0);
    lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);
    lv_obj_clear_flag(disc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(disc, LV_OBJ_FLAG_SCROLLABLE);

    if (apps[app_idx].icon) {
        lv_obj_t* img = UI_IMG_CREATE(disc);
        UI_IMG_SET_SRC(img, apps[app_idx].icon);

        // Все иконки 96×96 — рисуем 1:1 (scale 256).
        // Никаких апскейлов/даунскейлов: пиксель-в-пиксель, без
        // дизеринга и «мыла» на чёрном фоне.
        lv_obj_set_size(img, 96, 96);
#if LVGL_VERSION_MAJOR >= 9
        lv_image_set_scale(img, 256);
        lv_image_set_antialias(img, false);
#else
        lv_img_set_zoom(img, 256);
#endif
        lv_obj_center(img);
        lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    }

    // ---------- Подпись ----------
    // Диск занимает y = 32..148. Подпись ставим сразу под ним.
    lv_obj_t* name = ui_make_label(tile, F_TITLE, ui::text(),
                                   apps[app_idx].name);
    lv_obj_set_width(name, 240);
    lv_obj_set_pos(name, 0, 156);
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_clear_flag(name, LV_OBJ_FLAG_CLICKABLE);
}

// ---------------------------------------------------------------
static void menu_go_to(MenuCtx* c, int real_idx, bool anim)
{
    if (!c || !c->tv || c->count <= 1) return;
    if (real_idx < 0) real_idx = c->count - 1;
    if (real_idx >= c->count) real_idx = 0;
    c->cur = real_idx;

    lv_coord_t w = lv_obj_get_width(c->tv);
    lv_obj_scroll_to_x(c->tv, (real_idx + 1) * w,
                       anim ? LV_ANIM_ON : LV_ANIM_OFF);
    ui_dots_set(c->dots, c->cur);
}
static inline void menu_next(MenuCtx* c) { menu_go_to(c, c->cur + 1, true); }
static inline void menu_prev(MenuCtx* c) { menu_go_to(c, c->cur - 1, true); }

// ---------------------------------------------------------------
//  ВАЖНО: форс-инвалидация на каждый скролл — компенсирует
//  кривой scroll-shift на этом дисплее.
// ---------------------------------------------------------------
static void on_scroll(lv_event_t* e)
{
    MenuCtx* c = (MenuCtx*)lv_event_get_user_data(e);
    if (!c || !c->tv) return;
    lv_obj_invalidate(c->tv);            // ← ключевая строка

    if (c->count <= 1) return;
    lv_coord_t w = lv_obj_get_width(c->tv);
    lv_coord_t x = lv_obj_get_scroll_x(c->tv);
    if (w <= 0) return;
    int col = (x + w / 2) / w;
    int idx = col - 1;
    if (idx < 0) idx = 0;
    if (idx >= c->count) idx = c->count - 1;
    if (idx != c->cur) { c->cur = idx; ui_dots_set(c->dots, idx); }
}

static void on_scroll_end(lv_event_t* e)
{
    MenuCtx* c = (MenuCtx*)lv_event_get_user_data(e);
    if (!c || !c->tv || c->count <= 1) return;
    lv_obj_invalidate(c->tv);            // ← и на end тоже

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
    ui_dots_set(c->dots, c->cur);
}

static void on_pressed(lv_event_t* e)
{
    MenuCtx* c = (MenuCtx*)lv_event_get_user_data(e);
    if (!c) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) { c->press_seen = false; return; }
    lv_indev_get_point(indev, &c->press_pt);
    c->press_ms = lv_tick_get();
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

// ---------------------------------------------------------------
//  Края — тап на 10% слева/справа листает
// ---------------------------------------------------------------
static void make_edge(lv_obj_t* parent, MenuCtx* ctx, bool left)
{
    lv_obj_t* z = lv_obj_create(parent);
    lv_obj_remove_style_all(z);
    lv_obj_set_size(z, LV_PCT(10), LV_PCT(100));
    lv_obj_align(z, left ? LV_ALIGN_LEFT_MID : LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_flag(z, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(z, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(z);

    lv_obj_set_user_data(z, (void*)(intptr_t)(left ? 1 : 0));
    lv_obj_add_event_cb(z, [](lv_event_t* e){
        auto* c = (MenuCtx*)lv_event_get_user_data(e);
        bool l = (bool)(intptr_t)lv_obj_get_user_data(HW_EV_OBJ(e));
        if (l) menu_prev(c); else menu_next(c);
    }, LV_EVENT_SHORT_CLICKED, ctx);
}

// ---------------------------------------------------------------
//  Build
//  ВАЖНО: tileview НЕ перекрывает статусбар — статусбар 28 px
//  сверху, tileview сидит ниже на 28 px. Это устраняет
//  «пол-экрана не меняется» при скролле.
// ---------------------------------------------------------------
lv_obj_t* ui_build_menu(lv_obj_t* parent, lv_group_t* g)
{
    ui_make_screen(parent);

    auto& apps = AppManager::inst().apps();
    const int n = (int)apps.size();
    if (n == 0) return parent;

    auto* ctx = new MenuCtx{};
    ctx->count = n;

    // ---- tileview (под статусбаром) ----
    lv_obj_t* tv = lv_tileview_create(parent);
    ctx->tv = tv;
    lv_obj_set_size(tv, LV_PCT(100), ui::H - ui::STATUSBAR_H);
    lv_obj_align(tv, LV_ALIGN_BOTTOM_MID, 0, 0);   // ← было lv_obj_center
    lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(tv, ui::bg(), 0);
    lv_obj_set_style_bg_opa (tv, LV_OPA_COVER, 0);
    lv_obj_set_style_radius  (tv, 0, 0);
    lv_obj_set_style_border_width(tv, 0, 0);
    lv_obj_set_style_pad_all (tv, 0, 0);
    lv_obj_set_scroll_dir(tv, LV_DIR_HOR);
    // Отключаем scroll-моментум, чтобы не было «полу-прокрученного» кадра
    lv_obj_remove_flag(tv, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_remove_flag(tv, LV_OBJ_FLAG_SCROLL_ELASTIC);

    // [clone n-1] [real 0] ... [real n-1] [clone 0]
    if (n > 1) {
        make_tile(tv, g, 0, n - 1);
        for (int i = 0; i < n; ++i) make_tile(tv, g, i + 1, i);
        make_tile(tv, g, n + 1, 0);
    } else {
        make_tile(tv, g, 0, 0);
    }

    // ---- dots ----
    if (n > 1) {
        ctx->dots = ui_make_dots(parent, n, 14);
        if (ctx->dots) {
            lv_obj_move_foreground(ctx->dots);
            ui_dots_set(ctx->dots, 0);
        }
    }

    lv_obj_update_layout(tv);
    if (n > 1) {
        lv_coord_t w = lv_obj_get_width(tv);
        lv_obj_scroll_to_x(tv, w, LV_ANIM_OFF);
    }
    ctx->cur = 0;

    // ---- события тайлвью ----
    lv_obj_add_event_cb(tv, on_scroll,     LV_EVENT_SCROLL,     ctx);
    lv_obj_add_event_cb(tv, on_scroll_end, LV_EVENT_SCROLL_END, ctx);
    lv_obj_add_event_cb(tv, on_pressed,    LV_EVENT_PRESSED,    ctx);
    lv_obj_add_event_cb(tv, on_released,   LV_EVENT_RELEASED,   ctx);

    lv_obj_add_event_cb(parent, [](lv_event_t* e){
        delete (MenuCtx*)lv_event_get_user_data(e);
    }, LV_EVENT_DELETE, ctx);

    if (n > 1) {
        make_edge(parent, ctx, true);
        make_edge(parent, ctx, false);
    }

    ui_make_statusbar(parent);
    return parent;
}