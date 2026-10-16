#pragma once
#include <time.h>
#include <stdint.h>
#include "hal_interface.h"   // monitor_params_t

namespace datacache {

// Запускает фоновую задачу опроса. Вызывать ОДИН раз из setup()
// ПОСЛЕ Database::begin() и instance.begin().
void begin();

// Legacy: остаётся для совместимости, ничего не делает (опрос в задаче).
void poll();

// --- Время (RTC) ---
bool       time_valid();
struct tm  time();          // ← теперь по значению

// --- Питание/батарея (PMU) ---
bool              monitor_valid();
monitor_params_t  monitor(); // ← теперь по значению

// --- Шаги ---
bool     steps_valid();
uint32_t steps();

// --- Температура BMA423 (медленно меняется, кэшируется 60 с) ---
bool  temperature_valid();
float temperature();

} // namespace datacache