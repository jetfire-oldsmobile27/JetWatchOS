// ui/ui_tokens.h
#pragma once
#include <lvgl.h>

// =====================================================================
//  DESIGN TOKENS — 1:1 c :root в jetwatch_ui.html
//  Все значения — источник истины. Никаких "магических" цветов и чисел
//  в коде экранов быть не должно.
// =====================================================================
namespace ui {

namespace col {
    constexpr uint32_t BG         = 0x000000;
    constexpr uint32_t SURFACE    = 0x141414;
    constexpr uint32_t SURFACE_2  = 0x1F1F1F;
    constexpr uint32_t PRESS      = 0x2C2C2C;
    constexpr uint32_t POP        = 0x2A2A2A;
    constexpr uint32_t TEXT       = 0xFFFFFF;
    constexpr uint32_t TEXT_2     = 0xA8A8A8;
    constexpr uint32_t TEXT_3     = 0x6A6A6A;
    constexpr uint32_t DOT        = 0x3A3A3A;
    constexpr uint32_t ACCENT     = 0xFF7A1A;
    constexpr uint32_t BLUE       = 0x3A9EFF;
    constexpr uint32_t GREEN      = 0x34C759;
    constexpr uint32_t RED        = 0xFF453A;
}

inline lv_color_t C(uint32_t v)  { return lv_color_hex(v); }

// читается как в CSS: col::bg(), col::accent() и т.д.
inline lv_color_t bg()        { return C(col::BG);        }
inline lv_color_t surface()   { return C(col::SURFACE);   }
inline lv_color_t surface2()  { return C(col::SURFACE_2); }
inline lv_color_t press()     { return C(col::PRESS);     }
inline lv_color_t pop()       { return C(col::POP);       }
inline lv_color_t text()      { return C(col::TEXT);      }
inline lv_color_t text2()     { return C(col::TEXT_2);    }
inline lv_color_t text3()     { return C(col::TEXT_3);    }
inline lv_color_t dot()       { return C(col::DOT);       }
inline lv_color_t accent()    { return C(col::ACCENT);    }
inline lv_color_t blue()      { return C(col::BLUE);      }
inline lv_color_t green()     { return C(col::GREEN);     }
inline lv_color_t red()       { return C(col::RED);       }

// ---- Геометрия (240×240) ----
constexpr lv_coord_t W               = 240;
constexpr lv_coord_t H               = 240;
constexpr lv_coord_t SCREEN_RADIUS   = 24;
constexpr lv_coord_t STATUSBAR_H     = 28;
constexpr lv_coord_t ROW_H           = 44;
constexpr lv_coord_t ROW_INFO_H      = 30;
constexpr lv_coord_t ROW_RADIUS      = 12;
constexpr lv_coord_t CARD_RADIUS     = 12;
constexpr lv_coord_t ROW_PAD_H       = 12;
constexpr lv_coord_t ROW_GAP         = 6;
constexpr lv_coord_t LIST_PAD_BOTTOM = 16;

// ---- Шрифты (--fa Alibaba / --fm Montserrat) ----
//  f12  -> F_TINY   (700 12)  status bar, section, мелкие подписи
//  f14  -> F_BODY   (500 14)  строки списка, кнопки
//  f24  -> F_TITLE  (700 24)  заголовки экранов
//  f40  -> F_LARGE  (700 40)
//  f60  -> F_HERO   (700 60)  цифры Steps / Stopwatch
//  f100 -> F_GIANT  (700 100) часы 756_2
#define F_TINY   &font_alibaba_12
#define F_BODY   &lv_font_montserrat_14
#define F_TITLE  &font_alibaba_24
#define F_LARGE  &font_alibaba_40
#define F_HERO   &font_alibaba_60
#define F_GIANT  &font_alibaba_100

} // namespace ui