#include "AlarmManager.h"
#include "Database.h"
#include "PowerManager.h"
#include "BleService.h"
#include "ui/ui_common.h"
#include "hal/hw.h"

static lv_obj_t* s_alarm_ov = nullptr;
static lv_timer_t* s_vib_timer = nullptr;

AlarmManager& AlarmManager::inst() { static AlarmManager m; return m; }

void AlarmManager::begin()
{
    _last_minute = -1;
    _active = false;
}

void AlarmManager::tick()
{
    struct tm t; hw_get_date_time(t);
    if (t.tm_min == _last_minute) return;
    _last_minute = t.tm_min;
    if (!_active) _check();
}

void AlarmManager::_check()
{
    struct tm t; hw_get_date_time(t);
    auto& cfg = Database::inst().config();

    uint8_t wday_bit = 1 << ((t.tm_wday + 6) % 7);   // Mon = bit0

    for (int i = 0; i < 5; ++i) {
        const auto& a = cfg.alarms[i];
        if (!a.enabled) continue;
        if (a.hour   != t.tm_hour) continue;
        if (a.minute != t.tm_min)  continue;
        if (a.days_mask && !(a.days_mask & wday_bit)) continue;
        _fire(i);
        return;
    }
}

void AlarmManager::_fire(int idx)
{
    if (_active) return;
    _active = true;
    _active_idx = idx;

    PowerManager::inst().wake();

    // Оверлей
    s_alarm_ov = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_alarm_ov);
    lv_obj_set_size(s_alarm_ov, LV_PCT(100), LV_PCT(100));
    lv_obj_center(s_alarm_ov);
    lv_obj_set_style_bg_color(s_alarm_ov, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_alarm_ov, LV_OPA_90, 0);
    lv_obj_clear_flag(s_alarm_ov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(s_alarm_ov);

    lv_obj_t* icon = lv_label_create(s_alarm_ov);
    lv_label_set_text(icon, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xFF7A1A), 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -60);

    lv_obj_t* t = ui_make_label(s_alarm_ov, &font_alibaba_60,
                                lv_color_white(), "--:--");
    auto& a = Database::inst().config().alarms[idx];
    lv_label_set_text_fmt(t, "%02d:%02d", a.hour, a.minute);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* sub = ui_make_label(s_alarm_ov, &font_alibaba_12,
                                  lv_color_hex(0xAAAAAA), "WAKE UP");
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 50);

    lv_obj_t* dismiss = ui_make_btn(s_alarm_ov, "DISMISS", nullptr,
                                    nullptr, 160, 50);
    lv_obj_align(dismiss, LV_ALIGN_BOTTOM_MID, 0, -25);
    lv_obj_add_event_cb(dismiss, [](lv_event_t*){
        AlarmManager::inst()._dismiss();
    }, LV_EVENT_CLICKED, nullptr);

    // Вибрация раз в 800 мс
    s_vib_timer = lv_timer_create([](lv_timer_t*){
        hw_feedback();
    }, 800, nullptr);

    // Звук (если включён)
    if (Database::inst().config().sounds_on) {
        // Своих звуков пока нет — используем keyboard_audio как гудок.
        hw_set_sd_music_play(AUDIO_SOURCE_FATFS, "alarm.mp3");
        // когда добавишь — положишь /alarm.mp3 на FFat
    }

    // BLE-событие
    BleService::inst().eventAlarm((uint8_t)idx);
}

void AlarmManager::_dismiss()
{
    if (s_vib_timer) { lv_timer_del(s_vib_timer); s_vib_timer = nullptr; }
    if (s_alarm_ov)  { UI_OBJ_DEL(s_alarm_ov); s_alarm_ov = nullptr; }
    hw_set_play_stop();
    _active = false;
    _active_idx = -1;
}