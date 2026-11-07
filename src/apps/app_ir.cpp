// ─────────────────────────────────────────────────────────────
// IR-заголовки ОБЯЗАТЕЛЬНО первыми (см. п. 3.1)
// ─────────────────────────────────────────────────────────────
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <IRac.h>

#include "apps.h"
#include "ui/ui_common.h"
#include "core/PowerManager.h"
#include "AppTask.h"

// ============================== IR hardware ==============================
static const uint16_t kIrLedPin = 2;   // T-Watch S3: IR LED = GPIO2
static IRsend irsend(kIrLedPin);
static IRac   ac(kIrLedPin, false, true);

// ============================== Data models ==============================
struct IrKey {
    const char*   label;
    uint64_t      code;
    uint16_t      nbits;
    decode_type_t proto;
};

struct TvModel { const char* name; const IrKey* keys; size_t count; };
struct AcModel { const char* name; decode_type_t proto; };

// ------------- TV brands -------------
static const IrKey tvSamsung[] = {
    {"POWER", 0xE0E040BF, 32, decode_type_t::NEC},
    {"VOL+",  0xE0E0E01F, 32, decode_type_t::NEC},
    {"VOL-",  0xE0E0D02F, 32, decode_type_t::NEC},
    {"CH+",   0xE0E048B7, 32, decode_type_t::NEC},
    {"CH-",   0xE0E008F7, 32, decode_type_t::NEC},
    {"MUTE",  0xE0E0F00F, 32, decode_type_t::NEC},
};

static const IrKey tvLg[] = {
    {"POWER", 0x20DF10EF, 32, decode_type_t::NEC},
    {"VOL+",  0x20DF40BF, 32, decode_type_t::NEC},
    {"VOL-",  0x20DFC03F, 32, decode_type_t::NEC},
    {"CH+",   0x20DF00FF, 32, decode_type_t::NEC},
    {"CH-",   0x20DF807F, 32, decode_type_t::NEC},
    {"MUTE",  0x20DF906F, 32, decode_type_t::NEC},
};

static const IrKey tvSony[] = {
    {"POWER", 0xA90, 12, decode_type_t::SONY},
    {"VOL+",  0x490, 12, decode_type_t::SONY},
    {"VOL-",  0xC90, 12, decode_type_t::SONY},
    {"CH+",   0x090, 12, decode_type_t::SONY},
    {"CH-",   0x890, 12, decode_type_t::SONY},
    {"MUTE",  0x290, 12, decode_type_t::SONY},
};

static const IrKey tvPhilips[] = {
    {"POWER", 0x0C, 12, decode_type_t::RC5},
    {"VOL+",  0x10, 12, decode_type_t::RC5},
    {"VOL-",  0x11, 12, decode_type_t::RC5},
    {"CH+",   0x20, 12, decode_type_t::RC5},
    {"CH-",   0x21, 12, decode_type_t::RC5},
    {"MUTE",  0x0D, 12, decode_type_t::RC5},
};

static const TvModel tvModels[] = {
    {"Samsung", tvSamsung, 6},
    {"LG",      tvLg,      6},
    {"Sony",    tvSony,    6},
    {"Philips", tvPhilips, 6},
};

// ------------- AC brands (через IRac) -------------
static const AcModel acModels[] = {
    {"Daikin",     decode_type_t::DAIKIN},
    {"Gree",       decode_type_t::GREE},
    {"Midea",      decode_type_t::MIDEA},
    {"Mitsubishi", decode_type_t::MITSUBISHI_AC},
    {"LG",         decode_type_t::LG},
    {"Samsung",    decode_type_t::SAMSUNG_AC},
    {"Panasonic",  decode_type_t::PANASONIC_AC},
    {"Haier",      decode_type_t::HAIER_AC},
};

// ------------- AC state enumerations -------------
static const stdAc::opmode_t kModes[] = {
    stdAc::opmode_t::kCool,
    stdAc::opmode_t::kHeat,
    stdAc::opmode_t::kDry,
    stdAc::opmode_t::kFan,
    stdAc::opmode_t::kAuto,
};
static const char* kModeNames[] = {"COOL", "HEAT", "DRY", "FAN", "AUTO"};

static const stdAc::fanspeed_t kFans[] = {
    stdAc::fanspeed_t::kLow,
    stdAc::fanspeed_t::kMedium,
    stdAc::fanspeed_t::kHigh,
    stdAc::fanspeed_t::kAuto,
};
static const char* kFanNames[] = {"LOW", "MED", "HIGH", "AUTO"};

// ============================== App ==============================
class IrApp final : public App {
public:
    const char* name() const override { return "IR Remote"; }

    void onEnter(lv_obj_t* parent) override {
        _parent = parent;
        _tvIdx  = -1;
        _acIdx  = -1;
        _showRoot();
    }

    void onExit() override {}

private:
    lv_obj_t* _parent{nullptr};
    int _tvIdx = -1;
    int _acIdx = -1;

    int  _acTemp    = 24;
    int  _acModeIdx = 0;
    int  _acFanIdx  = 1;
    bool _acPower   = false;

    lv_obj_t* _lblTemp {nullptr};
    lv_obj_t* _lblMode {nullptr};
    lv_obj_t* _lblFan  {nullptr};
    lv_obj_t* _lblPower{nullptr};

    // ---------------- UI helpers ----------------
    void _clear() {
        lv_obj_clean(_parent);
        ui_make_screen(_parent);
        ui_make_statusbar(_parent);
    }

    lv_obj_t* _scroll_area() {
        lv_obj_t* a = lv_obj_create(_parent);
        lv_obj_set_size(a, LV_PCT(100), LV_PCT(100) - 24);
        lv_obj_align(a, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(a, 0, 0);
        lv_obj_set_style_pad_all(a, 8, 0);
        lv_obj_set_style_pad_row(a, 8, 0);
        lv_obj_set_flex_flow(a, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scrollbar_mode(a, LV_SCROLLBAR_MODE_AUTO);
        return a;
    }

    lv_obj_t* _title(lv_obj_t* p, const char* txt) {
        lv_obj_t* l = ui_make_label(p, &font_alibaba_24, lv_color_white(), txt);
        lv_obj_set_width(l, LV_PCT(100));
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        return l;
    }

    lv_obj_t* _btn(lv_obj_t* p, const char* txt, int w, int h,
                   lv_event_cb_t cb, void* user = nullptr) {
        lv_obj_t* b = lv_btn_create(p);
        lv_obj_set_size(b, w, h);
        lv_obj_set_style_radius(b, 12, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x2A2A2A), 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x3A3A3A), LV_STATE_PRESSED);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user ? user : this);

        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, txt);
        lv_obj_set_style_text_font(l, &font_alibaba_24, 0);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_obj_center(l);
        return b;
    }

    lv_obj_t* _list_btn(lv_obj_t* p, const char* txt, int idx, lv_event_cb_t cb) {
        lv_obj_t* b = lv_btn_create(p);
        lv_obj_set_size(b, LV_PCT(90), 52);
        lv_obj_set_style_radius(b, 12, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x2A2A2A), 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x3A3A3A), LV_STATE_PRESSED);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, this);

        int* hold = new int(idx);
        lv_obj_set_user_data(b, hold);
        lv_obj_add_event_cb(b, [](lv_event_t* e){
            delete static_cast<int*>(lv_obj_get_user_data(HW_EV_OBJ(e)));
        }, LV_EVENT_DELETE, nullptr);

        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, txt);
        lv_obj_set_style_text_font(l, &font_alibaba_24, 0);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_obj_center(l);
        return b;
    }

    // ---------------- Screens ----------------

    void _showRoot() {
        _clear();
        lv_obj_t* c = _scroll_area();
        _title(c, "IR Remote");

        _btn(c, "TV", LV_PCT(90), 64, [](lv_event_t* e){
            PowerManager::inst().resetActivity();
            static_cast<IrApp*>(lv_event_get_user_data(e))->_showTvList();
        });
        _btn(c, "Air Conditioner", LV_PCT(90), 64, [](lv_event_t* e){
            PowerManager::inst().resetActivity();
            static_cast<IrApp*>(lv_event_get_user_data(e))->_showAcList();
        });
    }

    void _showTvList() {
        _clear();
        lv_obj_t* c = _scroll_area();
        _title(c, "TV brand");

        for (size_t i = 0; i < sizeof(tvModels) / sizeof(tvModels[0]); ++i) {
            _list_btn(c, tvModels[i].name, (int)i, [](lv_event_t* e){
                auto* self = static_cast<IrApp*>(lv_event_get_user_data(e));
                int idx = *static_cast<int*>(lv_obj_get_user_data(HW_EV_OBJ(e)));
                self->_showTvRemote(idx);
            });
        }
        _btn(c, "Back", LV_PCT(90), 44, [](lv_event_t* e){
            static_cast<IrApp*>(lv_event_get_user_data(e))->_showRoot();
        });
    }

    void _showTvRemote(int idx) {
        _tvIdx = idx;
        _clear();
        const auto& m = tvModels[idx];
        lv_obj_t* c = _scroll_area();
        _title(c, m.name);

        lv_obj_t* grid = lv_obj_create(c);
        lv_obj_set_width(grid, LV_PCT(100));
        lv_obj_set_height(grid, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(grid, 0, 0);
        lv_obj_set_style_pad_all(grid, 0, 0);
        lv_obj_set_style_pad_row(grid, 8, 0);
        lv_obj_set_style_pad_column(grid, 8, 0);
        lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

        for (size_t i = 0; i < m.count; ++i) {
            auto* p = new int((int)i);
            lv_obj_t* b = lv_btn_create(grid);
            lv_obj_set_size(b, LV_PCT(45), 56);
            lv_obj_set_style_radius(b, 12, 0);
            lv_obj_set_style_bg_color(b, lv_color_hex(0x2A2A2A), 0);
            lv_obj_set_style_bg_color(b, lv_color_hex(0x3A3A3A), LV_STATE_PRESSED);
            lv_obj_set_user_data(b, p);
            lv_obj_add_event_cb(b, [](lv_event_t* e){
                auto* self = static_cast<IrApp*>(lv_event_get_user_data(e));
                int k = *static_cast<int*>(lv_obj_get_user_data(HW_EV_OBJ(e)));
                PowerManager::inst().resetActivity();
                self->_sendKey(&tvModels[self->_tvIdx].keys[k]);
            }, LV_EVENT_CLICKED, this);
            lv_obj_add_event_cb(b, [](lv_event_t* e){
                delete static_cast<int*>(lv_obj_get_user_data(HW_EV_OBJ(e)));
            }, LV_EVENT_DELETE, nullptr);

            lv_obj_t* l = lv_label_create(b);
            lv_label_set_text(l, m.keys[i].label);
            lv_obj_set_style_text_font(l, &font_alibaba_24, 0);
            lv_obj_set_style_text_color(l, lv_color_white(), 0);
            lv_obj_center(l);
        }

        _btn(c, "Back", LV_PCT(90), 44, [](lv_event_t* e){
            static_cast<IrApp*>(lv_event_get_user_data(e))->_showTvList();
        });
    }

    void _showAcList() {
        _clear();
        lv_obj_t* c = _scroll_area();
        _title(c, "AC brand");

        for (size_t i = 0; i < sizeof(acModels) / sizeof(acModels[0]); ++i) {
            _list_btn(c, acModels[i].name, (int)i, [](lv_event_t* e){
                auto* self = static_cast<IrApp*>(lv_event_get_user_data(e));
                int idx = *static_cast<int*>(lv_obj_get_user_data(HW_EV_OBJ(e)));
                self->_showAcControl(idx);
            });
        }
        _btn(c, "Back", LV_PCT(90), 44, [](lv_event_t* e){
            static_cast<IrApp*>(lv_event_get_user_data(e))->_showRoot();
        });
    }

    void _showAcControl(int idx) {
        _acIdx = idx;
        _clear();
        lv_obj_t* c = _scroll_area();
        _title(c, acModels[idx].name);

        _lblTemp = ui_make_label(c, &font_alibaba_60, lv_color_white(), "");
        lv_label_set_text_fmt(_lblTemp, "%d", _acTemp);
        lv_obj_set_width(_lblTemp, LV_PCT(100));
        lv_obj_set_style_text_align(_lblTemp, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t* row = lv_obj_create(c);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 56);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_pad_column(row, 8, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        _btn(row, "\u2212", LV_PCT(45), 56, [](lv_event_t* e){
            auto* self = static_cast<IrApp*>(lv_event_get_user_data(e));
            PowerManager::inst().resetActivity();
            if (self->_acTemp > 16) self->_acTemp--;
            lv_label_set_text_fmt(self->_lblTemp, "%d", self->_acTemp);
            self->_sendAcState();
        });
        _btn(row, "+", LV_PCT(45), 56, [](lv_event_t* e){
            auto* self = static_cast<IrApp*>(lv_event_get_user_data(e));
            PowerManager::inst().resetActivity();
            if (self->_acTemp < 30) self->_acTemp++;
            lv_label_set_text_fmt(self->_lblTemp, "%d", self->_acTemp);
            self->_sendAcState();
        });

        auto* modeBtn = _btn(c, "", LV_PCT(90), 56, [](lv_event_t* e){
            auto* self = static_cast<IrApp*>(lv_event_get_user_data(e));
            PowerManager::inst().resetActivity();
            self->_acModeIdx = (self->_acModeIdx + 1) % 5;
            lv_label_set_text_fmt(self->_lblMode, "Mode: %s", kModeNames[self->_acModeIdx]);
            self->_sendAcState();
        });
        _lblMode = lv_obj_get_child(modeBtn, 0);
        lv_label_set_text_fmt(_lblMode, "Mode: %s", kModeNames[_acModeIdx]);

        auto* fanBtn = _btn(c, "", LV_PCT(90), 56, [](lv_event_t* e){
            auto* self = static_cast<IrApp*>(lv_event_get_user_data(e));
            PowerManager::inst().resetActivity();
            self->_acFanIdx = (self->_acFanIdx + 1) % 4;
            lv_label_set_text_fmt(self->_lblFan, "Fan: %s", kFanNames[self->_acFanIdx]);
            self->_sendAcState();
        });
        _lblFan = lv_obj_get_child(fanBtn, 0);
        lv_label_set_text_fmt(_lblFan, "Fan: %s", kFanNames[_acFanIdx]);

        auto* pwrBtn = _btn(c, "", LV_PCT(90), 64, [](lv_event_t* e){
            auto* self = static_cast<IrApp*>(lv_event_get_user_data(e));
            PowerManager::inst().resetActivity();
            self->_acPower = !self->_acPower;
            lv_label_set_text(self->_lblPower,
                              self->_acPower ? "POWER: ON" : "POWER: OFF");
            self->_sendAcState();
        });
        _lblPower = lv_obj_get_child(pwrBtn, 0);
        lv_label_set_text(_lblPower, _acPower ? "POWER: ON" : "POWER: OFF");

        _btn(c, "Back", LV_PCT(90), 44, [](lv_event_t* e){
            static_cast<IrApp*>(lv_event_get_user_data(e))->_showAcList();
        });
    }

    // ---------------- Sending ----------------

    static void _sendKey(const IrKey* k) {
        auto proto = k->proto;
        auto code  = k->code;
        auto nbits = k->nbits;
        apptask_run_detached([proto, code, nbits]() {
            irsend.begin();
            irsend.send(proto, code, nbits);
        });
    }

    void _sendAcState() {
        auto  proto = acModels[_acIdx].proto;
        bool  power = _acPower;
        float temp  = (float)_acTemp;
        auto  mode  = _acPower ? kModes[_acModeIdx] : stdAc::opmode_t::kOff;
        auto  fan   = kFans[_acFanIdx];

        apptask_run_detached([proto, power, temp, mode, fan]() {
            ac.sendAc(proto, -1, power, mode, temp,
                      /*celsius*/ true, fan,
                      stdAc::swingv_t::kOff, stdAc::swingh_t::kOff,
                      false, false, false, false, false, false, false, -1);
        });
    }
};

std::unique_ptr<App> app_ir_make() {
    return std::make_unique<IrApp>();
}