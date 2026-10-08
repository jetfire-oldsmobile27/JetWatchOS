#include "apps.h"
#include "ui/ui_common.h"
#include "core/PowerManager.h"
#include "core/Database.h"

class FlashlightApp final : public App {
public:
    const char* name() const override { return "Flashlight"; }

    void onEnter(lv_obj_t* parent) override {
        lv_obj_set_style_bg_color(parent, lv_color_white(), 0);
        hw_set_disp_backlight(hw_get_disp_max_brightness());

        lv_obj_t* b = ui_make_btn(parent, "STOP", cb_off, this, 140, 60);
        lv_obj_align(b, LV_ALIGN_BOTTOM_MID, 0, -30);
    }

    void onExit() override {
        hw_set_disp_backlight(Database::inst().config().brightness);
    }

private:
    static void cb_off(lv_event_t*) { AppManager::inst().back(); }
};

std::unique_ptr<App> app_flashlight_make() {
    return std::make_unique<FlashlightApp>();
}