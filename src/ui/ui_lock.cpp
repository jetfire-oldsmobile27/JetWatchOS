#include "ui_common.h"
#include "core/PowerManager.h"

static lv_obj_t *l_h, *l_m, *l_c, *l_d;
#if defined(USING_BMA423_SENSOR)
static lv_obj_t *l_temp;
#endif

static void tick(lv_timer_t*)
{
    struct tm t; hw_get_date_time(t);
    lv_label_set_text_fmt(l_h, "%02d", t.tm_hour);
    lv_label_set_text_fmt(l_m, "%02d", t.tm_min);

    const char* wd[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    uint8_t wi = t.tm_wday > 6 ? 6 : t.tm_wday;
    lv_label_set_text_fmt(l_d, "%02d-%02d %s",
                          t.tm_mon + 1, t.tm_mday, wd[wi]);

    static bool colon_on = false;
    colon_on = !colon_on;
    lv_obj_set_style_text_opa(l_c,
        colon_on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
}

#if defined(USING_BMA423_SENSOR)
static void temp_tick(lv_timer_t*)
{
    if (!(hw_get_device_online() & HW_BMA423_ONLINE)) {
        lv_label_set_text(l_temp, "--.-°C");
        return;
    }
    float c = instance.sensor.getTemperature(SensorBMA423::TEMP_DEG);
    // датчик температуры на BMA423 — «грубый», ±1..2°C и сильно
    // уходит в самонагрев. Имеет смысл показывать целые градусы.
    lv_label_set_text_fmt(l_temp, "%.0f°C", c);
}
#endif

lv_obj_t* ui_build_lock(lv_obj_t* parent)
{
    ui_make_screen(parent);
    ui_make_statusbar(parent);

    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, -30);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    l_h = ui_make_label(row, &font_alibaba_60, lv_color_white(), "00");
    l_c = ui_make_label(row, &font_alibaba_60, lv_color_white(), ":");
    l_m = ui_make_label(row, &font_alibaba_60, lv_color_white(), "00");

    l_d = ui_make_label(parent, &font_alibaba_24, lv_color_hex(0xAAAAAA), "--");
    lv_obj_align(l_d, LV_ALIGN_CENTER, 0, 70);

#if defined(USING_BMA423_SENSOR)
    l_temp = ui_make_label(parent, &font_alibaba_24,
                           lv_color_hex(0xAAAAAA), "--.-°C");
    lv_obj_align(l_temp, LV_ALIGN_CENTER, 0, 110);
    lv_timer_create(temp_tick, 5000, nullptr);   // 5 c достаточно
    temp_tick(nullptr);
#endif

    lv_timer_create(tick, 1000, nullptr);
    tick(nullptr);
    return parent;
}