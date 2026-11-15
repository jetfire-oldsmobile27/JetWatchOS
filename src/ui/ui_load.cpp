#include "ui_common.h"
#include "core/PowerManager.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

// Ниже 30 мс (≈33 FPS) интерфейс начинает ощущаться «вязким».
// Нормальный интервал loop() ≈ 7–10 мс (delay(5) + работа).
constexpr uint32_t SLOW_INTERVAL_MS  = 30;
// Доля «медленных» итераций в окне, после которой показываем точку.
constexpr float    SLOW_RATIO_ON     = 0.40f;
// Ниже этой доли — прячем (гистерезис против мигания).
constexpr float    SLOW_RATIO_OFF    = 0.15f;
// Размер окна (число итераций loop()).
constexpr int      WINDOW            = 40;
// Как часто пересчитывать состояние точки.
constexpr uint32_t SAMPLE_PERIOD_MS  = 500;
// Минимальное время «показан» / «скрыт» — чтобы не дёргалась на всплесках.
constexpr uint32_t MIN_SHOW_MS       = 2000;
constexpr uint32_t MIN_HIDE_MS       = 1000;

struct LoadState {
    lv_obj_t*  dot             = nullptr;
    uint32_t   last_loop_ms    = 0;
    uint32_t   last_check_ms   = 0;
    uint32_t   min_visible_to  = 0;
    uint32_t   min_hidden_to   = 0;

    uint16_t   intervals[WINDOW] = {0};
    int        idx   = 0;
    int        count = 0;
    bool       shown = false;
};

LoadState s;

void make_dot()
{
    if (s.dot) return;

    s.dot = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s.dot);
    lv_obj_set_size(s.dot, 10, 10);
    lv_obj_align(s.dot, LV_ALIGN_TOP_RIGHT, -6, 6);
    lv_obj_set_style_radius(s.dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s.dot, lv_color_hex(0xFF3030), 0);
    lv_obj_set_style_bg_opa(s.dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s.dot, 0, 0);
    lv_obj_set_style_shadow_width(s.dot, 0, 0);
    lv_obj_clear_flag(s.dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s.dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s.dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s.dot);
}

void set_visible(bool on)
{
    if (!s.dot || s.shown == on) return;
    s.shown = on;
    if (on) {
        lv_obj_clear_flag(s.dot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s.dot);
    } else {
        lv_obj_add_flag(s.dot, LV_OBJ_FLAG_HIDDEN);
    }
}

void compute_and_update()
{
    if (s.count < 5) return;                 // мало данных — не судим

    int slow = 0;
    for (int i = 0; i < s.count; ++i)
        if (s.intervals[i] >= SLOW_INTERVAL_MS) ++slow;

    float ratio = (float)slow / (float)s.count;
    uint32_t now = millis();

    if (!s.shown && ratio >= SLOW_RATIO_ON && now >= s.min_hidden_to) {
        set_visible(true);
        s.min_visible_to = now + MIN_SHOW_MS;
        Serial.printf("[LOAD] ON  ratio=%.2f slow=%d/%d\n", ratio, slow, s.count);
    }
    if (s.shown && ratio <= SLOW_RATIO_OFF && now >= s.min_visible_to) {
        set_visible(false);
        s.min_hidden_to = now + MIN_HIDE_MS;
        Serial.printf("[LOAD] OFF ratio=%.2f slow=%d/%d\n", ratio, slow, s.count);
    }

    // Держим на переднем плане — что бы ни легло сверху (нотификации, диалоги)
    if (s.shown) lv_obj_move_foreground(s.dot);
}

} // namespace

void ui_load_indicator_init()
{
    make_dot();
    s.last_loop_ms = millis();
}

void ui_load_tick()
{
    if (!s.dot) make_dot();

    // Экран выключен — delay(50) в loop() это НЕ «высокая нагрузка».
    // Сбрасываем аккумулятор, чтобы на wake не показывать ложный «загружено».
    if (PowerManager::inst().isScreenOff()) {
        s.last_loop_ms = millis();
        s.idx = 0;
        s.count = 0;
        if (s.shown) set_visible(false);
        return;
    }

    uint32_t now = millis();
    uint32_t dt  = now - s.last_loop_ms;
    s.last_loop_ms = now;

    if (dt == 0 || dt > 5000) return;    // первый цикл после setup() или блокировка

    s.intervals[s.idx] = (uint16_t)(dt > 0xFFFF ? 0xFFFF : dt);
    s.idx = (s.idx + 1) % WINDOW;
    if (s.count < WINDOW) ++s.count;

    if (now - s.last_check_ms < SAMPLE_PERIOD_MS) return;
    s.last_check_ms = now;
    compute_and_update();
}