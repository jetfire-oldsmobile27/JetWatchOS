#include "face_carousel.hpp"
#include "face_manager.hpp"
#include "ui/ui_common.h"
#include "core/PowerManager.h"

static lv_obj_t* s_overlay  = nullptr;
static lv_obj_t* s_tv       = nullptr;
static lv_obj_t* s_dots[32] = {nullptr};
static int       s_cur      = 0;
static int       s_count    = 0;

static void update_dots();

// ---------------------------------------------------------------------------
//  Один тайл карусели
// ---------------------------------------------------------------------------
static void make_tile(lv_obj_t* tv, int tile_col, int face_idx)
{
    const auto& e = FaceManager::inst().at(face_idx);

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
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(btn, [](lv_event_t* e){
        int idx = (int)(intptr_t)lv_event_get_user_data(e);
        FaceManager::inst().select(idx, true);
        PowerManager::inst().resetActivity();
        face_carousel_hide();
    }, LV_EVENT_SHORT_CLICKED, (void*)(intptr_t)face_idx);

    if (e.preview) {
        lv_obj_t* img = UI_IMG_CREATE(btn);
        UI_IMG_SET_SRC(img, e.preview);
        lv_obj_align(img, LV_ALIGN_CENTER, 0, -20);
    } else {
        // Placeholder — круг с инициалом
        lv_obj_t* ph = lv_obj_create(btn);
        lv_obj_set_size(ph, 160, 160);
        lv_obj_align(ph, LV_ALIGN_CENTER, 0, -20);
        lv_obj_set_style_radius(ph, 80, 0);
        lv_obj_set_style_bg_color(ph, lv_color_hex(0x333333), 0);
        lv_obj_set_style_border_width(ph, 0, 0);
        lv_obj_clear_flag(ph, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* ini = ui_make_label(ph, &font_alibaba_60,
                                      lv_color_white(),
                                      e.name[0] ? "D" : "?");
        lv_obj_center(ini);
    }

    lv_obj_t* lbl = ui_make_label(btn, &font_alibaba_24,
                                  lv_color_white(), e.name);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 80);
}


// ---------------------------------------------------------------------------
//  show / hide
// ---------------------------------------------------------------------------
void face_carousel_show()
{
    if (s_overlay) return;

    auto& fm = FaceManager::inst();
    s_count = fm.count();
    if (s_count == 0) return;
    s_cur = fm.currentIndex();
    if (s_cur < 0 || s_cur >= s_count) s_cur = 0;

    s_overlay = lv_obj_create(UI_SCR_ACT());
    lv_obj_set_size(s_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_center(s_overlay);
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_overlay, 0, 0);
    lv_obj_set_style_radius(s_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_overlay, 0, 0);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(s_overlay);

    s_tv = lv_tileview_create(s_overlay);
    lv_obj_set_size(s_tv, LV_PCT(100), LV_PCT(100));
    lv_obj_center(s_tv);
    lv_obj_set_scrollbar_mode(s_tv, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(s_tv, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_tv, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_tv, 0, 0);
    lv_obj_set_style_border_width(s_tv, 0, 0);
    lv_obj_set_style_pad_all(s_tv, 0, 0);

    for (int i = 0; i < s_count; ++i) make_tile(s_tv, i, i);

    // ---- точки-индикатор ----
    lv_obj_t* dots = lv_obj_create(s_overlay);
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

    for (int i = 0; i < s_count; ++i) {
        lv_obj_t* d = lv_obj_create(dots);
        lv_obj_set_size(d, 6, 6);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(d, lv_color_hex(0x444444), 0);
        lv_obj_set_style_border_width(d, 0, 0);
        lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
        s_dots[i] = d;
    }
    lv_obj_move_foreground(dots);

    lv_obj_update_layout(s_tv);
    lv_coord_t w = lv_obj_get_width(s_tv);
    lv_obj_scroll_to_x(s_tv, s_cur * w, LV_ANIM_OFF);
    update_dots();

    lv_obj_add_event_cb(s_tv, [](lv_event_t*){
        if (!s_tv) return;
        lv_coord_t ww = lv_obj_get_width(s_tv);
        lv_coord_t x  = lv_obj_get_scroll_x(s_tv);
        if (ww <= 0) return;
        int col = (x + ww / 2) / ww;
        if (col < 0) col = 0;
        if (col >= s_count) col = s_count - 1;
        if (col != s_cur) { s_cur = col; update_dots(); }
    }, LV_EVENT_SCROLL, nullptr);

    lv_obj_add_event_cb(s_tv, [](lv_event_t*){
        if (!s_tv) return;
        lv_coord_t ww = lv_obj_get_width(s_tv);
        lv_coord_t x  = lv_obj_get_scroll_x(s_tv);
        if (ww <= 0) return;
        int col = (x + ww / 2) / ww;
        if (col < 0) col = 0;
        if (col >= s_count) col = s_count - 1;
        s_cur = col;
        update_dots();
    }, LV_EVENT_SCROLL_END, nullptr);
}

void face_carousel_hide()
{
    if (!s_overlay) return;
    UI_OBJ_DEL(s_overlay);
    s_overlay = nullptr;
    s_tv      = nullptr;
    for (int i = 0; i < 32; ++i) s_dots[i] = nullptr;
}

bool face_carousel_is_open() { return s_overlay != nullptr; }

static void update_dots()
{
    for (int i = 0; i < s_count; ++i) {
        bool active = (i == s_cur);
        lv_obj_set_style_bg_color(s_dots[i],
            active ? lv_color_white() : lv_color_hex(0x444444), 0);
        lv_obj_set_style_width (s_dots[i], active ? 8 : 6, 0);
        lv_obj_set_style_height(s_dots[i], active ? 8 : 6, 0);
    }
}