#include "apps.h"
#include "ui/ui_common.h"
#include "core/Database.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class WeatherApp : public App {
public:
    const char* name() const override { return "Weather"; }
    uint32_t tickPeriodMs() const override { return 60000; }

    void onEnter(lv_obj_t* parent) override {
        ui_make_screen(parent);
        ui_make_statusbar(parent);

        _city = ui_make_label(parent, &font_alibaba_24, lv_color_white(), "--");
        lv_obj_align(_city, LV_ALIGN_TOP_MID, 0, 40);
        _temp = ui_make_label(parent, &font_alibaba_60, lv_color_white(), "--°C");
        lv_obj_align(_temp, LV_ALIGN_CENTER, 0, -10);
        _info = ui_make_label(parent, &font_alibaba_24, lv_color_hex(0xAAAAAA), "loading...");
        lv_obj_align(_info, LV_ALIGN_CENTER, 0, 60);

        lv_label_set_text(_city, Database::inst().config().city);
        _fetch();
    }

private:
    lv_obj_t *_city, *_temp, *_info;
    uint32_t _last_fetch = 0;

    void _fetch()
    {
        if (!hw_get_wifi_connected()) {
            lv_label_set_text(_info, "No WiFi");
            return;
        }
        auto& cfg = Database::inst().config();
        char url[200];
        snprintf(url, sizeof(url),
                 "http://api.open-meteo.com/v1/forecast?"
                 "latitude=%.4f&longitude=%.4f&current=temperature_2m,"
                 "relative_humidity_2m,weather_code",
                 cfg.lat, cfg.lon);

        WiFiClient client; HTTPClient http;
        http.begin(client, url);
        int code = http.GET();
        if (code != 200) { lv_label_set_text(_info, "HTTP err"); http.end(); return; }

        String body = http.getString(); http.end();

        JsonDocument doc;
        if (deserializeJson(doc, body) != DeserializationError::Ok) {
            lv_label_set_text(_info, "Parse err"); return;
        }
        float t = doc["current"]["temperature_2m"] | 0.0f;
        int   h = doc["current"]["relative_humidity_2m"] | 0;
        int   wc = doc["current"]["weather_code"] | 0;

        lv_label_set_text_fmt(_temp, "%.0f°C", t);
        lv_label_set_text_fmt(_info, "hum %d%%  code %d", h, wc);
    }
};

std::unique_ptr<App> app_weather_make() { return std::make_unique<WeatherApp>(); }