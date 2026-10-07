#include "apps.h"
#include "ui/ui_common.h"
#include "core/Database.h"
#include "core/BleService.h"

//LV_IMG_DECLARE(img_walkie);

class PedometerApp : public App {
public:
    const char* name() const override { return "Steps"; }
    uint32_t tickPeriodMs() const override { return 1000; }

    void onEnter(lv_obj_t* parent) override {
        ui_make_screen(parent);
        ui_make_statusbar(parent);

        auto total = ui_make_label(parent, &font_alibaba_60, lv_color_white(), "0");
        lv_obj_align(total, LV_ALIGN_CENTER, 0, -30);
        _steps = total;

        auto goal = ui_make_label(parent, &font_alibaba_24,
                                  lv_color_hex(0xAAAAAA), "0 / 10000");
        lv_obj_align(goal, LV_ALIGN_CENTER, 0, 40);
        _goal = goal;

        _bar = lv_bar_create(parent);
        lv_obj_set_size(_bar, LV_PCT(80), 10);
        lv_obj_align(_bar, LV_ALIGN_CENTER, 0, 80);
        lv_bar_set_range(_bar, 0, 10000);

        _refresh();
    }

    void onTick() override { _refresh(); }

private:
    lv_obj_t *_steps, *_goal, *_bar;

    void _refresh() {
        uint32_t s = Database::inst().stepsToday();
        lv_label_set_text_fmt(_steps, "%u", s);
        lv_label_set_text_fmt(_goal, "%u / 10000", s);
        lv_bar_set_value(_bar, s > 10000 ? 10000 : s, LV_ANIM_OFF);
    }
};

std::unique_ptr<App> app_pedometer_make() {
    return std::make_unique<PedometerApp>();
}