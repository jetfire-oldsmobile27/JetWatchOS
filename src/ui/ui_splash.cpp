#include "ui_splash.hpp"
#include "ui_common.h"

namespace {

struct Splash {
    lv_obj_t*   overlay   = nullptr;
    lv_obj_t*   title     = nullptr;
    lv_obj_t*   comet     = nullptr;
    lv_obj_t*   bar       = nullptr;
    lv_obj_t*   status    = nullptr;
    lv_timer_t* timer     = nullptr;

    uint32_t    start_ms   = 0;
    int         cur_pct    = 0;
    int         target_pct = 0;
};

Splash s;

// Минимум — длительность стартового звука, 4.5 с
constexpr uint32_t SPLASH_MIN_MS  = 4500;

// CRT-палитра (зелёный люминофор)
constexpr uint32_t C_BG     = 0x04140A;   // почти чёрный, с зелёным сдвигом
constexpr uint32_t C_GLOW   = 0x22DD66;   // основной зелёный
constexpr uint32_t C_DIM    = 0x0E5A22;   // тусклый для подписи
constexpr uint32_t C_TRACK  = 0x0A2412;   // фон прогресс-бара

// Комета: один медленный проблеск — гаснет к концу заставки
constexpr uint32_t COMET_HOLD_MS  = 3000; // держим яркой первые 3 с
constexpr uint32_t COMET_FADE_MS  = 1500; // потом плавно гаснет за 1.5 с

// ------------------------------------------------------------------
//  Плавное подтягивание прогресса + автозакрытие
// ------------------------------------------------------------------
void splash_tick(lv_timer_t*)
{
    if (!s.overlay) return;

    // По 1 % за тик (25 мс) = 40 %/с — успевает от 0 до 100 за ~2.5 с
    if (s.cur_pct != s.target_pct) {
        s.cur_pct += (s.cur_pct < s.target_pct) ? 1 : -1;
        lv_bar_set_value(s.bar, s.cur_pct, LV_ANIM_OFF);
    }

    uint32_t elapsed = lv_tick_get() - s.start_ms;
    if (elapsed >= SPLASH_MIN_MS &&
        s.target_pct >= 100 && s.cur_pct >= 100) {
        ui_splash_end();
    }
}

void comet_opa_cb(void* var, int32_t v)
{
    lv_obj_set_style_text_opa((lv_obj_t*)var, (lv_opa_t)v, 0);
}

} // namespace

// ------------------------------------------------------------------
//  begin
// ------------------------------------------------------------------
void ui_splash_begin()
{
    if (s.overlay) return;

    s.start_ms   = lv_tick_get();
    s.cur_pct    = 0;
    s.target_pct = 0;

    lv_obj_t* ov = lv_obj_create(lv_layer_top());
    s.overlay = ov;
    lv_obj_remove_style_all(ov);
    lv_obj_set_size(ov, LV_PCT(100), LV_PCT(100));
    lv_obj_center(ov);
    lv_obj_set_style_bg_color(ov, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(ov);

    // Строка заголовка: [SEVASTOPOL] [*]
    lv_obj_t* row = lv_obj_create(ov);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, -18);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(row, 4, 0);

    s.title = lv_label_create(row);
    lv_label_set_text(s.title, "SEVASTOPOL");
    lv_obj_set_style_text_font(s.title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s.title, lv_color_hex(C_GLOW), 0);
    lv_obj_set_style_text_letter_space(s.title, 3, 0);
    lv_obj_set_style_shadow_color(s.title, lv_color_hex(C_GLOW), 0);
    lv_obj_set_style_shadow_width(s.title, 8, 0);
    lv_obj_set_style_shadow_opa(s.title, LV_OPA_30, 0);

    s.comet = lv_label_create(row);
    lv_label_set_text(s.comet, "*");
    lv_obj_set_style_text_font(s.comet, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(s.comet, lv_color_hex(C_GLOW), 0);
    lv_obj_set_style_text_opa(s.comet, 255, 0);
    lv_obj_set_style_shadow_color(s.comet, lv_color_hex(C_GLOW), 0);
    lv_obj_set_style_shadow_width(s.comet, 12, 0);
    lv_obj_set_style_shadow_opa(s.comet, LV_OPA_60, 0);
    // поднять над базовой линией, чтобы читалось как «комета сверху»
    lv_obj_set_style_translate_y(s.comet, -12, 0);

    // Прогресс-бар
    s.bar = lv_bar_create(ov);
    lv_obj_set_size(s.bar, LV_PCT(70), 6);
    lv_obj_align(s.bar, LV_ALIGN_CENTER, 0, 40);
    lv_bar_set_range(s.bar, 0, 100);
    lv_bar_set_value(s.bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s.bar, lv_color_hex(C_TRACK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s.bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s.bar, 3, LV_PART_MAIN);
    lv_obj_set_style_border_width(s.bar, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s.bar, lv_color_hex(C_GLOW), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s.bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s.bar, 3, LV_PART_INDICATOR);
    lv_obj_set_style_shadow_color(s.bar, lv_color_hex(C_GLOW), LV_PART_INDICATOR);
    lv_obj_set_style_shadow_width(s.bar, 6, LV_PART_INDICATOR);
    lv_obj_set_style_shadow_opa(s.bar, LV_OPA_40, LV_PART_INDICATOR);

    // Подпись текущего этапа
    s.status = lv_label_create(ov);
    lv_label_set_text(s.status, "booting...");
    lv_obj_set_style_text_font(s.status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s.status, lv_color_hex(C_DIM), 0);
    lv_obj_set_style_text_letter_space(s.status, 1, 0);
    lv_obj_align(s.status, LV_ALIGN_CENTER, 0, 62);

    // Тикер
    s.timer = lv_timer_create(splash_tick, 25, nullptr);

    // Комета: горит 3 с, потом плавно гаснет за 1.5 с
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s.comet);
    lv_anim_set_exec_cb(&a, comet_opa_cb);
    lv_anim_set_values(&a, 255, 0);
    lv_anim_set_delay(&a, COMET_HOLD_MS);
    lv_anim_set_time(&a, COMET_FADE_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

// ------------------------------------------------------------------
//  step — вызывается между этапами инициализации
// ------------------------------------------------------------------
void ui_splash_step(const char* text, uint8_t percent)
{
    if (!s.overlay) return;
    if (percent > 100) percent = 100;
    s.target_pct = percent;
    if (text && s.status) lv_label_set_text(s.status, text);

    // Во время setup() main-loop не крутится — LVGL сам не отрисуется.
    // Здесь мы явно даём ему шанс, чтобы прогресс-бар сдвинулся.
    lv_task_handler();
}

// ------------------------------------------------------------------
//  end
// ------------------------------------------------------------------
void ui_splash_end()
{
    if (!s.overlay) return;
    if (s.timer) { lv_timer_del(s.timer); s.timer = nullptr; }

    lv_obj_t* ov = s.overlay;
    s.overlay = nullptr;
    s.title = s.comet = s.bar = s.status = nullptr;
    s.start_ms = 0;
    s.cur_pct = s.target_pct = 0;

    UI_OBJ_DEL_ASYNC(ov);
}

bool ui_splash_active() { return s.overlay != nullptr; }