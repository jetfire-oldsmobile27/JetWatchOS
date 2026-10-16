#pragma once
#include <stdint.h>

/**
 * Survival mode — экстремальный энергосберегающий режим для замера реальной
 * автономности железа. МК проводит всё время в deep sleep и просыпается
 * ТОЛЬКО на нажатие боковой кнопки, чтобы напечатать метрики в Serial.
 *
 * Точки входа:
 *   - survival::armAndReboot()    — вызывается из BLE-хендлера (мобильное приложение).
 *   - survival::runSurvivalBoot() — вызывается ПЕРВОЙ СТРОКОЙ setup().
 *   - survival::isArmed()         — быстрый чек флага в NVS.
 *
 * Что не работает в режиме (намеренно не инициализируется):
 *   экран, LVGL, LoRa, Wi-Fi, BLE, IR, тачпад, BMA423, GPS, аудио.
 * Единственное, что инициализируется на пробуждении — PMU/RTC через
 * instance.begin() (нужен для чтения заряда батареи и метки времени).
 *
 * Выход: двойное нажатие боковой кнопки. Флаг снимается, esp_restart().
 */
namespace survival {

/** true, если флаг арминга сохранён в NVS. Читается быстро. */
bool isArmed();

/**
 * Взводит флаг в NVS и делает esp_restart().
 * Не возвращает управление.
 */
[[noreturn]] 
void armAndReboot();

/** Снять флаг (на случай, если понадобится выключить удалённо). */
void disarmFromPhone();

/**
 * Вызывается в самом начале setup() ДО Serial.begin / crash_init / instance.begin.
 * - Если не armed -> сразу return, вызывающий продолжает обычный boot.
 * - Если armed:
 *     * первый boot после арминга: печатает ENTER, уходит в deep sleep;
 *     * wake по кнопке: печатает метрики, детектит double-press,
 *       уходит в deep sleep (или в esp_restart при double-press).
 *   На этих путях НЕ возвращает управление.
 */
void runSurvivalBoot();

} // namespace survival