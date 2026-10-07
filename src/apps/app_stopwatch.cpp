#include "apps.h"
#include "ui/ui_common.h"
#include "core/PowerManager.h"

class StopwatchApp : public App {
public:
    const char* name() const override { return "Stopwatch"; }

    void onEnter(lv_obj_t* parent) override {
        ui_make_screen(parent);
        _lbl = ui_make_label(parent, &font_alibaba_60, lv_color_white(), "00:00.00");
        lv_obj_align(_lbl, LV_ALIGN_CENTER, 0, -20);

        _b_start = ui_make_btn(parent, "Start", cb_start, this, 120, 50);
        lv_obj_align(_b_start, LV_ALIGN_BOTTOM_MID, -70, -20);
        _b_reset = ui_make_btn(parent, "Reset", cb_reset, this, 120, 50);
        lv_obj_align(_b_reset, LV_ALIGN_BOTTOM_MID, 70, -20);

        _t0 = lv_tick_get(); _elapsed = 0; _running = false;
        _t = lv_timer_create(cb_tick, 30, this);
    }
    void onExit() override { if (_t) { lv_timer_del(_t); _t =nullptr; } }

private:
    lv_obj_t *_lbl, *_b_start, *_b_reset;
    lv_timer_t* _t  {nullptr};
    uint32_t _t0 = 0, _elapsed = 0;
    bool _running = false;

    static void cb_start(lv_event_t* e) {
        auto* s = (StopwatchApp*)lv_event_get_user_data(e);
        s->_running = !s->_running;
        if (s->_running) s->_t0 = lv_tick_get() - s->_elapsed;
        else             s->_elapsed = lv_tick_get() - s->_t0;
        lv_label_set_text(lv_obj_get_child(s->_b_start, 0),
                          s->_running ? "Stop" : "Start");
    }
    static void cb_reset(lv_event_t* e) {
        auto* s = (StopwatchApp*)lv_event_get_user_data(e);
        s->_running = false; s->_elapsed = 0;
        lv_label_set_text(s->_lbl, "00:00.00");
        lv_label_set_text(lv_obj_get_child(s->_b_start, 0), "Start");
    }
    static void cb_tick(lv_timer_t* t) {
        auto* s = (StopwatchApp*)lv_timer_get_user_data(t);
        uint32_t ms = s->_running ? (lv_tick_get() - s->_t0) : s->_elapsed;
        uint32_t m = ms / 60000; ms %= 60000;
        uint32_t sec = ms / 1000; ms %= 1000;
        lv_label_set_text_fmt(s->_lbl, "%02u:%02u.%02u", m, sec, ms/10);
    }
};

std::unique_ptr<App> app_stopwatch_make() {
    return std::make_unique<StopwatchApp>();
}