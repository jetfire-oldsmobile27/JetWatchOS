#include "face_default.hpp"
#include "ui/ui_common.h"
#include "hal/hw.h"
#include "core/Database.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
//  Геометрия
// ============================================================
static constexpr int CX        = 175;
static constexpr int CY        = 122;
static constexpr int R_ARC     = 58;
static constexpr int R_TICK_O  = 56;
static constexpr int R_TICK_I  = 50;
static constexpr int R_NEEDLE  = 44;
static constexpr int STEP_GOAL = 10000;

// Правый полукруг — батарея:  0% вверху-справа, 100% внизу-справа
static constexpr float BATT_DEG_A = 282.0f;   // up-right
static constexpr float BATT_DEG_B = 438.0f;   // = 78 + 360 (down-right)

// Левый полукруг — шаги:  0% внизу-слева, 100% вверху-слева
static constexpr float STEP_DEG_A = 102.0f;   // down-left
static constexpr float STEP_DEG_B = 258.0f;   // up-left

// ============================================================
static lv_obj_t *l_h, *l_m, *l_s;
static lv_obj_t *l_date, *l_temp;
static lv_obj_t *line_batt, *line_steps;

static lv_point_precise_t pt_batt[2];
static lv_point_precise_t pt_steps[2];

static constexpr int ARC_N = 24;
static lv_point_precise_t arc_batt [ARC_N];
static lv_point_precise_t arc_steps[ARC_N];

static constexpr int TICK_N = 5;
static lv_point_precise_t tick_batt [TICK_N][2];
static lv_point_precise_t tick_steps[TICK_N][2];

// ============================================================
static void polar(int r, float deg,
                  lv_value_precise_t* x, lv_value_precise_t* y)
{
    float a = deg * (float)M_PI / 180.0f;
    *x = (lv_value_precise_t)(CX + r * cosf(a));
    *y = (lv_value_precise_t)(CY + r * sinf(a));
}

static lv_obj_t* new_line(lv_obj_t* parent, lv_color_t color, int w)
{
    lv_obj_t* l = lv_line_create(parent);
    //lv_obj_set_size(parent, 240, 240);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_line_width(l, w, 0);
    lv_obj_set_style_line_color(l, color, 0);
    lv_obj_set_style_line_rounded(l, true, 0);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_SCROLLABLE);
    return l;
}

static void make_arc(lv_obj_t* parent, float deg_a, float deg_b,
                     lv_point_precise_t* pts, lv_color_t color)
{
    for (int i = 0; i < ARC_N; ++i) {
        float t = i / (float)(ARC_N - 1);
        polar(R_ARC, deg_a + t * (deg_b - deg_a), &pts[i].x, &pts[i].y);
    }
    lv_obj_t* l = new_line(parent, color, 3);
    lv_line_set_points(l, pts, ARC_N);
}

// ============================================================
void face_default_build(lv_obj_t* parent)
{
    lv_obj_set_size(parent, 240, 240);
    // Родитель — жёсткий 240x240 канвас без скролла, паддингов, бордера.
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_set_style_radius(parent, 0, 0);
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    ui_make_statusbar(parent);

    // ---------- Время слева столбиком ----------
    l_h = ui_make_label(parent, &font_alibaba_40, lv_color_white(), "00");
    lv_obj_set_pos(l_h, 14, 38);

    l_m = ui_make_label(parent, &font_alibaba_40, lv_color_white(), "00");
    lv_obj_set_pos(l_m, 14, 94);

    l_s = ui_make_label(parent, &font_alibaba_40, lv_color_hex(0xFF7A1A), "00");
    lv_obj_set_pos(l_s, 14, 150);

    l_date = ui_make_label(parent, &font_alibaba_12, lv_color_hex(0x888888), "--");
    lv_obj_set_pos(l_date, 16, 206);

#if defined(USING_BMA423_SENSOR)
    l_temp = ui_make_label(parent, &font_alibaba_12, lv_color_hex(0x666666), "--");
    lv_obj_set_pos(l_temp, 16, 222);

    lv_obj_t* l_temp_u = ui_make_label(parent, &lv_font_montserrat_12,
                                       lv_color_hex(0x666666), " °C");
    lv_obj_align_to(l_temp_u, l_temp, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
#endif

    // ---------- Два полукруга (границы) ----------
    make_arc(parent, BATT_DEG_A, BATT_DEG_B, arc_batt,  lv_color_hex(0x3A9EFF));
    make_arc(parent, STEP_DEG_A, STEP_DEG_B, arc_steps, lv_color_hex(0xFF7A1A));

    // ---------- 5 тиков на каждой дуге ----------
    for (int i = 0; i < TICK_N; ++i) {
        float t = i / (float)(TICK_N - 1);

        float db = BATT_DEG_A + t * (BATT_DEG_B - BATT_DEG_A);
        polar(R_TICK_O, db, &tick_batt[i][0].x, &tick_batt[i][0].y);
        polar(R_TICK_I, db, &tick_batt[i][1].x, &tick_batt[i][1].y);
        lv_obj_t* tb = new_line(parent, lv_color_hex(0x777777),
                                (i == 2) ? 3 : 2);
        lv_line_set_points(tb, tick_batt[i], 2);

        float ds = STEP_DEG_A + t * (STEP_DEG_B - STEP_DEG_A);
        polar(R_TICK_O, ds, &tick_steps[i][0].x, &tick_steps[i][0].y);
        polar(R_TICK_I, ds, &tick_steps[i][1].x, &tick_steps[i][1].y);
        lv_obj_t* ts = new_line(parent, lv_color_hex(0x777777),
                                (i == 2) ? 3 : 2);
        lv_line_set_points(ts, tick_steps[i], 2);
    }

    // ---------- Стрелки ----------
    line_batt  = new_line(parent, lv_color_hex(0x3A9EFF), 3);
    line_steps = new_line(parent, lv_color_hex(0xFF7A1A), 3);

    // Начальное положение — 0%
    polar(R_NEEDLE, BATT_DEG_A, &pt_batt[1].x,  &pt_batt[1].y);
    pt_batt[0].x = (lv_value_precise_t)CX;
    pt_batt[0].y = (lv_value_precise_t)CY;
    lv_line_set_points(line_batt, pt_batt, 2);

    polar(R_NEEDLE, STEP_DEG_A, &pt_steps[1].x, &pt_steps[1].y);
    pt_steps[0].x = (lv_value_precise_t)CX;
    pt_steps[0].y = (lv_value_precise_t)CY;
    lv_line_set_points(line_steps, pt_steps, 2);

    // ---------- Хаб ----------
    lv_obj_t* hub = lv_obj_create(parent);
    lv_obj_remove_style_all(hub);
    lv_obj_set_size(hub, 12, 12);
    lv_obj_set_pos(hub, CX - 6, CY - 6);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(hub, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
    lv_obj_clear_flag(hub, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(hub, LV_OBJ_FLAG_SCROLLABLE);
}

// ============================================================
static void set_needle(lv_obj_t* line, lv_point_precise_t pts[2], float deg)
{
    polar(R_NEEDLE, deg, &pts[1].x, &pts[1].y);
    pts[0].x = (lv_value_precise_t)CX;
    pts[0].y = (lv_value_precise_t)CY;
    lv_line_set_points(line, pts, 2);
}

void face_default_update()
{
    if (!l_h || !l_m || !l_s || !line_batt || !line_steps) return;

    struct tm t; hw_get_date_time(t);
    lv_label_set_text_fmt(l_h, "%02d", t.tm_hour);
    lv_label_set_text_fmt(l_m, "%02d", t.tm_min);
    lv_label_set_text_fmt(l_s, "%02d", t.tm_sec);

    const char* wd[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    uint8_t wi = t.tm_wday > 6 ? 6 : t.tm_wday;
    lv_label_set_text_fmt(l_date, "%s  %02d.%02d",
                          wd[wi], t.tm_mday, t.tm_mon + 1);

    // Battery
    monitor_params_t mp;
    hw_get_monitor_params(mp);
    int batt = mp.battery_percent;
    if (batt < 0) batt = 0;
    if (batt > 100) batt = 100;
    float tb = batt / 100.0f;
    set_needle(line_batt, pt_batt, BATT_DEG_A + tb * (BATT_DEG_B - BATT_DEG_A));

    // Steps
    uint32_t steps = Database::inst().stepsToday();
    if (steps > (uint32_t)STEP_GOAL) steps = STEP_GOAL;
    float ts = (float)steps / (float)STEP_GOAL;
    set_needle(line_steps, pt_steps, STEP_DEG_A + ts * (STEP_DEG_B - STEP_DEG_A));

#if defined(USING_BMA423_SENSOR)
    if (l_temp) {
        if (!(hw_get_device_online() & HW_BMA423_ONLINE)) {
            lv_label_set_text(l_temp, "--");
        } else {
            float c = instance.sensor.getTemperature(SensorBMA423::TEMP_DEG);
            lv_label_set_text_fmt(l_temp, "%.0f", c);   // ← без " °C"
        }
    }
#endif
}