#include "apps.h"
#include "ui/ui_common.h"
#include "core/PowerManager.h"
#include "AppTask.h"
#include "hal/hw.h"
#include "core/BleService.h"

// ============================================================================
//  Marquee (бегущая строка)
// ============================================================================
// Скорость прокрутки, пикселей в секунду. Можно переопределить через
// platformio.ini:  build_flags = -DMARQUEE_SPEED_PX_PER_SEC=30
#ifndef MARQUEE_SPEED_PX_PER_SEC
#define MARQUEE_SPEED_PX_PER_SEC   40
#endif

// Пауза перед началом прокрутки (мс)
#ifndef MARQUEE_HOLD_MS
#define MARQUEE_HOLD_MS            1000
#endif

static void _marquee_exec(void* var, int32_t v) {
    lv_obj_set_style_translate_x((lv_obj_t*)var, v, 0);
}

// Создаёт «окно» фиксированного размера с клиппингом. Возвращает label —
// через него менять текст через _marquee_set_text.
static lv_obj_t* _marquee_create(lv_obj_t* parent,
                                  lv_coord_t width, lv_coord_t height,
                                  const lv_font_t* font, lv_color_t color)
{
    lv_obj_t* clip = lv_obj_create(parent);
    lv_obj_remove_style_all(clip);
    lv_obj_set_size(clip, width, height);
    lv_obj_clear_flag(clip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(clip, LV_OBJ_FLAG_CLICKABLE);
    // LV_OBJ_FLAG_OVERFLOW_VISIBLE не выставляем → дети клиппятся по границам

    lv_obj_t* lbl = lv_label_create(clip);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    return lbl;
}

// Установить текст. Если помещается — центрируется без анимации.
// Если не помещается — текст дублируется и запускается бегущая строка.
static void _marquee_set_text(lv_obj_t* lbl, const char* text)
{
    if (!lbl) return;
    lv_anim_del(lbl, _marquee_exec);
    lv_obj_set_style_translate_x(lbl, 0, 0);

    const lv_font_t* font = lv_obj_get_style_text_font(lbl, LV_PART_MAIN);
    lv_obj_t* clip = lv_obj_get_parent(lbl);
    lv_coord_t clip_w = lv_obj_get_width(clip);

    const char* t = text ? text : "";
    if (!t[0]) {
        lv_label_set_text(lbl, "");
        lv_obj_set_width(lbl, clip_w);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    // Измеряем одиночный текст
    lv_point_t sz;
    lv_txt_get_size(&sz, t, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_coord_t txt_w = sz.x;

    if (txt_w <= clip_w) {
        // Помещается — центрируем
        lv_obj_set_width(lbl, clip_w);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(lbl, t);
        return;
    }

    // Не помещается — дублируем с разделителем из 5 пробелов
    String doubled = String(t) + "     " + t;

    lv_point_t sz2;
    lv_txt_get_size(&sz2, doubled.c_str(), font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_coord_t full_w = sz2.x;
    lv_coord_t gap_w  = full_w - 2 * txt_w;

    lv_obj_set_width(lbl, full_w);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(lbl, doubled.c_str());

    // Прокручиваем ровно на одну копию (txt_w + gap_w). В конце вторая копия
    // оказывается там, где начиналась первая → сброс к 0 визуально бесшовный.
    int32_t distance = -(int32_t)(txt_w + gap_w);
    uint32_t dur_ms = (uint32_t)(-distance) * 1000UL / MARQUEE_SPEED_PX_PER_SEC;
    if (dur_ms < 800) dur_ms = 800;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, lbl);
    lv_anim_set_exec_cb(&a, _marquee_exec);
    lv_anim_set_values(&a, 0, distance);
    lv_anim_set_time(&a, dur_ms);
    lv_anim_set_delay(&a, MARQUEE_HOLD_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_start(&a);
}

// ============================================================================
//  Music app
// ============================================================================
struct MusicState {
    char title[64]  = {0};
    char artist[64] = {0};
    bool playing    = false;
    int  vol        = 0;
    int  vol_max    = 0;
    bool dirty      = true;
};
static MusicState s_state;
static uint32_t   s_last_toggle_ms = 0;

void app_music_set_state(const char* title, const char* artist,
                          bool playing, int vol, int vol_max)
{
    strlcpy(s_state.title,  title  ? title  : "", sizeof(s_state.title));
    strlcpy(s_state.artist, artist ? artist : "", sizeof(s_state.artist));

    // После оптимистичного тапа play/pause держим playing 3 с как есть,
    // иначе телефон пришлёт стейл-снапшот и иконка отскочит.
    if (millis() - s_last_toggle_ms > 3000) {
        s_state.playing = playing;
    }
    s_state.vol     = vol;
    s_state.vol_max = vol_max;
    s_state.dirty   = true;
}

class MusicApp final : public App {
public:
    const char* name() const override { return "Music"; }
    uint32_t tickPeriodMs() const override { return 500; }

    void onEnter(lv_obj_t* parent) override {
        ui_make_screen(parent);
        ui_make_statusbar(parent);

        // Title: y = 32, font_mont_16 (40% крупнее 12), ширина окна 216
        _title = _marquee_create(parent, 216, 22, F_MUSIC, lv_color_white());
        lv_obj_align(lv_obj_get_parent(_title), LV_ALIGN_TOP_MID, 0, 32);

        // Artist: y = 56, font_mont_16, серый, тоже marquee
        _artist = _marquee_create(parent, 216, 22, F_MUSIC, lv_color_hex(0xAAAAAA));
        lv_obj_align(lv_obj_get_parent(_artist), LV_ALIGN_TOP_MID, 0, 56);

        // Кнопки prev / play-pause / next — y = 90..178
        lv_obj_t* cont = lv_obj_create(parent);
        lv_obj_set_size(cont, LV_PCT(100), 88);
        lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 90);
        lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(cont, 0, 0);
        lv_obj_set_style_pad_all(cont, 0, 0);
        lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        mk(cont, LV_SYMBOL_PREV, MEDIA_PREVIOUS, 56, 56, 28);
        _b_play = mk(cont, LV_SYMBOL_PLAY, MEDIA_PLAY_PAUSE, 64, 64, 32);
        mk(cont, LV_SYMBOL_NEXT, MEDIA_NEXT, 56, 56, 28);

        // Громкость: [-], [NN%], [+]
        lv_obj_t* vol = lv_obj_create(parent);
        lv_obj_set_size(vol, LV_PCT(94), 38);
        lv_obj_align(vol, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_set_style_bg_opa(vol, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(vol, 0, 0);
        lv_obj_set_style_pad_all(vol, 0, 0);
        lv_obj_set_flex_flow(vol, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(vol, LV_FLEX_ALIGN_SPACE_EVENLY,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        mk(vol, LV_SYMBOL_MINUS, MEDIA_VOLUME_DOWN, 44, 38, 8);

        _vol_lbl = ui_make_label(vol, F_TINY, lv_color_hex(0xCCCCCC), "Vol -");
        lv_obj_set_width(_vol_lbl, 96);
        lv_obj_set_style_text_align(_vol_lbl, LV_TEXT_ALIGN_CENTER, 0);

        mk(vol, LV_SYMBOL_PLUS, MEDIA_VOLUME_UP, 44, 38, 8);

        s_state.dirty = true;
        _refresh();
    }

    void onExit() override {
        if (_title)  lv_anim_del(_title,  _marquee_exec);
        if (_artist) lv_anim_del(_artist, _marquee_exec);
        _title = _artist = _vol_lbl = _b_play = nullptr;
    }
    void onTick() override { _refresh(); }

private:
    lv_obj_t* _title   = nullptr;
    lv_obj_t* _artist  = nullptr;
    lv_obj_t* _vol_lbl = nullptr;
    lv_obj_t* _b_play  = nullptr;

    // Кэш последнего показанного текста — чтобы не перезапускать marquee
    // при изменении только громкости.
    char _cur_title[64]  = {0};
    char _cur_artist[64] = {0};
    int  _cur_vol_pct    = -1;
    bool _cur_playing    = false;

    void _refresh() {
        if (!s_state.dirty) return;
        s_state.dirty = false;

        // ---- Title ----
        const char* t = s_state.title[0] ? s_state.title : "-";
        if (_title && strcmp(_cur_title, t) != 0) {
            strlcpy(_cur_title, t, sizeof(_cur_title));
            _marquee_set_text(_title, t);
        }

        // ---- Artist ----
        const char* a = s_state.artist[0] ? s_state.artist : "";
        if (_artist && strcmp(_cur_artist, a) != 0) {
            strlcpy(_cur_artist, a, sizeof(_cur_artist));
            _marquee_set_text(_artist, a);
        }

        // ---- Volume ----
        int pct = (s_state.vol_max > 0)
                    ? (s_state.vol * 100 / s_state.vol_max)
                    : -1;
        if (_vol_lbl && pct != _cur_vol_pct) {
            _cur_vol_pct = pct;
            if (pct >= 0) lv_label_set_text_fmt(_vol_lbl, "%d%%", pct);
            else          lv_label_set_text(_vol_lbl, "Vol -");
        }

        // ---- Play/pause icon ----
        if (_b_play && s_state.playing != _cur_playing) {
            _cur_playing = s_state.playing;
            lv_obj_t* l = lv_obj_get_child(_b_play, 0);
            if (l) lv_label_set_text(l, _cur_playing
                                        ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
        }
    }

    static lv_obj_t* mk(lv_obj_t* p, const char* sym, media_key_value_t key,
                         int w, int h, int radius) {
        lv_obj_t* b = lv_btn_create(p);
        lv_obj_set_size(b, w, h);
        lv_obj_set_style_radius(b, radius, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x2A2A2A), 0);
        lv_obj_add_event_cb(b, [](lv_event_t* e){
            auto k = (media_key_value_t)(intptr_t)lv_event_get_user_data(e);
            PowerManager::inst().resetActivity();

            if (k == MEDIA_PLAY_PAUSE) {
                s_state.playing  = !s_state.playing;
                s_state.dirty    = true;
                s_last_toggle_ms = millis();

                lv_obj_t* btn = HW_EV_OBJ(e);
                lv_obj_t* lbl = btn ? lv_obj_get_child(btn, 0) : nullptr;
                if (lbl) lv_label_set_text(lbl, s_state.playing
                                                ? LV_SYMBOL_PAUSE
                                                : LV_SYMBOL_PLAY);
            }

            apptask_run_detached([k]() {
                switch (k) {
                    case MEDIA_NEXT:        BleService::inst().sendJson("{\"cmd\":\"media_next\"}");       break;
                    case MEDIA_PREVIOUS:    BleService::inst().sendJson("{\"cmd\":\"media_prev\"}");       break;
                    case MEDIA_PLAY_PAUSE:  BleService::inst().sendJson("{\"cmd\":\"media_play_pause\"}"); break;
                    case MEDIA_VOLUME_UP:   BleService::inst().sendJson("{\"cmd\":\"media_vol_up\"}");     break;
                    case MEDIA_VOLUME_DOWN: BleService::inst().sendJson("{\"cmd\":\"media_vol_down\"}");   break;
                }
            });
        }, LV_EVENT_CLICKED, (void*)(intptr_t)key);

        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, sym);
        lv_obj_center(l);
        return b;
    }
};

std::unique_ptr<App> app_music_make() {
    return std::make_unique<MusicApp>();
}