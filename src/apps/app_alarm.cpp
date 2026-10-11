#include "apps.h"
#include "ui/ui_common.h"
#include "core/Database.h"
#include "AppTask.h"

class AlarmApp final : public App {
public:
    const char* name() const override { return "Alarm"; }

    void onEnter(lv_obj_t* parent) override { _parent = parent; _build(); }
    void onExit() override {}

private:
    lv_obj_t* _parent{nullptr};

    struct RowCtx {
        int       idx;
        lv_obj_t* lbl;
    };

    void _build()
    {
        lv_obj_clean(_parent);
        ui_make_screen(_parent);
        ui_make_statusbar(_parent);

        lv_obj_t* list = lv_obj_create(_parent);
        lv_obj_set_size(list, LV_PCT(100), LV_PCT(100) - 24);
        lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(list, 0, 0);
        lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(list, 4, 0);
        lv_obj_set_style_pad_all(list, 6, 0);

        auto& cfg = Database::inst().config();
        for (int i = 0; i < 5; i++) {
            auto& a = cfg.alarms[i];

            lv_obj_t* row = lv_obj_create(list);
            lv_obj_set_size(row, LV_PCT(100), 44);
            lv_obj_set_style_bg_color(row, lv_color_hex(0x1F1F1F), 0);
            lv_obj_set_style_radius(row, 10, 0);
            lv_obj_set_style_border_width(row, 0, 0);
            lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                                  LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_hor(row, 8, 0);

            lv_obj_t* lbl = ui_make_label(row, &font_alibaba_24,
                                          lv_color_white(), "");
            lv_label_set_text_fmt(lbl, "%02d:%02d  %s",
                                  a.hour, a.minute, a.enabled ? "ON" : "off");

            lv_obj_t* btn = ui_make_btn(row, "+1h", nullptr, nullptr, 60, 32);

            auto* ctx = new RowCtx{ i, lbl };
            lv_obj_add_event_cb(btn, cb_add, LV_EVENT_CLICKED, ctx);
            lv_obj_add_event_cb(btn, [](lv_event_t* e){
                delete static_cast<RowCtx*>(lv_event_get_user_data(e));
            }, LV_EVENT_DELETE, ctx);
        }
    }

    static void cb_add(lv_event_t* e)
    {
        auto* ctx = static_cast<RowCtx*>(lv_event_get_user_data(e));
        auto& a = Database::inst().config().alarms[ctx->idx];
        a.hour = (a.hour + 1) % 24;
        if (a.hour == 0) a.enabled = !a.enabled;

        // Мгновенный отклик
        lv_label_set_text_fmt(ctx->lbl, "%02d:%02d  %s",
                              a.hour, a.minute, a.enabled ? "ON" : "off");

        // Сохранение — в фоне
        apptask_run_detached([]() { Database::inst().saveConfig(); });
    }
};

std::unique_ptr<App> app_alarm_make() {
    return std::make_unique<AlarmApp>();
}