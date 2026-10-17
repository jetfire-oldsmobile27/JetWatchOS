#pragma once
#include <lvgl.h>

// Показать/обновить/убрать CRT-заставку.
// ui_splash_begin() вызывается один раз сразу после beginLvglHelper().
// ui_splash_step() — между этапами инициализации; внутри сам крутит
// lv_task_handler(), чтобы заставка успела отрисоваться.
// ui_splash_end() обычно не нужен — заставка закрывается сама через
// минимум 4.5 с, когда прогресс дошёл до 100 %.
void ui_splash_begin();
void ui_splash_step(const char* text, uint8_t percent);
void ui_splash_end();
bool ui_splash_active();