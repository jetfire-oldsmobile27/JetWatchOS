#include "apps.h"
#include "ui/ui_common.h"
#include "core/Database.h"
#include "core/PowerManager.h"
#include "core/BleService.h"
#include "core/Protocol.h"
#include "core/GpsLog.h"
#include "AppTask.h"
#include <WiFi.h>
#include <functional>

#ifndef FIRMWARE_VERSION_STR
#define FIRMWARE_VERSION_STR "1.0.0-dev"
#endif

class SettingsApp final : public App {
public:
    const char* name() const override { return "Settings"; }

    void onEnter(lv_obj_t* parent) override {
        _tok = std::make_shared<AppToken>();
        _parent = parent;
        _build();
    }

    void onExit() override {
        if (_tok) _tok->alive = false;
        if (_info_timer) { lv_timer_del(_info_timer); _info_timer = nullptr; }
    }

private:
    lv_obj_t* _parent{nullptr};
    lv_obj_t* _list{nullptr};
    lv_obj_t* _kb{nullptr};
    lv_obj_t* _ta{nullptr};
    lv_obj_t* _slider_ov{nullptr};
    lv_timer_t* _info_timer{nullptr};
    std::shared_ptr<AppToken> _tok;

    int _step_val = 0, _step_mn = 0, _step_mx = 0, _step_step = 1;
    std::function<void(int)> _step_cb;
    lv_obj_t* _step_val_lbl{nullptr};
    bool _list_was_hidden{false};

    struct PwrLabels {
        lv_obj_t* charging{nullptr};
        lv_obj_t* discharge{nullptr};
        lv_obj_t* vbus_in{nullptr};
        lv_obj_t* chg_state{nullptr};
        lv_obj_t* batt_mv{nullptr};
        lv_obj_t* vbus_mv{nullptr};
        lv_obj_t* sys_mv{nullptr};
        lv_obj_t* batt_pct{nullptr};
        lv_obj_t* firmware{nullptr};
    } _pwr;

    enum Field { FIELD_NONE, FIELD_SSID, FIELD_PWD, FIELD_CITY };

    // -----------------------------------------------------------------
    //  Async-save helper — везде, где что-то пишется на FFat
    // -----------------------------------------------------------------
    static void _save_async() {
        apptask_run_detached([]() { Database::inst().saveConfig(); });
    }

    // -----------------------------------------------------------------
    //  Build
    // -----------------------------------------------------------------
    void _build() {
        if (_info_timer) { lv_timer_del(_info_timer); _info_timer = nullptr; }
        _pwr = PwrLabels{};
        _kb = nullptr;
        _ta = nullptr;
        _slider_ov = nullptr;

        lv_obj_clean(_parent);

        ui_make_screen(_parent);
        ui_make_statusbar(_parent);

        _list = lv_obj_create(_parent);
        lv_obj_set_size(_list, LV_PCT(100), LV_PCT(100) - 24);
        lv_obj_align(_list, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_opa(_list, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(_list, 0, 0);
        lv_obj_set_flex_flow(_list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(_list, 4, 0);
        lv_obj_set_style_pad_row(_list, 4, 0);
        lv_obj_set_scrollbar_mode(_list, LV_SCROLLBAR_MODE_OFF);

        auto& cfg = Database::inst().config();

        // ---------- Network ----------
        _row_switch(_list, "WiFi", cfg.wifi_on, [](bool v){
            auto& c = Database::inst().config();
            c.wifi_on = v;
            _save_async();
            if (v && c.wifi_ssid[0]) {
                // hw_set_wifi_connect + WiFi.begin блокируют
                std::string ssid = c.wifi_ssid;
                std::string pwd  = c.wifi_pwd;
                apptask_run_detached([ssid, pwd]() {
                    wifi_conn_params_t p;
                    p.ssid = ssid;
                    p.password = pwd;
                    hw_set_wifi_connect(p);
                });
            } else if (!v) {
                apptask_run_detached([]() { WiFi.disconnect(true); });
            }
        });

        _row_text(_list, "SSID",     cfg.wifi_ssid, FIELD_SSID);
        _row_text(_list, "Password", "******",      FIELD_PWD);
        _row_text(_list, "City",     cfg.city,      FIELD_CITY);

        // ---------- Connectivity ----------
        _row_switch(_list, "Bluetooth", cfg.bt_on, [](bool v){
            auto& c = Database::inst().config();
            c.bt_on = v;
            _save_async();
            if (v) {
                apptask_run_detached([]() {
                    BleService::inst().begin(proto::DEVICE_NAME);
                    PowerManager::inst().setBtActive(true);
                });
            } else {
                apptask_run_detached([]() {
                    BleService::inst().end();
                    PowerManager::inst().setBtActive(false);
                });
            }
        });

        _row_switch(_list, "Sounds", cfg.sounds_on, [](bool v){
            Database::inst().config().sounds_on = v;
            // `_save_async()` — статический метод, но внутри лямбды без capture this
            // просто вызываем напрямую через Database:
            apptask_run_detached([](){ Database::inst().saveConfig(); });
        });

        // ---------- Display ----------
        _row_value_stepper(_list, "Brightness", cfg.brightness, 0, 255, 5,
        [](int v){
            Database::inst().config().brightness = v;
            hw_set_disp_backlight(v);          // I²C ~200 µs — оставляем sync
            _save_async();
        });

        _row_value_stepper(_list, "Timeout", cfg.disp_timeout_s, 0, 180, 5,
        [](int v){
            Database::inst().config().disp_timeout_s = v;
            PowerManager::inst().setScreenTimeout(v * 1000UL);
            _save_async();
        });

        // ---------- GPS ----------
        _row_value_stepper(_list, "Timezone", cfg.tz_offset_min, -720, 840, 30,
        [](int v){
            Database::inst().config().tz_offset_min = v;
            _save_async();
        });

        _row_switch(_list, "GPS", cfg.gps_on, [](bool v){
            Database::inst().config().gps_on = v;
            _save_async();
        });

        _row_switch(_list, "Auto time sync", cfg.gps_time_sync, [](bool v){
            Database::inst().config().gps_time_sync = v;
            _save_async();
            if (!v) {
                extern bool gps_force_sync_once;
                gps_force_sync_once = false;
            }
        });

        _row_action(_list, "Sync time via GPS", ">", [](){
            auto& c = Database::inst().config();
            if (!c.gps_on) {
                ui_msg_pop_up("GPS", "GPS is disabled in Settings");
                return;
            }
            if (!(hw_get_device_online() & HW_GPS_ONLINE)) {
                ui_msg_pop_up("GPS", "No GPS hardware");
                return;
            }
            // hw_get_gps_info читает UART — до 1 с, не в LVGL-потоке
            apptask_run_detached([]() {
                gps_params_t p{};
                hw_get_gps_info(p);            // получаем фикс
                instance.powerControl(POWER_GPS, false);   // гасим LDO GPS
            });
            ui_msg_pop_up("GPS", "Time sync requested — wait for fix");
        });

        // ---------- Power info ----------
        _row_separator(_list, "POWER");
        _row_info(_list, "Charging",     &_pwr.charging);
        _row_info(_list, "Discharge",    &_pwr.discharge);
        _row_info(_list, "USB PlugIn",   &_pwr.vbus_in);
        _row_info(_list, "CHG state",    &_pwr.chg_state);
        _row_info(_list, "Batt Voltage", &_pwr.batt_mv);
        _row_info(_list, "USB Voltage",  &_pwr.vbus_mv);
        _row_info(_list, "SYS Voltage",  &_pwr.sys_mv);
        _row_info(_list, "Batt Percent", &_pwr.batt_pct);

        // ---------- Diagnostics ----------
        _row_separator(_list, "DIAGNOSTICS");
        _row_action(_list, "GPS Log", ">", [this](){ _open_gps_log(); });
        _row_separator(_list, "ABOUT");
        _row_info(_list, "Firmware",     &_pwr.firmware);

        // ---------- Keyboard ----------
        _kb = lv_keyboard_create(_parent);
        lv_obj_add_flag(_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(_kb, LV_PCT(100), LV_PCT(50));
        lv_obj_align(_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_clear_flag(_kb, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_move_foreground(_kb);

        lv_obj_add_event_cb(_kb, [](lv_event_t* e){
            auto* s = static_cast<SettingsApp*>(lv_event_get_user_data(e));
            if (s->_ta) {
                const char* txt = lv_textarea_get_text(s->_ta);
                auto& c = Database::inst().config();
                Field f = static_cast<Field>(
                    reinterpret_cast<intptr_t>(lv_obj_get_user_data(s->_ta)));
                if (f == FIELD_SSID) strlcpy(c.wifi_ssid, txt, sizeof(c.wifi_ssid));
                if (f == FIELD_PWD)  strlcpy(c.wifi_pwd,  txt, sizeof(c.wifi_pwd));
                if (f == FIELD_CITY) strlcpy(c.city,      txt, sizeof(c.city));
                _save_async();
            }
            lv_async_call([](void* p){
                static_cast<SettingsApp*>(p)->_build();
            }, s);
        }, LV_EVENT_READY, this);

        lv_obj_add_event_cb(_kb, [](lv_event_t* e){
            auto* s = static_cast<SettingsApp*>(lv_event_get_user_data(e));
            lv_async_call([](void* p){
                static_cast<SettingsApp*>(p)->_build();
            }, s);
        }, LV_EVENT_CANCEL, this);

        _refresh_power();
        _info_timer = lv_timer_create(_pwr_timer_cb, 2000, this);
    }

    // -----------------------------------------------------------------
    //  Power info
    // -----------------------------------------------------------------
    static void _pwr_timer_cb(lv_timer_t* t) {
        static_cast<SettingsApp*>(lv_timer_get_user_data(t))->_refresh_power();
    }

    void _refresh_power()
    {
        if (!_pwr.charging) return;
        if (_pwr.firmware) lv_label_set_text(_pwr.firmware, FIRMWARE_VERSION_STR);

#if defined(USING_PMU_MANAGE)
        static const char* chg_states[] = {
            "Tri", "Pre", "CC", "CV", "Done", "None"
        };
        bool    chg  = instance.pmu.isCharging();
        bool    dis  = instance.pmu.isDischarge();
        bool    vbus = instance.pmu.isVbusIn();
        uint8_t cs   = instance.pmu.getChargerStatus();

        lv_label_set_text(_pwr.charging, chg ? "YES" : "NO");
        lv_obj_set_style_text_color(_pwr.charging,
            chg ? lv_color_hex(0x4CAF50) : lv_color_white(), 0);
        lv_label_set_text(_pwr.discharge, dis  ? "YES" : "NO");
        lv_label_set_text(_pwr.vbus_in,   vbus ? "YES" : "NO");
        lv_label_set_text(_pwr.chg_state, (cs < 6) ? chg_states[cs] : "?");
        lv_label_set_text_fmt(_pwr.batt_mv,  "%u mV", instance.pmu.getBattVoltage());
        lv_label_set_text_fmt(_pwr.vbus_mv,  "%u mV", instance.pmu.getVbusVoltage());
        lv_label_set_text_fmt(_pwr.sys_mv,   "%u mV", instance.pmu.getSystemVoltage());
        lv_label_set_text_fmt(_pwr.batt_pct, "%d%%",  instance.pmu.getBatteryPercent());
#else
        monitor_params_t mp; hw_get_monitor_params(mp);
        lv_label_set_text(_pwr.charging,  "n/a");
        lv_label_set_text(_pwr.discharge, "n/a");
        lv_label_set_text(_pwr.vbus_in,   "n/a");
        lv_label_set_text(_pwr.chg_state, mp.charge_state.c_str());
        lv_label_set_text_fmt(_pwr.batt_mv,  "%u mV", mp.battery_voltage);
        lv_label_set_text_fmt(_pwr.vbus_mv,  "%u mV", mp.usb_voltage);
        lv_label_set_text_fmt(_pwr.sys_mv,   "%u mV", mp.sys_voltage);
        lv_label_set_text_fmt(_pwr.batt_pct, "%d%%",  mp.battery_percent);
#endif
    }

    // -----------------------------------------------------------------
    //  Row-builders (ниже — без изменений по логике, кроме _save_async)
    // -----------------------------------------------------------------
    void _row_separator(lv_obj_t* parent, const char* title) {
        lv_obj_t* row = lv_obj_create(parent);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(row, LV_PCT(100), 20);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_hor(row, 10, 0);
        lv_obj_set_style_pad_ver(row, 0, 0);
        ui_make_label(row, &font_alibaba_12, lv_color_hex(0x666666), title);
    }

    void _row_info(lv_obj_t* parent, const char* name, lv_obj_t** out) {
        lv_obj_t* row = lv_obj_create(parent);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(row, LV_PCT(100), 28);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x161616), 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_hor(row, 10, 0);
        lv_obj_set_style_pad_ver(row, 0, 0);
        ui_make_label(row, &font_alibaba_12, lv_color_hex(0xBBBBBB), name);
        lv_obj_t* val = ui_make_label(row, &font_alibaba_12, lv_color_white(), "--");
        lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);
        if (out) *out = val;
    }

    void _row_switch(lv_obj_t* parent, const char* name, bool val,
                     std::function<void(bool)> cb)
    {
        lv_obj_t* row = lv_obj_create(parent);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(row, LV_PCT(100), 40);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1F1F1F), 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_hor(row, 10, 0);
        ui_make_label(row, &font_alibaba_24, lv_color_white(), name);

        lv_obj_t* sw = lv_switch_create(row);
        lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
        if (val) lv_obj_add_state(sw, LV_STATE_CHECKED);

        auto* fn = new std::function<void(bool)>(cb);
        lv_obj_add_event_cb(sw, [](lv_event_t* e){
            auto* f = static_cast<std::function<void(bool)>*>(
                lv_event_get_user_data(e));
            lv_obj_t* s = HW_EV_OBJ(e);
            (*f)(lv_obj_has_state(s, LV_STATE_CHECKED));
        }, LV_EVENT_VALUE_CHANGED, fn);
        lv_obj_add_event_cb(sw, [](lv_event_t* e){
            delete static_cast<std::function<void(bool)>*>(
                lv_event_get_user_data(e));
        }, LV_EVENT_DELETE, fn);
    }

    void _row_text(lv_obj_t* parent, const char* name,
                   const char* val, Field field)
    {
        lv_obj_t* row = lv_obj_create(parent);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(row, LV_PCT(100), 44);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1F1F1F), 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_hor(row, 10, 0);
        lv_obj_set_user_data(row, (void*)(intptr_t)field);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* name_lbl = ui_make_label(row, &font_alibaba_24,
                                           lv_color_white(), name);
        lv_obj_set_width(name_lbl, LV_PCT(40));
        lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
        lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t* val_lbl = ui_make_label(row, &font_alibaba_24,
                                          lv_color_hex(0xAAAAAA), val);
        lv_obj_set_width(val_lbl, LV_PCT(48));
        lv_label_set_long_mode(val_lbl, LV_LABEL_LONG_DOT);
        lv_obj_align(val_lbl, LV_ALIGN_RIGHT_MID, -18, 0);
        lv_obj_set_style_text_align(val_lbl, LV_TEXT_ALIGN_RIGHT, 0);

        ui_make_label(row, &font_alibaba_24, lv_color_hex(0x666666), ">");

        lv_obj_add_event_cb(row, [](lv_event_t* e){
            auto* s = static_cast<SettingsApp*>(lv_event_get_user_data(e));
            if (s->_ta) return;
            lv_obj_t* row = HW_EV_OBJ(e);
            Field f = (Field)(intptr_t)lv_obj_get_user_data(row);

            s->_list_was_hidden = true;
            lv_obj_add_flag(s->_list, LV_OBJ_FLAG_HIDDEN);

            s->_ta = lv_textarea_create(s->_parent);
            lv_obj_set_size(s->_ta, LV_PCT(90), 44);
            lv_obj_align(s->_ta, LV_ALIGN_TOP_MID, 0, 36);
            lv_textarea_set_one_line(s->_ta, true);
            lv_obj_set_user_data(s->_ta, (void*)(intptr_t)f);

            auto& cfg = Database::inst().config();
            const char* cur =
                f == FIELD_SSID ? cfg.wifi_ssid :
                f == FIELD_PWD  ? cfg.wifi_pwd  :
                                  cfg.city;
            lv_textarea_set_text(s->_ta, cur);
            lv_obj_move_foreground(s->_ta);

            lv_group_t* g = lv_group_get_default();
            if (g) { lv_group_add_obj(g, s->_ta); lv_group_focus_obj(s->_ta); }

            lv_obj_clear_flag(s->_kb, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(s->_kb);
            lv_keyboard_set_textarea(s->_kb, s->_ta);
        }, LV_EVENT_CLICKED, this);
    }

    void _row_action(lv_obj_t* parent, const char* name, const char* chevron,
                     std::function<void()> cb)
    {
        lv_obj_t* row = lv_obj_create(parent);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(row, LV_PCT(100), 40);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1F1F1F), 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_hor(row, 10, 0);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        ui_make_label(row, &font_alibaba_24, lv_color_white(), name);

        lv_obj_t* chev = ui_make_label(row, &font_alibaba_24,
                                       lv_color_hex(0x666666), chevron);
        lv_obj_align(chev, LV_ALIGN_RIGHT_MID, 0, 0);

        auto* fn = new std::function<void()>(std::move(cb));
        lv_obj_add_event_cb(row, [](lv_event_t* e){
            auto* f = static_cast<std::function<void()>*>(lv_event_get_user_data(e));
            (*f)();
        }, LV_EVENT_CLICKED, fn);
        lv_obj_add_event_cb(row, [](lv_event_t* e){
            delete static_cast<std::function<void()>*>(lv_event_get_user_data(e));
        }, LV_EVENT_DELETE, fn);
    }

    void _row_value_stepper(lv_obj_t* parent, const char* name,
                            int val, int mn, int mx, int step,
                            std::function<void(int)> cb)
    {
        lv_obj_t* row = lv_obj_create(parent);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(row, LV_PCT(100), 44);
        lv_obj_set_style_radius(row, 8, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1F1F1F), 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_hor(row, 10, 0);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t* name_lbl = ui_make_label(row, &font_alibaba_24,
                                           lv_color_white(), name);
        lv_obj_set_width(name_lbl, LV_PCT(40));
        lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
        lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t* val_lbl = ui_make_label(row, &font_alibaba_24,
                                          lv_color_hex(0xAAAAAA), "");
        lv_label_set_text_fmt(val_lbl, "%d", val);
        lv_obj_set_width(val_lbl, LV_PCT(48));
        lv_label_set_long_mode(val_lbl, LV_LABEL_LONG_DOT);
        lv_obj_align(val_lbl, LV_ALIGN_RIGHT_MID, -18, 0);
        lv_obj_set_style_text_align(val_lbl, LV_TEXT_ALIGN_RIGHT, 0);

        ui_make_label(row, &font_alibaba_24, lv_color_hex(0x666666), ">");

        struct Ctx {
            SettingsApp* app;
            const char* title;
            int mn, mx, step, val;
            std::function<void(int)> cb;
        };
        auto* ctx = new Ctx{this, name, mn, mx, step, val, std::move(cb)};

        lv_obj_add_event_cb(row, [](lv_event_t* e){
            auto* c = static_cast<Ctx*>(lv_event_get_user_data(e));
            c->app->_open_stepper(c->title, c->mn, c->mx, c->val,
                                  c->step, c->cb);
        }, LV_EVENT_CLICKED, ctx);

        lv_obj_add_event_cb(row, [](lv_event_t* e){
            delete static_cast<Ctx*>(lv_event_get_user_data(e));
        }, LV_EVENT_DELETE, ctx);
    }

    // -----------------------------------------------------------------
    //  Stepper overlay
    // -----------------------------------------------------------------
    void _open_stepper(const char* title, int mn, int mx, int val, int step,
                       std::function<void(int)> cb)
    {
        if (_slider_ov) return;

        _step_val  = val;
        _step_mn   = mn;
        _step_mx   = mx;
        _step_step = step ? step : 1;
        _step_cb   = std::move(cb);

        lv_obj_add_flag(_list, LV_OBJ_FLAG_HIDDEN);
        if (_kb) lv_obj_add_flag(_kb, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t* ov = lv_obj_create(_parent);
        lv_obj_set_size(ov, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(ov, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(ov, 0, 0);
        lv_obj_set_style_radius(ov, 0, 0);
        lv_obj_set_style_pad_all(ov, 0, 0);
        lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_move_foreground(ov);
        _slider_ov = ov;

        lv_obj_t* t = ui_make_label(ov, &font_alibaba_24, lv_color_white(), title);
        lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 12);

        lv_obj_t* v = ui_make_label(ov, &font_alibaba_60, lv_color_white(), "");
        lv_label_set_text_fmt(v, "%d", _step_val);
        lv_obj_align(v, LV_ALIGN_CENTER, 0, -60);
        _step_val_lbl = v;

        auto mk = [&](const char* sym, lv_coord_t dx, lv_coord_t dy, int delta){
            lv_obj_t* b = lv_btn_create(ov);
            lv_obj_set_size(b, 90, 90);
            lv_obj_set_style_radius(b, 24, 0);
            lv_obj_set_style_bg_color(b, lv_color_hex(0x2A2A2A), 0);
            lv_obj_set_style_bg_color(b, lv_color_hex(0x3A3A3A), LV_STATE_PRESSED);
            lv_obj_set_style_shadow_width(b, 0, 0);
            lv_obj_align(b, LV_ALIGN_CENTER, dx, dy);
            lv_obj_add_event_cb(b, [](lv_event_t* e){
                auto* s = static_cast<SettingsApp*>(lv_event_get_user_data(e));
                int d = (int)(intptr_t)lv_obj_get_user_data(HW_EV_OBJ(e));
                s->_step_val += d;
                if (s->_step_val < s->_step_mn) s->_step_val = s->_step_mn;
                if (s->_step_val > s->_step_mx) s->_step_val = s->_step_mx;
                lv_label_set_text_fmt(s->_step_val_lbl, "%d", s->_step_val);
                if (s->_step_cb) s->_step_cb(s->_step_val);
            }, LV_EVENT_CLICKED, this);
            lv_obj_set_user_data(b, (void*)(intptr_t)delta);

            lv_obj_t* l = lv_label_create(b);
            lv_label_set_text(l, sym);
            lv_obj_set_style_text_font(l, &font_alibaba_60, 0);
            lv_obj_set_style_text_color(l, lv_color_white(), 0);
            lv_obj_center(l);
        };
        mk("-", -75, 25, -_step_step);
        mk("+",  75, 25,  _step_step);

        lv_obj_t* ok = ui_make_btn(ov, "OK", nullptr, nullptr, 180, 52);
        lv_obj_align(ok, LV_ALIGN_BOTTOM_MID, 0, -16);
        lv_obj_add_event_cb(ok, [](lv_event_t* e){
            auto* s = static_cast<SettingsApp*>(lv_event_get_user_data(e));
            UI_OBJ_DEL(s->_slider_ov);
            s->_slider_ov = nullptr;
            s->_step_val_lbl = nullptr;
            s->_step_cb = nullptr;
            lv_async_call([](void* p){
                static_cast<SettingsApp*>(p)->_build();
            }, s);
        }, LV_EVENT_CLICKED, this);
    }

    // -----------------------------------------------------------------
    //  GPS log overlay
    // -----------------------------------------------------------------
    void _open_gps_log()
    {
        if (_slider_ov) return;

        lv_obj_add_flag(_list, LV_OBJ_FLAG_HIDDEN);
        if (_kb) lv_obj_add_flag(_kb, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t* ov = lv_obj_create(_parent);
        lv_obj_set_size(ov, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(ov, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(ov, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(ov, 0, 0);
        lv_obj_set_style_radius(ov, 0, 0);
        lv_obj_set_style_pad_all(ov, 0, 0);
        lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(ov, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_move_foreground(ov);
        _slider_ov = ov;

        lv_obj_t* t = ui_make_label(ov, &font_alibaba_24,
                                    lv_color_white(), "GPS Log");
        lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 6);

        lv_obj_t* list = lv_obj_create(ov);
        lv_obj_set_size(list, LV_PCT(100), LV_PCT(100) - 76);
        lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 34);
        lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(list, 0, 0);
        lv_obj_set_style_pad_all(list, 4, 0);
        lv_obj_set_style_pad_row(list, 2, 0);
        lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

        if (gpslog::count() == 0) {
            const char* msg = (hw_get_device_online() & HW_GPS_ONLINE)
                            ? "no events yet"
                            : "No GPS hardware";
            ui_make_label(list, &font_alibaba_12, lv_color_hex(0x888888), msg);
        } else {
            for (int i = 0; i < gpslog::count(); ++i) {
                const auto& e = gpslog::at(i);
                uint32_t sec = e.ms / 1000;
                lv_obj_t* l = ui_make_label(list, &font_alibaba_12,
                                            lv_color_hex(0xCCCCCC), "");
                lv_label_set_text_fmt(l, "[%02u:%02u]  %s",
                                      (sec / 60) % 100, sec % 60, e.text);
                lv_obj_set_width(l, LV_PCT(100));
            }
        }

        lv_obj_t* ok = ui_make_btn(ov, "OK", nullptr, nullptr, 140, 42);
        lv_obj_align(ok, LV_ALIGN_BOTTOM_MID, 0, -12);
        lv_obj_move_foreground(ok);
        lv_obj_add_event_cb(ok, [](lv_event_t* e){
            auto* s = static_cast<SettingsApp*>(lv_event_get_user_data(e));
            UI_OBJ_DEL(s->_slider_ov);
            s->_slider_ov = nullptr;
            lv_async_call([](void* p){
                static_cast<SettingsApp*>(p)->_build();
            }, s);
        }, LV_EVENT_CLICKED, this);
    }
};

std::unique_ptr<App> app_settings_make() {
    return std::make_unique<SettingsApp>();
}