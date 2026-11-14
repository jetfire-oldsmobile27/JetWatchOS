#include "apps.h"
#include "ui/ui_common.h"
#include "core/PowerManager.h"
#include "core/Database.h"
#include "hal/hw.h"
#include <vector>
#include <cstring>
#include <cctype>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

struct Notification {
    char app[16];
    char title[32];
    char body[96];
    char icon[8];
    uint32_t ts;
};

struct NotifPayload {
    char app[16];
    char title[32];
    char body[96];
    char icon[8];
    bool was_off;
};

static std::vector<Notification> s_list;
static lv_obj_t*   s_popup       = nullptr;
static lv_timer_t* s_popup_timer = nullptr;
static QueueHandle_t s_q         = nullptr;

static void _popup_destroy_ui() {
    if (s_popup) { UI_OBJ_DEL(s_popup); s_popup = nullptr; }
}

// Из кнопки DISMISS: мы в LVGL, но НЕ в таймер-колбэке. Можно удалить таймер вручную.
static void _close_popup_from_ui() {
    if (s_popup_timer) {
        lv_timer_t* t = s_popup_timer;
        s_popup_timer = nullptr;
        lv_timer_del(t);
    }
    _popup_destroy_ui();
}

// Из таймер-колбэка: LVGL удалит таймер сам (repeat_count == 1). Только UI.
static void _close_popup_from_timer() {
    s_popup_timer = nullptr;
    _popup_destroy_ui();
}

static lv_color_t _icon_color(const char* key, const char* app) {
    if (key && key[0] && strcmp(key, "app") != 0) {
        if (!strcmp(key, "tg"))   return lv_color_hex(0x2AABEE);
        if (!strcmp(key, "wa"))   return lv_color_hex(0x25D366);
        if (!strcmp(key, "vk"))   return lv_color_hex(0x0077FF);
        if (!strcmp(key, "ig"))   return lv_color_hex(0xE1306C);
        if (!strcmp(key, "mail")) return lv_color_hex(0xEA4335);
        if (!strcmp(key, "sms"))  return lv_color_hex(0x34C759);
        if (!strcmp(key, "call")) return lv_color_hex(0x34C759);
    }
    // Детерминированный цвет по имени приложения (стабильный между вызовами).
    if (app && app[0]) {
        uint32_t h = 2166136261u;
        for (const char* p = app; *p; ++p) { h ^= (uint8_t)*p; h *= 16777619u; }
        static const uint32_t pal[] = {
            0xE53935, 0x8E24AA, 0x3949AB, 0x1E88E5,
            0x00897B, 0x43A047, 0xFB8C00, 0x6D4C41,
        };
        return lv_color_hex(pal[h & 7]);
    }
    return lv_color_hex(0x666666);
}

static lv_obj_t* _icon_circle(lv_obj_t* parent, int size,
                               const char* app, const char* icon_key)
{
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_size(c, size, size);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(c, _icon_color(icon_key, app), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_CLICKABLE);

    char letter[2] = { '?', 0 };
    if (icon_key && icon_key[0] && strcmp(icon_key, "app") != 0) {
        letter[0] = (char)toupper((unsigned char)icon_key[0]);
    } else if (app && app[0] && (uint8_t)app[0] < 128) {
        letter[0] = (char)toupper((unsigned char)app[0]);
    } else if (app) {
        // ищем первую ASCII-букву в имени (напр. "Яндекс Music" → 'M')
        for (const char* p = app; *p; ++p) {
            if ((uint8_t)*p < 128 && isalpha((unsigned char)*p)) {
                letter[0] = (char)toupper((unsigned char)*p);
                break;
            }
        }
    }

    lv_obj_t* l = lv_label_create(c);
    lv_label_set_text(l, letter);
    lv_obj_set_style_text_font(l, size >= 60 ? &font_alibaba_40 : &font_alibaba_24, 0);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    lv_obj_center(l);
    return c;
}

static void _show_banner(const Notification& n) {
    _popup_destroy_ui();
    if (s_popup_timer) {
        lv_timer_t* t = s_popup_timer; s_popup_timer = nullptr;
        lv_timer_del(t);
    }

    s_popup = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_popup);
    lv_obj_set_size(s_popup, LV_PCT(90), 68);
    lv_obj_align(s_popup, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_bg_color(s_popup, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_bg_opa(s_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_popup, 0, 0);
    lv_obj_set_style_radius(s_popup, 14, 0);
    lv_obj_set_style_pad_all(s_popup, 8, 0);
    lv_obj_move_foreground(s_popup);

    lv_obj_t* ic = _icon_circle(s_popup, 44, n.app, n.icon);
    lv_obj_align(ic, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* txt = ui_make_label(s_popup, &font_alibaba_12, lv_color_white(), "");
    lv_label_set_text_fmt(txt, "%s: %s\n%s", n.app, n.title, n.body);
    lv_obj_set_width(txt, LV_PCT(72));
    lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);
    lv_obj_align(txt, LV_ALIGN_RIGHT_MID, -4, 0);

    s_popup_timer = lv_timer_create([](lv_timer_t*){
        _close_popup_from_timer();
    }, 4000, nullptr);
    lv_timer_set_repeat_count(s_popup_timer, 1);
}

static void _show_fullscreen(const Notification& n) {
    _popup_destroy_ui();
    if (s_popup_timer) {
        lv_timer_t* t = s_popup_timer; s_popup_timer = nullptr;
        lv_timer_del(t);
    }

    s_popup = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_popup);
    lv_obj_set_size(s_popup, LV_PCT(100), LV_PCT(100));
    lv_obj_center(s_popup);
    lv_obj_set_style_bg_color(s_popup, lv_color_hex(0x0A0A0A), 0);
    lv_obj_set_style_bg_opa(s_popup, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_popup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(s_popup);

    lv_obj_t* ic = _icon_circle(s_popup, 60, n.app, n.icon);
    lv_obj_align(ic, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t* app_lbl = ui_make_label(s_popup, &font_alibaba_12,
                                       lv_color_hex(0xAAAAAA), n.app);
    lv_obj_align(app_lbl, LV_ALIGN_TOP_MID, 0, 78);

    lv_obj_t* title_lbl = ui_make_label(s_popup, &font_alibaba_24,
                                         lv_color_white(), n.title);
    lv_obj_set_width(title_lbl, LV_PCT(92));
    lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(title_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_MID, 0, 96);

    lv_obj_t* body_lbl = ui_make_label(s_popup, &font_alibaba_12,
                                        lv_color_hex(0xCCCCCC), n.body);
    lv_obj_set_width(body_lbl, LV_PCT(92));
    lv_obj_set_height(body_lbl, 44);
    lv_label_set_long_mode(body_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(body_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(body_lbl, LV_ALIGN_TOP_MID, 0, 128);

    lv_obj_t* dismiss = ui_make_btn_red(s_popup, "DISMISS",
        [](lv_event_t*){ _close_popup_from_ui(); }, nullptr, 140, 36);
    lv_obj_align(dismiss, LV_ALIGN_BOTTOM_MID, 0, -10);

    s_popup_timer = lv_timer_create([](lv_timer_t*){
        _close_popup_from_timer();
    }, 15000, nullptr);
    lv_timer_set_repeat_count(s_popup_timer, 1);
}

static void _push_impl(const NotifPayload& p) {
    Serial.printf("[NOTIF] '%s' / '%s' ico=%s was_off=%d\n",
                  p.app, p.title, p.icon, (int)p.was_off);

    Notification n{};
    strlcpy(n.app,   p.app,   sizeof(n.app));
    strlcpy(n.title, p.title, sizeof(n.title));
    strlcpy(n.body,  p.body,  sizeof(n.body));
    strlcpy(n.icon,  p.icon,  sizeof(n.icon));
    n.ts = (uint32_t)time(nullptr);
    s_list.insert(s_list.begin(), n);
    if (s_list.size() > 20) s_list.pop_back();

    if (p.was_off) PowerManager::inst().wake();
    hw_feedback();
    if (Database::inst().config().sounds_on) {
        hw_set_sd_music_play(AUDIO_SOURCE_FATFS, "notify.mp3");
    }

    if (p.was_off) _show_fullscreen(n);
    else           _show_banner(n);
}

void app_notifications_push(const char* app, const char* title,
                             const char* body, const char* icon_key)
{
    if (!s_q) s_q = xQueueCreate(8, sizeof(NotifPayload));

    NotifPayload p{};
    strlcpy(p.app,   app ? app : "", sizeof(p.app));
    strlcpy(p.title, title ? title : "", sizeof(p.title));
    strlcpy(p.body,  body ? body : "", sizeof(p.body));
    strlcpy(p.icon,  icon_key ? icon_key : "", sizeof(p.icon));
    p.was_off = PowerManager::inst().isScreenOff();

    if (xQueueSend(s_q, &p, 0) != pdPASS) {
        Serial.println("[NOTIF] queue full, drop");
    }
}

void app_notifications_poll() {
    if (!s_q) return;
    NotifPayload p;
    while (xQueueReceive(s_q, &p, 0) == pdTRUE) _push_impl(p);
}

class NotificationsApp final : public App {
public:
    const char* name() const override { return "Notifications"; }

    void onEnter(lv_obj_t* parent) override {
        ui_make_screen(parent);
        ui_make_statusbar(parent);

        lv_obj_t* list = lv_obj_create(parent);
        lv_obj_set_size(list, LV_PCT(100), LV_PCT(100) - 24);
        lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(list, 0, 0);
        lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(list, 4, 0);
        lv_obj_set_style_pad_row(list, 4, 0);
        lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

        if (s_list.empty()) {
            ui_make_label(list, &font_alibaba_24, lv_color_hex(0x888888),
                          "No notifications");
            return;
        }
        for (auto& n : s_list) {
            lv_obj_t* card = lv_obj_create(list);
            lv_obj_set_size(card, LV_PCT(100), 72);
            lv_obj_set_style_radius(card, 10, 0);
            lv_obj_set_style_bg_color(card, lv_color_hex(0x1F1F1F), 0);
            lv_obj_set_style_border_width(card, 0, 0);
            lv_obj_set_style_pad_hor(card, 8, 0);
            lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t* ic = _icon_circle(card, 48, n.app, n.icon);
            lv_obj_align(ic, LV_ALIGN_LEFT_MID, 0, 0);

            lv_obj_t* l = ui_make_label(card, &font_alibaba_12,
                                         lv_color_white(), "");
            lv_label_set_text_fmt(l, "%s: %s\n%s", n.app, n.title, n.body);
            lv_obj_set_width(l, LV_PCT(74));
            lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
            lv_obj_align(l, LV_ALIGN_RIGHT_MID, -4, 0);
        }
    }
};

std::unique_ptr<App> app_notifications_make() {
    return std::make_unique<NotificationsApp>();
}