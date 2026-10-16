#include "apps.h"
#include "ui/ui_common.h"
#include "core/PowerManager.h"
#include "core/Database.h"
#include "hal/hw.h"
#include <vector>
#include <cstring>

struct Notification {
    char app[16];
    char title[32];
    char body[96];
    uint32_t ts;
};

static std::vector<Notification> s_list;
static lv_obj_t* s_popup  {nullptr};

void app_notifications_push(const char* app, const char* title, const char* body)
{
    Serial.printf("[NOTIF] push app='%s' title='%s' body='%s' screen_off=%d\n",
                  app ? app : "", title ? title : "", body ? body : "",
                  (int)PowerManager::inst().isScreenOff());

    Notification n{};
    strlcpy(n.app,   app,   sizeof(n.app));
    strlcpy(n.title, title, sizeof(n.title));
    strlcpy(n.body,  body,  sizeof(n.body));
    n.ts = (uint32_t)time(nullptr);
    s_list.insert(s_list.begin(), n);
    if (s_list.size() > 20) s_list.pop_back();

    // 1) Разбудить экран ДО вибрации и баннера — чтобы LVGL
    //    успел выйти из "слепого" режима к моменту отрисовки.
    if (PowerManager::inst().isScreenOff()) {
        PowerManager::inst().wake();
        Serial.println("[NOTIF] screen woke");
    }

    // 2) Вибрация
    hw_feedback();
    Serial.println("[NOTIF] vibrate done");

    // 3) Звук (если включён)
    if (Database::inst().config().sounds_on) {
        hw_set_sd_music_play(AUDIO_SOURCE_FATFS, "notify.mp3");
    }

    // 4) Баннер
    if (s_popup) { UI_OBJ_DEL(s_popup); s_popup = nullptr; }
    s_popup = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_popup);
    lv_obj_set_size(s_popup, LV_PCT(90), 60);
    lv_obj_align(s_popup, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_bg_color(s_popup, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_bg_opa(s_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_popup, 0, 0);
    lv_obj_set_style_radius(s_popup, 12, 0);
    lv_obj_set_style_pad_all(s_popup, 8, 0);
    lv_obj_move_foreground(s_popup);

    lv_obj_t* l = ui_make_label(s_popup, &font_alibaba_12, lv_color_white(), "");
    lv_label_set_text_fmt(l, "%s: %s\n%s", n.app, n.title, n.body);
    lv_obj_set_width(l, LV_PCT(95));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_center(l);

    lv_timer_t* t = lv_timer_create([](lv_timer_t* tt){
        if (s_popup) { UI_OBJ_DEL(s_popup); s_popup = nullptr; }
        lv_timer_del(tt);
    }, 4000, nullptr);
    lv_timer_set_repeat_count(t, 1);

    Serial.println("[NOTIF] banner created");
}

class NotificationsApp final : public App {
public:
    const char* name() const override { return "Notifications"; }

    void onEnter(lv_obj_t* parent) override {
        ui_make_screen(parent);
        ui_make_statusbar(parent);

        lv_obj_t* list = lv_obj_create(parent);
        lv_obj_set_size(list, LV_PCT(100), LV_PCT(100) - 24);
        lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(list, 0, 0);
        lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(list, 4, 0);
        lv_obj_set_style_pad_row(list, 4, 0);

        if (s_list.empty()) {
            ui_make_label(list, &font_alibaba_24, lv_color_hex(0x888888),
                          "No notifications");
            return;
        }
        for (auto& n : s_list) {
            lv_obj_t* card = lv_obj_create(list);
            lv_obj_set_size(card, LV_PCT(100), 64);
            lv_obj_set_style_radius(card, 10, 0);
            lv_obj_set_style_bg_color(card, lv_color_hex(0x1F1F1F), 0);
            lv_obj_set_style_border_width(card, 0, 0);
            lv_obj_set_style_pad_hor(card, 8, 0);

            lv_obj_t* l = ui_make_label(card, &font_alibaba_12,
                                        lv_color_white(), "");
            lv_label_set_text_fmt(l, "%s  %s\n%s", n.app, n.title, n.body);
            lv_obj_set_width(l, LV_PCT(100));
            lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
            lv_obj_align(l, LV_ALIGN_LEFT_MID, 0, 0);
        }
    }
};

std::unique_ptr<App> app_notifications_make() {
    return std::make_unique<NotificationsApp>();
}