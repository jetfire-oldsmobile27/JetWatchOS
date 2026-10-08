#include "apps.h"
#include "ui/ui_common.h"

class TerminalApp final : public App {
public:
    const char* name() const override { return "Terminal"; }
    uint32_t tickPeriodMs() const override { return 100; }

    void onEnter(lv_obj_t* parent) override {
        ui_make_screen(parent);
        ui_make_statusbar(parent);

        _log = lv_label_create(parent);
        lv_obj_set_size(_log, LV_PCT(96), LV_PCT(100) - 32);
        lv_obj_align(_log, LV_ALIGN_BOTTOM_MID, 0, -4);
        lv_label_set_long_mode(_log, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(_log, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(_log, lv_color_hex(0x00FF00), 0);
        lv_obj_set_style_bg_color(_log, lv_color_black(), 0);
        lv_label_set_text(_log, "");
        _len = 0;

        Serial.begin(115200);   // вход от отладочного UART RPi
    }

    void onExit() override {}

    void onTick() override {
        bool got = false;
        while (Serial.available() && _len < sizeof(_buf) - 1) {
            char c = (char)Serial.read();
            if (c == '\r') continue;
            _buf[_len++] = c;
            got = true;
        }
        if (got) {
            // автоочистка при переполнении
            if (_len > sizeof(_buf) - 256) {
                memmove(_buf, _buf + _len / 2, _len - _len / 2);
                _len -= _len / 2;
            }
            _buf[_len] = 0;
            lv_label_set_text(_log, _buf);
            lv_obj_scroll_to_y(lv_obj_get_parent(_log), LV_COORD_MAX, LV_ANIM_OFF);
        }
    }

private:
    lv_obj_t* _log  {nullptr};
    char _buf[3000];
    size_t _len = 0;
};

std::unique_ptr<App> app_terminal_make() {
    return std::make_unique<TerminalApp>();
}