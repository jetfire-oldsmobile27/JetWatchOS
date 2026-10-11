#include "apps.h"
#include "ui/ui_common.h"
#include "core/Database.h"
#include "core/BleService.h"
#include "AppTask.h"
#include <time.h>

class TrainingApp final : public App {
public:
    const char* name() const override { return "Training"; }
    uint32_t tickPeriodMs() const override { return 1000; }

    void onEnter(lv_obj_t* parent) override
    {
        _tok = std::make_shared<AppToken>();

        ui_make_screen(parent);
        ui_make_statusbar(parent);

        _l_state = ui_make_label(parent, &font_alibaba_24,
                                 lv_color_white(), "Idle");
        lv_obj_align(_l_state, LV_ALIGN_TOP_MID, 0, 40);

        _l_time = ui_make_label(parent, &font_alibaba_60,
                                lv_color_white(), "00:00");
        lv_obj_align(_l_time, LV_ALIGN_CENTER, 0, -20);

        _l_steps = ui_make_label(parent, &font_alibaba_24,
                                 lv_color_hex(0xAAAAAA), "0 steps");
        lv_obj_align(_l_steps, LV_ALIGN_CENTER, 0, 40);

        _b = ui_make_btn(parent, "Start", cb_btn, this, 140, 54);
        lv_obj_align(_b, LV_ALIGN_BOTTOM_MID, 0, -20);
    }

    void onExit() override
    {
        // Если тренировка активна — финализируем. Токен обнуляем, но
        // работа (FFat write + BLE notify) доиграется в фоне.
        if (_running) _finalize(/*from_exit=*/true);
        if (_tok) _tok->alive = false;
    }

    void onTick() override
    {
        if (!_running) return;
        uint32_t el = (lv_tick_get() - _start_ms) / 1000;
        lv_label_set_text_fmt(_l_time, "%02u:%02u", el / 60, el % 60);
        uint32_t s = Database::inst().stepsToday() - _steps_at_start;
        lv_label_set_text_fmt(_l_steps, "%u steps", s);
    }

private:
    lv_obj_t* _l_state{};
    lv_obj_t* _l_time{};
    lv_obj_t* _l_steps{};
    lv_obj_t* _b{};
    std::shared_ptr<AppToken> _tok;

    bool     _running{false};
    uint32_t _start_ms{0};
    uint32_t _steps_at_start{0};
    uint32_t _id{0};

    struct Payload {
        uint32_t id, dur, steps, dist;
    };

    void _start()
    {
        _running        = true;
        _start_ms       = lv_tick_get();
        _steps_at_start = Database::inst().stepsToday();
        _id             = Database::inst().trainingStart("walk");

        lv_label_set_text(_l_state, "Running");
        lv_label_set_text(lv_obj_get_child(_b, 0), "Stop");

        // BLE notify — в фоне
        apptask_run_detached([]() {
            BleService::inst().eventTrainingStart("walk",
                (uint32_t)time(nullptr));
        });
    }

    void _finalize(bool from_exit)
    {
        if (!_running) return;
        Payload p{};
        p.id    = _id;
        p.dur   = (uint32_t)((lv_tick_get() - _start_ms) / 1000);
        p.steps = Database::inst().stepsToday() - _steps_at_start;
        p.dist  = (uint32_t)(p.steps * 0.75f);
        _running = false;

        if (!from_exit) {
            lv_label_set_text(_l_state, "Saving...");
            lv_label_set_text(lv_obj_get_child(_b, 0), "Start");
        }

        // Работа уходит в фон, UI обновим из deliver — но только если
        // приложение ещё живо (токен).
        apptask_run(_tok,
            [p]() -> bool {
                Database::inst().trainingStop(p.id, p.dur, p.steps, p.dist);
                BleService::inst().eventTrainingEnd(p.id, p.steps, p.dist, p.dur);
                return true;
            },
            [this, from_exit](bool) {
                if (from_exit) return;
                if (_l_state) lv_label_set_text(_l_state, "Saved");
            });
    }

    static void cb_btn(lv_event_t* e) {
        auto* s = static_cast<TrainingApp*>(lv_event_get_user_data(e));
        if (s->_running) s->_finalize(false); else s->_start();
    }
};

std::unique_ptr<App> app_training_make() {
    return std::make_unique<TrainingApp>();
}