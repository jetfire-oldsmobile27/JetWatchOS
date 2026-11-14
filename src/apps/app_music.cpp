#include "apps.h"
#include "ui/ui_common.h"
#include "core/PowerManager.h"
#include "AppTask.h"
#include "hal/hw.h"
#include "core/BleService.h"

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

    // Пока не истекло 3 с после оптимистичного тапа — не перетираем
    // playing. Иначе телефон пришлёт стейл-снапшот и иконка отскочит.
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

        // Title: y=34, font_alibaba_12 (в 2 раза меньше прежнего 24 px)
        _title = ui_make_label(parent, &font_alibaba_12, lv_color_white(), "-");
        lv_obj_set_width(_title, LV_PCT(94));
        lv_label_set_long_mode(_title, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(_title, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(_title, LV_ALIGN_TOP_MID, 0, 34);

        // Artist: y=58 (было 50, +15%), font_alibaba_12 — тот же, что и title,
        // но серый и строкой ниже. Перегенерённый с кириллицей.
        _artist = ui_make_label(parent, &font_alibaba_12,
                                lv_color_hex(0xAAAAAA), "");
        lv_obj_set_width(_artist, LV_PCT(94));
        lv_label_set_long_mode(_artist, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(_artist, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(_artist, LV_ALIGN_TOP_MID, 0, 58);

        // Кнопки prev/play/next
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

        _vol_lbl = ui_make_label(vol, &font_alibaba_12,
                                 lv_color_hex(0xCCCCCC), "Vol -");
        lv_obj_set_width(_vol_lbl, 96);
        lv_obj_set_style_text_align(_vol_lbl, LV_TEXT_ALIGN_CENTER, 0);

        mk(vol, LV_SYMBOL_PLUS, MEDIA_VOLUME_UP, 44, 38, 8);

        s_state.dirty = true;
        _refresh();
    }

    void onExit() override { _title = _artist = _vol_lbl = _b_play = nullptr; }
    void onTick() override { _refresh(); }

private:
    lv_obj_t* _title   = nullptr;
    lv_obj_t* _artist  = nullptr;
    lv_obj_t* _vol_lbl = nullptr;
    lv_obj_t* _b_play  = nullptr;

    void _refresh() {
        if (!s_state.dirty) return;
        s_state.dirty = false;

        if (_title)  lv_label_set_text(_title,  s_state.title[0]  ? s_state.title  : "-");
        if (_artist) lv_label_set_text(_artist, s_state.artist[0] ? s_state.artist : "");
        if (_vol_lbl) {
            if (s_state.vol_max > 0) {
                int pct = s_state.vol * 100 / s_state.vol_max;
                lv_label_set_text_fmt(_vol_lbl, "%d%%", pct);
            } else {
                lv_label_set_text(_vol_lbl, "Vol -");
            }
        }
        if (_b_play) {
            lv_obj_t* l = lv_obj_get_child(_b_play, 0);
            if (l) lv_label_set_text(l, s_state.playing
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

            // Оптимистичное переключение play/pause — меняем сразу, не
            // дожидаясь ответа телефона. s_last_toggle_ms блокирует
            // перезапись playing на 3 с.
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