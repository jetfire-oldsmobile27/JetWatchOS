#include "AlarmManager.h"
#include "Database.h"
#include "PowerManager.h"
#include "BleService.h"
#include "ui/ui_common.h"
#include "hal/hw.h"

static lv_obj_t* s_alarm_ov   = nullptr;
static lv_timer_t* s_vib_timer = nullptr;

AlarmManager& AlarmManager::inst() { static AlarmManager m; return m; }

void AlarmManager::begin()
{
    _last_minute = -1;
    _active      = false;
    _active_idx  = -1;

    struct tm t; hw_get_date_time(t);
    Serial.printf("[ALARM] begin: RTC=%04d-%02d-%02d %02d:%02d:%02d wday=%d\n",
                  t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                  t.tm_hour, t.tm_min, t.tm_sec, t.tm_wday);

    auto& cfg = Database::inst().config();
    for (int i = 0; i < 5; ++i) {
        const auto& a = cfg.alarms[i];
        Serial.printf("[ALARM] slot %d: en=%d %02d:%02d days=0x%02X\n",
                      i, (int)a.enabled, a.hour, a.minute, a.days_mask);
    }
}

void AlarmManager::tick()
{
    struct tm t; hw_get_date_time(t);
    if (t.tm_min == _last_minute) return;

    Serial.printf("[ALARM] minute change: prev_min=%d now=%02d:%02d (wday=%d)\n",
                  _last_minute, t.tm_hour, t.tm_min, t.tm_wday);
    _last_minute = t.tm_min;

    if (!_active) _check();
    else Serial.println("[ALARM] tick skipped: already active");
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
        if (a.days_mask && !(a.days_mask & wday_bit)) {
            Serial.printf("[ALARM] slot %d time-match but days_mask=0x%02X "
                          "& wday_bit=0x%02X = 0 -> skip\n",
                          i, a.days_mask, wday_bit);
            continue;
        }
        Serial.printf("[ALARM] MATCH slot %d at %02d:%02d\n",
                      i, a.hour, a.minute);
        _fire(i);
        return;
    }
    Serial.printf("[ALARM] check at %02d:%02d wday=%d -> no match\n",
                  t.tm_hour, t.tm_min, t.tm_wday);
}

void AlarmManager::_fire(int idx)
{
    if (_active) {
        Serial.printf("[ALARM] _fire(%d) ignored: already active\n", idx);
        return;
    }
    _active     = true;
    _active_idx = idx;

    bool was_off = PowerManager::inst().isScreenOff();
    Serial.printf("[ALARM] FIRE idx=%d, waking screen (was_off=%d)\n",
                  idx, (int)was_off);
    PowerManager::inst().wake();
    Serial.printf("[ALARM] screen now_off=%d\n",
                  (int)PowerManager::inst().isScreenOff());

    // ---------- Оверлей ----------
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

    lv_obj_t* tl = ui_make_label(s_alarm_ov, &font_alibaba_60,
                                 lv_color_white(), "--:--");
    auto& a = Database::inst().config().alarms[idx];
    lv_label_set_text_fmt(tl, "%02d:%02d", a.hour, a.minute);
    lv_obj_align(tl, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* sub = ui_make_label(s_alarm_ov, &font_alibaba_12,
                                  lv_color_hex(0xAAAAAA), "WAKE UP");
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 50);

    lv_obj_t* dismiss = ui_make_btn(s_alarm_ov, "DISMISS", nullptr,
                                    nullptr, 160, 50);
    lv_obj_align(dismiss, LV_ALIGN_BOTTOM_MID, 0, -25);
    lv_obj_add_event_cb(dismiss, [](lv_event_t*){
        AlarmManager::inst()._dismiss();
    }, LV_EVENT_CLICKED, nullptr);

    // ---------- Вибрация раз в 800 мс ----------
    s_vib_timer = lv_timer_create([](lv_timer_t*){
        hw_feedback();
    }, 800, nullptr);

    // ---------- Звук ----------
    // if (Database::inst().config().sounds_on) {
    //     apptask_run_detached([]() {
    //         hw_set_sd_music_play(AUDIO_SOURCE_FATFS, "alarm.mp3");
    //     });
    // }

    // ---------- BLE-событие ----------
    BleService::inst().eventAlarm((uint8_t)idx);

    Serial.printf("[ALARM] fired idx=%d, overlay+audio+notify done\n", idx);
}

void AlarmManager::_dismiss()
{
    Serial.printf("[ALARM] dismiss idx=%d\n", _active_idx);

    if (s_vib_timer) { lv_timer_del(s_vib_timer); s_vib_timer = nullptr; }
    if (s_alarm_ov)  { UI_OBJ_DEL(s_alarm_ov); s_alarm_ov = nullptr; }
    hw_set_play_stop();

    _active     = false;
    _active_idx = -1;
}