// src/apps/app_weather.cpp
#include "apps.h"
#include "ui/ui_common.h"
#include "core/Database.h"
#include "AppTask.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

struct WeatherResult {
    bool  ok = false;
    char  err[40] = {0};
    float t = 0;
    int   h = 0;
    int   wc = 0;
};

class WeatherApp final : public App {
public:
    const char* name() const override { return "Weather"; }
    uint32_t tickPeriodMs() const override { return 0; }

    void onEnter(lv_obj_t* parent) override
    {
        _tok = std::make_shared<AppToken>();
        _parent = parent;

        ui_make_screen(parent);
        ui_make_statusbar(parent);

        _city = ui_make_label(parent, &font_alibaba_24, lv_color_white(),
                              Database::inst().config().city);
        lv_obj_align(_city, LV_ALIGN_TOP_MID, 0, 40);

        // Число температуры и знак °C — два отдельных лейбла.
        // В font_alibaba_60 нет глифа U+00B0, поэтому °C идёт
        // встроенным Montserrat'ом, а цифра — Alibaba.
        _temp = ui_make_label(parent, &font_alibaba_60, lv_color_white(), "--");
        lv_obj_align(_temp, LV_ALIGN_CENTER, -18, -10);

        _deg = ui_make_label(parent, &lv_font_montserrat_24,
                             lv_color_white(), "°C");
        lv_obj_align_to(_deg, _temp, LV_ALIGN_OUT_RIGHT_TOP, 2, 8);

        _info = ui_make_label(parent, &font_alibaba_24,
                              lv_color_hex(0xAAAAAA), "Loading...");
        lv_obj_align(_info, LV_ALIGN_CENTER, 0, 60);

        // Спиннер, чтобы пользователь видел процесс
        _spin = lv_spinner_create(parent);
        lv_obj_set_size(_spin, 40, 40);
        lv_obj_align(_spin, LV_ALIGN_BOTTOM_MID, 0, -20);

        // Кнопка Retry — на случай ошибки
        _retry = ui_make_btn(parent, "Retry", cb_retry, this, 100, 36);
        lv_obj_align(_retry, LV_ALIGN_BOTTOM_MID, 0, -20);
        lv_obj_add_flag(_retry, LV_OBJ_FLAG_HIDDEN);

        _fetch();
    }

    void onExit() override
    {
        if (_tok) _tok->alive = false;
    }

private:
    std::shared_ptr<AppToken> _tok;
    lv_obj_t *_parent{}, *_city{}, *_temp{}, *_info{}, *_spin{}, *_retry{}, *_deg{};

    void _fetch()
    {
        if (_spin)  lv_obj_clear_flag(_spin, LV_OBJ_FLAG_HIDDEN);
        if (_retry) lv_obj_add_flag(_retry, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(_info, "Loading...");

        auto& cfg = Database::inst().config();
        float lat = cfg.lat, lon = cfg.lon;

        apptask_run(_tok,
            [lat, lon]() -> WeatherResult {
                WeatherResult r;
                if (!hw_get_wifi_connected()) {
                    strlcpy(r.err, "No WiFi", sizeof(r.err));
                    return r;
                }
                char url[220];
                snprintf(url, sizeof(url),
                    "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
                    "&current=temperature_2m,relative_humidity_2m,weather_code",
                    lat, lon);

                WiFiClient client;
                client.setTimeout(4);
                HTTPClient http;
                http.begin(client, url);
                http.setConnectTimeout(4000);
                http.setTimeout(4000);
                http.setReuse(false);

                int code = http.GET();
                if (code != 200) {
                    snprintf(r.err, sizeof(r.err), "HTTP %d", code);
                    http.end();
                    return r;
                }
                String body = http.getString();
                http.end();

                JsonDocument doc;
                auto e = deserializeJson(doc, body);
                if (e) {
                    snprintf(r.err, sizeof(r.err), "Parse: %s", e.c_str());
                    return r;
                }
                r.t  = doc["current"]["temperature_2m"]        | 0.0f;
                r.h  = doc["current"]["relative_humidity_2m"]  | 0;
                r.wc = doc["current"]["weather_code"]          | 0;
                r.ok = true;
                return r;
            },
            [this](WeatherResult r) {
                if (_spin)  { UI_OBJ_DEL(_spin);  _spin = nullptr; }
                if (!r.ok) {
                    lv_label_set_text(_info, r.err[0] ? r.err : "Unknown error");
                    if (_retry) lv_obj_clear_flag(_retry, LV_OBJ_FLAG_HIDDEN);
                    return;
                }
                lv_label_set_text_fmt(_temp, "%.0f", r.t);
                lv_obj_align_to(_deg, _temp, LV_ALIGN_OUT_RIGHT_TOP, 2, 8);
                lv_label_set_text_fmt(_info, "hum %d%%  code %d", r.h, r.wc);
            });
    }

    static void cb_retry(lv_event_t* e) {
        static_cast<WeatherApp*>(lv_event_get_user_data(e))->_fetch();
    }
};

std::unique_ptr<App> app_weather_make() { return std::make_unique<WeatherApp>(); }