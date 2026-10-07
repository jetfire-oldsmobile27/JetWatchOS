#include "apps.h"
#include "ui/ui_common.h"
#include "core/PowerManager.h"


class MusicApp : public App {
public:
    const char* name() const override { return "Music"; }

    void onEnter(lv_obj_t* parent) override {
        ui_make_screen(parent);
        ui_make_statusbar(parent);

        lv_obj_t* cont = lv_obj_create(parent);
        lv_obj_set_size(cont, LV_PCT(90), 180);
        lv_obj_align(cont, LV_ALIGN_CENTER, 0, -20);
        lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(cont, 0, 0);
        lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        mk(cont, LV_SYMBOL_PREV, []{ hw_set_ble_key(MEDIA_PREVIOUS); });
        mk(cont, LV_SYMBOL_PLAY, []{ hw_set_ble_key(MEDIA_PLAY_PAUSE); });
        mk(cont, LV_SYMBOL_NEXT, []{ hw_set_ble_key(MEDIA_NEXT); });

        lv_obj_t* vol = lv_obj_create(parent);
        lv_obj_set_size(vol, LV_PCT(90), 60);
        lv_obj_align(vol, LV_ALIGN_BOTTOM_MID, 0, -20);
        lv_obj_set_style_bg_opa(vol, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(vol, 0, 0);
        lv_obj_set_flex_flow(vol, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(vol, LV_FLEX_ALIGN_SPACE_EVENLY,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        mk(vol, LV_SYMBOL_MINUS, []{ hw_set_ble_key(MEDIA_VOLUME_DOWN); }, 60, 50);
        mk(vol, LV_SYMBOL_PLUS,  []{ hw_set_ble_key(MEDIA_VOLUME_UP);   }, 60, 50);
    }

private:
    static void mk(lv_obj_t* p, const char* sym, void(*cb)(), int w=64, int h=64) {
        lv_obj_t* b = lv_btn_create(p);
        lv_obj_set_size(b, w, h);
        lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x2A2A2A), 0);
        lv_obj_add_event_cb(b, [](lv_event_t* e){
            auto fn = reinterpret_cast<void(*)()>(lv_event_get_user_data(e));
            fn(); PowerManager::inst().resetActivity();
        }, LV_EVENT_CLICKED, (void*)cb);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, sym);
        lv_obj_center(l);
    }
};

std::unique_ptr<App> app_music_make() {
    return std::make_unique<MusicApp>();
}