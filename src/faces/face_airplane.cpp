#include "face_airplane.hpp"
#include "ui/ui_common.h"
#include "core/PowerManager.h"
#include "core/AppManager.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ---- Подгонка под реальную ориентацию BMA423 ----
// Реально на плате: +x к 12 часам, +y вправо, +z из экрана.
// После remap(): +x вправо, +y к 12 часам, +z из экрана.
#define AP_SIGN_PITCH  (1.0f)   
#define AP_SIGN_ROLL   (1.0f)  
// BMA423 на этих часах: +x смотрит к 12 часам, +y вправо, +z из экрана.
// Внутренняя система кода: +x вправо, +y к 12 часам, +z из экрана.
// => меняем x и y местами.
static inline void remap(float& x, float& y, float& z) {
    float t = x;
    x = y;
    y = t;
    (void)z;
}

namespace {

// ---------- Геометрия окна сетки ----------
constexpr int   WIN_X = 8,  WIN_Y = 32, WIN_W = 216, WIN_H = 170;
constexpr float CX = WIN_W / 2.0f, CY = WIN_H / 2.0f;
constexpr float PX_PER_DEG = 2.8f;
constexpr int   N_LAD = 7;                  // -30..+30 step 10, [3] = горизонт

// ---------- Адаптивная частота ----------
constexpr uint32_t P_FAST = 41;     // ~24 fps
constexpr uint32_t P_MID  = 83;     // ~12 fps
constexpr uint32_t P_LOW  = 166;    // ~6 fps
constexpr uint32_t P_IDLE = 200;    // опрос 5 Гц, перерисовка только при сдвиге
constexpr float    ACT_FAST = 0.12f, ACT_MID = 0.05f, ACT_LOW = 0.015f;  // g
constexpr int      ZERO_N = 5;

inline lv_color_t GLOW() { return lv_color_hex(0x22DD66); }
inline lv_color_t DIM()  { return lv_color_hex(0x0E5A22); }
inline lv_color_t AIR()  { return lv_color_hex(0xFFD24A); }

struct S {
    lv_obj_t*   win = nullptr;
    lv_obj_t*   lad[N_LAD] = {};
    lv_obj_t*   gbar = nullptr;
    lv_obj_t*   l_att = nullptr;
    lv_obj_t*   l_g   = nullptr;
    lv_obj_t*   l_rot = nullptr;
    lv_timer_t* tmr = nullptr;

    // состояние
    bool     was_off = false;
    int      zero_n = 0;
    float    zsum[3] = {0, 0, 0};
    float    g_lsb = 512.f;
    float    p0 = 0, r0 = 0;
    float    f[3] = {0, 0, 0};       // фильтрованный вектор (g)
    float    prev[3] = {0, 0, 0};
    bool     have_prev = false;
    uint32_t last_ms = 0, last_draw = 0, period = 100;
    float    act = 0;
    float    pr = 0, rr = 0;         // предыдущие относительные углы
    float    rate_p = 0, rate_r = 0; // °/s
    float    dp = 999, dr = 999;     // последние отрисованные
    char     c_att[32] = "", c_g[16] = "", c_rot[40] = "";
} s;

lv_point_precise_t lad_pts[N_LAD][2];
lv_point_precise_t wing_l[3], wing_r[3], stub[2];

lv_obj_t* new_line(lv_obj_t* parent, lv_color_t c, int w)
{
    lv_obj_t* l = lv_line_create(parent);
    lv_obj_set_style_line_width(l, w, 0);
    lv_obj_set_style_line_color(l, c, 0);
    lv_obj_set_style_line_rounded(l, true, 0);
    lv_obj_clear_flag(l, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    return l;
}

float wrap180(float a) { while (a > 180.f) a -= 360.f; while (a < -180.f) a += 360.f; return a; }

void set_if(lv_obj_t* l, char* cache, size_t n, const char* txt)
{
    if (strcmp(cache, txt) == 0) return;
    strlcpy(cache, txt, n);
    lv_label_set_text(l, txt);
}

bool read_accel(float& x, float& y, float& z)
{
#if defined(USING_BMA423_SENSOR)
    if (!(hw_get_device_online() & HW_BMA423_ONLINE)) return false;
    int16_t rx, ry, rz;
    if (!instance.sensor.getAccelerometer(rx, ry, rz)) return false;
    x = rx; y = ry; z = rz;
    remap(x, y, z);
    return true;
#else
    (void)x; (void)y; (void)z;
    return false;
#endif
}

void attitude(const float* v, float& pitch, float& roll)
{
    pitch = atan2f(v[1], sqrtf(v[0] * v[0] + v[2] * v[2])) * 57.29578f * AP_SIGN_PITCH;
    roll  = atan2f(-v[0], v[2]) * 57.29578f * AP_SIGN_ROLL;
}

void rezero()
{
    s.zero_n = 0;
    s.zsum[0] = s.zsum[1] = s.zsum[2] = 0;
    s.have_prev = false;
    s.last_ms = 0;
    s.act = 1.0f;                 // на время обнуления — быстрый режим
    s.dp = s.dr = 999;
}

void render(float pitch, float roll)
{
    float r  = roll * 0.0174533f;
    float sr = sinf(r), cr = cosf(r);
    float ux =  cr, uy = -sr;     // вдоль горизонта
    float nx = -sr, ny = -cr;     // "вверх" в повёрнутой системе

    for (int i = 0; i < N_LAD; ++i) {
        float off = ((i - 3) * 10.f - pitch) * PX_PER_DEG;
        if (fabsf(off) > 150.f) { lv_obj_add_flag(s.lad[i], LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(s.lad[i], LV_OBJ_FLAG_HIDDEN);

        float cx = CX + nx * off, cy = CY + ny * off;
        float half = (i == 3) ? 300.f : (abs(i - 3) == 2 ? 46.f : 30.f);
        lad_pts[i][0].x = (lv_value_precise_t)(cx - ux * half);
        lad_pts[i][0].y = (lv_value_precise_t)(cy - uy * half);
        lad_pts[i][1].x = (lv_value_precise_t)(cx + ux * half);
        lad_pts[i][1].y = (lv_value_precise_t)(cy + uy * half);
        lv_line_set_points(s.lad[i], lad_pts[i], 2);
    }
}

void tick(lv_timer_t* t)
{
    auto& pm = PowerManager::inst();
    uint32_t now = lv_tick_get();

    if (pm.isScreenOff()) {                    // экран спит: ничего не читаем
        s.was_off = true;
        if (s.period != 1000) { s.period = 1000; lv_timer_set_period(t, 1000); }
        return;
    }
    if (s.was_off) { s.was_off = false; rezero(); }   // ← точка отсчёта = момент включения

    if (!AppManager::inst().isLocked()) {      // открыто приложение/меню — не рисуем
        s.have_prev = false; s.last_ms = 0;
        if (s.period != 300) { s.period = 300; lv_timer_set_period(t, 300); }
        return;
    }

    float x, y, z;
    if (!read_accel(x, y, z)) {
        set_if(s.l_rot, s.c_rot, sizeof(s.c_rot), "NO IMU");
        if (s.period != 500) { s.period = 500; lv_timer_set_period(t, 500); }
        return;
    }

    // ---- Обнуление ----
    if (s.zero_n < ZERO_N) {
        s.zsum[0] += x; s.zsum[1] += y; s.zsum[2] += z;
        if (++s.zero_n == ZERO_N) {
            float ax = s.zsum[0] / ZERO_N, ay = s.zsum[1] / ZERO_N, az = s.zsum[2] / ZERO_N;
            float m = sqrtf(ax * ax + ay * ay + az * az);
            s.g_lsb = (m > 200.f && m < 2200.f) ? m : 512.f;   // 1 g в LSB
            float v[3] = { ax / s.g_lsb, ay / s.g_lsb, az / s.g_lsb };
            attitude(v, s.p0, s.r0);
            s.f[0] = v[0]; s.f[1] = v[1]; s.f[2] = v[2];
            s.prev[0] = v[0]; s.prev[1] = v[1]; s.prev[2] = v[2];
            s.have_prev = true; s.last_ms = now;
            s.pr = s.rr = 0; s.rate_p = s.rate_r = 0;
            s.act = 0;
            set_if(s.l_rot, s.c_rot, sizeof(s.c_rot), "ROT --");
            render(0, 0);
        } else {
            set_if(s.l_rot, s.c_rot, sizeof(s.c_rot), "ZEROING...");
        }
        if (s.period != P_FAST) { s.period = P_FAST; lv_timer_set_period(t, P_FAST); }
        return;
    }

    // ---- В g ----
    float v[3] = { x / s.g_lsb, y / s.g_lsb, z / s.g_lsb };
    float dt_ms = s.last_ms ? (float)(now - s.last_ms) : 0.f;
    s.last_ms = now;

    // ---- Активность -> частота ----
    float inst = 0;
    if (s.have_prev) {
        float dx = v[0] - s.prev[0], dy = v[1] - s.prev[1], dz = v[2] - s.prev[2];
        inst = sqrtf(dx * dx + dy * dy + dz * dz);
    }
    s.prev[0] = v[0]; s.prev[1] = v[1]; s.prev[2] = v[2]; s.have_prev = true;

    float decay = 1.f - dt_ms / 700.f; if (decay < 0) decay = 0;
    s.act = fmaxf(inst, s.act * decay);               // быстро вверх, медленно вниз

    uint32_t np = s.act > ACT_FAST ? P_FAST :
                  s.act > ACT_MID  ? P_MID  :
                  s.act > ACT_LOW  ? P_LOW  : P_IDLE;
    if (np != s.period) { s.period = np; lv_timer_set_period(t, np); }

    // ---- Фильтр гравитации ----
    float a = (np <= P_LOW) ? 0.35f : 0.12f;
    for (int i = 0; i < 3; ++i) s.f[i] += a * (v[i] - s.f[i]);

    float p, r; attitude(s.f, p, r);
    float pitch = wrap180(p - s.p0), roll = wrap180(r - s.r0);
    if (pitch >  90.f) pitch =  90.f;
    if (pitch < -90.f) pitch = -90.f;

    // ---- Скорости вращения (°/s) ----
    if (dt_ms > 0) {
        float k = 1000.f / dt_ms;
        s.rate_p += 0.3f * (wrap180(pitch - s.pr) * k - s.rate_p);
        s.rate_r += 0.3f * (wrap180(roll  - s.rr) * k - s.rate_r);
    }
    s.pr = pitch; s.rr = roll;

    float mag = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    float dyn = mag - 1.0f;                             // g

    // ---- Перерисовка ----
    bool moved = fabsf(pitch - s.dp) + fabsf(roll - s.dr) > 0.4f;
    if (!(moved || np <= P_LOW || now - s.last_draw > 1000)) return;
    s.last_draw = now; s.dp = pitch; s.dr = roll;

    render(pitch, roll);

    char buf[48];
    snprintf(buf, sizeof(buf), "P%+d\xC2\xB0 R%+d\xC2\xB0", (int)lroundf(pitch), (int)lroundf(roll));
    set_if(s.l_att, s.c_att, sizeof(s.c_att), buf);

    snprintf(buf, sizeof(buf), "%+.2fG", dyn);
    set_if(s.l_g, s.c_g, sizeof(s.c_g), buf);

    const char* rr_s = s.rate_r >  6.f ? ">>" : s.rate_r < -6.f ? "<<" : "--";
    const char* pp_s = s.rate_p >  6.f ? "^^" : s.rate_p < -6.f ? "vv" : "--";
    snprintf(buf, sizeof(buf), "ROLL %s %d  PIT %s %d", rr_s,
             (int)lroundf(fabsf(s.rate_r)), pp_s, (int)lroundf(fabsf(s.rate_p)));
    set_if(s.l_rot, s.c_rot, sizeof(s.c_rot), buf);

    int gb = (int)lroundf(dyn * 100.f);
    lv_bar_set_value(s.gbar, gb > 100 ? 100 : gb < -100 ? -100 : gb, LV_ANIM_OFF);
}

} // namespace

// =====================================================================
void face_airplane_build(lv_obj_t* parent)
{
    lv_obj_set_size(parent, 240, 240);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_set_style_radius(parent, 0, 0);
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    ui_make_statusbar(parent);        // иконка Caffeine рисуется там

    // Окно с клиппингом: линии горизонта длиннее экрана, наружу не вылезут
    s.win = lv_obj_create(parent);
    lv_obj_remove_style_all(s.win);
    lv_obj_set_size(s.win, WIN_W, WIN_H);
    lv_obj_set_pos(s.win, WIN_X, WIN_Y);
    lv_obj_set_style_border_width(s.win, 1, 0);
    lv_obj_set_style_border_color(s.win, DIM(), 0);
    lv_obj_set_style_border_opa(s.win, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s.win, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));

    for (int i = 0; i < N_LAD; ++i)
        s.lad[i] = new_line(s.win, i == 3 ? GLOW() : DIM(), i == 3 ? 3 : 2);

    // Неподвижный силуэт самолёта
    wing_l[0] = { (lv_value_precise_t)(CX - 50), (lv_value_precise_t)CY };
    wing_l[1] = { (lv_value_precise_t)(CX - 18), (lv_value_precise_t)CY };
    wing_l[2] = { (lv_value_precise_t)(CX - 18), (lv_value_precise_t)(CY + 8) };
    wing_r[0] = { (lv_value_precise_t)(CX + 50), (lv_value_precise_t)CY };
    wing_r[1] = { (lv_value_precise_t)(CX + 18), (lv_value_precise_t)CY };
    wing_r[2] = { (lv_value_precise_t)(CX + 18), (lv_value_precise_t)(CY + 8) };
    stub[0]   = { (lv_value_precise_t)CX, (lv_value_precise_t)(CY - 4) };
    stub[1]   = { (lv_value_precise_t)CX, (lv_value_precise_t)(CY - 16) };
    lv_line_set_points(new_line(s.win, AIR(), 3), wing_l, 3);
    lv_line_set_points(new_line(s.win, AIR(), 3), wing_r, 3);
    lv_line_set_points(new_line(s.win, AIR(), 3), stub, 2);

    // G-шкала справа (симметричная от нуля)
    s.gbar = lv_bar_create(parent);
    lv_obj_set_size(s.gbar, 5, 120);
    lv_obj_set_pos(s.gbar, 229, 56);
    lv_bar_set_mode(s.gbar, LV_BAR_MODE_SYMMETRICAL);
    lv_bar_set_range(s.gbar, -100, 100);
    lv_bar_set_value(s.gbar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s.gbar, DIM(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s.gbar, GLOW(), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s.gbar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(s.gbar, 2, LV_PART_INDICATOR);

    s.l_att = ui_make_label(parent, F_TINY, GLOW(), "P+0\xC2\xB0 R+0\xC2\xB0");
    lv_obj_set_pos(s.l_att, 16, 206);
    s.l_g = ui_make_label(parent, F_TINY, GLOW(), "+0.00G");
    lv_obj_align(s.l_g, LV_ALIGN_TOP_RIGHT, -16, 206);
    s.l_rot = ui_make_label(parent, F_TINY, DIM(), "ROT --");
    lv_obj_set_pos(s.l_rot, 16, 221);
    lv_obj_set_style_text_color(s.l_rot, GLOW(), 0);
}

void face_airplane_show()
{
    rezero();
    s.was_off = false;
    s.period = 100;
    if (!s.tmr) s.tmr = lv_timer_create(tick, 100, nullptr);
}

void face_airplane_hide()
{
    if (s.tmr) { lv_timer_del(s.tmr); s.tmr = nullptr; }
}