#include "apps.h"
#include "ui/ui_common.h"
#include "core/PowerManager.h"

class StopwatchApp final : public App {
public:
    const char* name() const override { return "Stopwatch"; }

    void onEnter(lv_obj_t* parent) override {
        _p = parent;

        ui_make_screen(parent, ui::bg());
        ui_make_statusbar(parent);

        // Заголовок
        lv_obj_t* title = ui_make_label(parent, F_TITLE, ui::text2(), "Stopwatch");
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 36);

        // Цифры — крупные, tabular-nums через моноширинный сброс
        _lbl = ui_make_label(parent, F_HERO, ui::text(), "00:00.00");
        lv_obj_align(_lbl, LV_ALIGN_CENTER, 0, -22);

        // Подпись "lap"
        _lap = ui_make_label(parent, F_TINY, ui::text3(), "tap Lap to record");
        lv_obj_align(_lap, LV_ALIGN_CENTER, 0, 26);

        // Кнопки снизу — круглые (76×76), как .rbtn в HTML
        _b_reset = _mk_round(parent, "Reset", ui::surface2(), ui::text(),
                             cb_reset, this, -40);
        _b_start = _mk_round(parent, "Start", ui::green(), lv_color_hex(0),
                             cb_start, this,  40);

        _t0 = lv_tick_get();
        _elapsed = 0;
        _running = false;
        _t = lv_timer_create(cb_tick, 30, this);
    }

    void onExit() override
    {
        if (_t) { lv_timer_del(_t); _t = nullptr; }
    }

private:
    lv_obj_t* _p       {nullptr};
    lv_obj_t* _lbl     {nullptr};
    lv_obj_t* _lap     {nullptr};
    lv_obj_t* _b_reset {nullptr};
    lv_obj_t* _b_start {nullptr};
    lv_timer_t* _t     {nullptr};
    uint32_t _t0 {0}, _elapsed {0};
    int      _lap_n {0};
    bool     _running {false};

    // -----------------------------------------------------------------
    static lv_obj_t* _mk_round(lv_obj_t* parent, const char* txt,
                               lv_color_t bg, lv_color_t fg,
                               lv_event_cb_t cb, void* ud, int dx)
    {
        lv_obj_t* b = lv_obj_create(parent);
        lv_obj_remove_style_all(b);
        lv_obj_set_size(b, 76, 76);
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(b, bg, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(b, ui::press(), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
        lv_obj_align(b, LV_ALIGN_BOTTOM_MID, dx, -18);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);

        lv_obj_t* l = ui_make_label(b, F_BODY, fg, txt);
        lv_obj_center(l);
        return b;
    }

    // -----------------------------------------------------------------
    static void cb_start(lv_event_t* e)
    {
        auto* s = (StopwatchApp*)lv_event_get_user_data(e);
        s->_running = !s->_running;
        if (s->_running) s->_t0 = lv_tick_get() - s->_elapsed;
        else             s->_elapsed = lv_tick_get() - s->_t0;

        // подсветка/название
        lv_obj_t* lbl = lv_obj_get_child(s->_b_start, 0);
        lv_label_set_text(lbl, s->_running ? "Stop" : "Start");
        lv_obj_set_style_bg_color(s->_b_start,
            s->_running ? ui::red() : ui::green(), 0);
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    }

    static void cb_reset(lv_event_t* e)
    {
        auto* s = (StopwatchApp*)lv_event_get_user_data(e);
        s->_running = false; s->_elapsed = 0; s->_lap_n = 0;
        lv_label_set_text(s->_lbl, "00:00.00");
        lv_label_set_text(s->_lap, "tap Lap to record");
        lv_obj_t* lbl = lv_obj_get_child(s->_b_start, 0);
        lv_label_set_text(lbl, "Start");
        lv_obj_set_style_bg_color(s->_b_start, ui::green(), 0);
    }

    static void cb_tick(lv_timer_t* t)
    {
        auto* s = (StopwatchApp*)lv_timer_get_user_data(t);
        uint32_t ms = s->_running ? (lv_tick_get() - s->_t0) : s->_elapsed;
        uint32_t m  = ms / 60000; ms %= 60000;
        uint32_t sec = ms / 1000; ms %= 1000;
        lv_label_set_text_fmt(s->_lbl, "%02u:%02u.%02u", m, sec, ms / 10);
    }
};

std::unique_ptr<App> app_stopwatch_make() {
    return std::make_unique<StopwatchApp>();
}