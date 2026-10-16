#include "SurvivalMode.hpp"
#include "hal/hw.h"           // instance, hw_set_disp_backlight, hw_set_display_power
#include <Arduino.h>
#include <Preferences.h>
#include <esp_attr.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <driver/rtc_io.h>
#include <driver/gpio.h>
#include <string.h>
#include <time.h>

// ============================================================================
//  Board-specific configuration — ПОДПРАВЬ ПОД СВОЮ ПЛАТУ
// ============================================================================
// GPIO боковой кнопки. Должен быть RTC-совместимым (0..21 на ESP32-S3).
// На большинстве плат LilyGo T-Watch это GPIO0 (кнопка BOOT/PWR).
#ifndef SURVIVAL_BTN_GPIO
#  define SURVIVAL_BTN_GPIO    GPIO_NUM_21
#endif

// Полярность: true = нажата = LOW (кнопка к GND, с подтяжкой вверх).
#ifndef SURVIVAL_BTN_ACTIVE_LOW
#  define SURVIVAL_BTN_ACTIVE_LOW   1
#endif

// ============================================================================
//  RTC-память (переживает deep sleep и soft reset; на power-on — случайна)
// ============================================================================
static constexpr uint32_t SV_MAGIC = 0x53555256u; // "SURV"

RTC_NOINIT_ATTR struct SvState {
    uint32_t magic;
    uint32_t boot_count;        // каждый пробуждённый boot инкрементируется (для логов)
    uint64_t entry_epoch;       // unix epoch, когда вошли в режим
    uint64_t last_press_epoch;  // unix epoch последнего нажатия (для double-press)
} g_sv;

// ============================================================================
//  NVS-флаг арминга
// ============================================================================
static constexpr const char* NVS_NS  = "surv";
static constexpr const char* NVS_KEY = "armed";

static bool nvs_is_armed()
{
    Preferences p;
    if (!p.begin(NVS_NS, true)) return false;
    bool v = p.getBool(NVS_KEY, false);
    p.end();
    return v;
}

static void nvs_set_armed(bool v)
{
    Preferences p;
    p.begin(NVS_NS, false);
    p.putBool(NVS_KEY, v);
    p.end();
}

// ============================================================================
//  Helpers
// ============================================================================
static const char* wake_reason_str(esp_sleep_wakeup_cause_t r)
{
    switch (r) {
    case ESP_SLEEP_WAKEUP_EXT0:
    case ESP_SLEEP_WAKEUP_EXT1:      return "BUTTON";
    case ESP_SLEEP_WAKEUP_TIMER:     return "TIMER";
    case ESP_SLEEP_WAKEUP_UNDEFINED: return "COLD";
    default:                         return "OTHER";
    }
}

static bool wake_is_button(esp_sleep_wakeup_cause_t r)
{
    return r == ESP_SLEEP_WAKEUP_EXT0 || r == ESP_SLEEP_WAKEUP_EXT1;
}

// Возвращает unix-epoch из PCF8563 или 0, если время не выставили.
static uint64_t rtc_now_epoch()
{
    struct tm t = {};
    hw_get_date_time(t);
    if (t.tm_year + 1900 < 2020) return 0;   // RTC не валидны
    return (uint64_t)mktime(&t);
}

static uint32_t read_battery_percent()
{
    if (hw_get_device_online() & HW_PMU_ONLINE) {
        int pct = instance.pmu.getBatteryPercent();
        if (pct < 0)   pct = 0;
        if (pct > 100) pct = 100;
        return (uint32_t)pct;
    }
    return 0xFFFFFFFFu; // unknown
}

static void fmt_uptime(char* out, size_t n, uint64_t seconds)
{
    uint64_t h = seconds / 3600;
    uint64_t m = (seconds / 60) % 60;
    uint64_t s = seconds % 60;
    snprintf(out, n, "%02llu:%02llu:%02llu",
             (unsigned long long)h, (unsigned long long)m, (unsigned long long)s);
}

static void configure_wake_sources()
{
    const gpio_num_t pin = (gpio_num_t)SURVIVAL_BTN_GPIO;

    // 1) Инициализируем PMU и разрешаем IRQ по короткому нажатию PWR.
    //    Без этого AXP2101 не будет дёргать линию PMU_INT.
    if (hw_get_device_online() & HW_PMU_ONLINE) {
        instance.pmu.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
        instance.pmu.clearIrqStatus();
        instance.pmu.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
        Serial.println("[SURV] PMU IRQ enabled (PKEY_SHORT)");
    }

    // 2) Настраиваем RTC-подтяжку на PMU_INT.
    //    Обычные pinMode/attachInterrupt не выживают deep sleep —
    //    нужны именно rtc_gpio_* функции.
    rtc_gpio_deinit(pin);
    if (SURVIVAL_BTN_ACTIVE_LOW) {
        rtc_gpio_pullup_en(pin);
        rtc_gpio_pulldown_dis(pin);
        esp_sleep_enable_ext0_wakeup(pin, 0);   // просыпаться на LOW
    } else {
        rtc_gpio_pulldown_en(pin);
        rtc_gpio_pullup_dis(pin);
        esp_sleep_enable_ext0_wakeup(pin, 1);
    }

    // 3) Таймер НЕ включаем — поэтому и не пытаемся его «отключать».
    //    Строка esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER)
    //    убрана: она падала с ESP_ERR_INVALID_STATE.
}

[[noreturn]] static void sleep_now()
{
    // ВАЖНО: дисплейный драйвер в режиме выживания не инициализирован,
    // поэтому НИКАКИХ hw_set_disp_backlight()/hw_set_display_power()/
    // setBrightness() — они ведут к assert(io_handle) в LilyGoDispSPI.
    configure_wake_sources();

    Serial.flush();
    delay(20);
    esp_deep_sleep_start();
    while (true) {}
}

static void wait_for_button_release()
{
    const gpio_num_t pin = (gpio_num_t)SURVIVAL_BTN_GPIO;

    // Возвращаем GPIO в обычный режим, чтобы прочитать уровень.
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = (1ULL << pin);
    cfg.mode         = GPIO_MODE_INPUT;
    cfg.pull_up_en   = SURVIVAL_BTN_ACTIVE_LOW ? GPIO_PULLUP_ENABLE   : GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = SURVIVAL_BTN_ACTIVE_LOW ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;
    cfg.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&cfg);

    const int pressed_level = SURVIVAL_BTN_ACTIVE_LOW ? 0 : 1;
    uint32_t t0 = millis();

    // Ждём, пока линия PMU_INT вернётся в неактивное состояние.
    // Дополнительно чистим IRQ-статус PMU, чтобы следующий вход в sleep
    // не сработал мгновенно на «залипшем» флаге.
    while (digitalRead(pin) == pressed_level && (millis() - t0) < 3000) {
        delay(5);
    }

    if (hw_get_device_online() & HW_PMU_ONLINE) {
        instance.pmu.clearIrqStatus();
    }
}

// ============================================================================
//  Public API
// ============================================================================
bool survival::isArmed() { return nvs_is_armed(); }

void survival::armAndReboot()
{
    nvs_set_armed(true);
    delay(50);
    esp_restart();
    while (true) {}
}

void survival::disarmFromPhone()
{
    nvs_set_armed(false);
}

static bool wait_for_usb_cdc(uint32_t timeout_ms = 8000)
{
    Serial.begin(115200);

    // Некоторые версии Arduino-ESP32 требуют первичного "пинка" USB,
    // чтобы peripheral начал переподключение.
    delay(50);

    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < timeout_ms) {
        delay(20);
    }
    return (bool)Serial;
}

static void serial_cold_start(const char* tag)
{
    // Всё, что здесь печатается, будет потеряно, пока хост не подключился.
    // Поэтому сначала дожидаемся, потом уже пишем.
    bool online = wait_for_usb_cdc(8000);

    // Ещё немного времени хосту, чтобы монитор успел открыть порт.
    delay(200);

    if (!online) {
        // Хоста нет — нет смысла что-то печатать, всё уйдёт в FIFO и пропадёт.
        // Отдаём сигнал «я тут» вибрацией и уходим спать.
        return;
    }

    for (int i = 0; i < 5; ++i) {
        Serial.printf("[SURV] %s ready %d (wake cause=%d)\n",
                      tag, i, (int)esp_sleep_get_wakeup_cause());
        Serial.flush();      // важно: HWCDC без flush может копить в буфере
        delay(100);
    }
    Serial.println();
    Serial.flush();
}

// Возвращает true, если кнопку держали >= hold_ms → это запрос на выход.
static bool wait_for_button_release_or_hold(uint32_t hold_ms = 3000)
{
    const gpio_num_t pin = (gpio_num_t)SURVIVAL_BTN_GPIO;

    gpio_config_t cfg = {};
    cfg.pin_bit_mask = (1ULL << pin);
    cfg.mode         = GPIO_MODE_INPUT;
    cfg.pull_up_en   = SURVIVAL_BTN_ACTIVE_LOW ? GPIO_PULLUP_ENABLE   : GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = SURVIVAL_BTN_ACTIVE_LOW ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;
    cfg.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&cfg);

    const int pressed_level = SURVIVAL_BTN_ACTIVE_LOW ? 0 : 1;
    uint32_t t0 = millis();
    bool held = false;

    while (digitalRead(pin) == pressed_level && (millis() - t0) < hold_ms + 500) {
        if (millis() - t0 >= hold_ms) { held = true; break; }
        delay(10);
    }

    // дождаться отпускания, чтобы не поймать ложный wake
    while (digitalRead(pin) == pressed_level) delay(5);

    if (hw_get_device_online() & HW_PMU_ONLINE) {
        instance.pmu.clearIrqStatus();
    }
    return held;
}

void survival::runSurvivalBoot()
{
    // Мы здесь, потому что NVS говорит armed. Эта функция НИКОГДА не должна
    // вернуть управление: иначе setup() пойдёт инициализировать экран, LVGL,
    // приложения и всё, что мы стараемся выключить.

    esp_sleep_wakeup_cause_t wr = esp_sleep_get_wakeup_cause();
    const bool is_button = wake_is_button(wr);

    // ----- Первый boot после арминга (или power-on с установленным флагом) -----
   if (g_sv.magic != SV_MAGIC) {
        memset(&g_sv, 0, sizeof(g_sv));
        g_sv.magic = SV_MAGIC;

        serial_cold_start("ENTER");
        instance.begin();
        setCpuFrequencyMhz(80);          // 40 на S3 невалидна, всё равно будет 80

        g_sv.entry_epoch      = rtc_now_epoch();
        g_sv.last_press_epoch = 0;

        Serial.printf("\n[SURV] ENTER  reason=%s  cpu=%luMHz  batt=%s%%  rtc=%s\n",
                      wake_reason_str(wr),
                      (unsigned long)getCpuFrequencyMhz(),
                      (read_battery_percent() == 0xFFFFFFFFu) ? "?" :
                          String(read_battery_percent()).c_str(),
                      g_sv.entry_epoch ? "ok" : "invalid");

        if (wait_for_button_release_or_hold(3000)) {
            Serial.println("[SURV] HOLD -> exiting survival mode");
            nvs_set_armed(false);
            delay(200);
            esp_restart();
        }

        sleep_now();
    }
    // ----- Пробуждение по кнопке -----
    if (!is_button) {
        // Любой другой wake при активном флаге — просто уходим обратно.
        sleep_now();
    }
    serial_cold_start("WAKE");
    Serial.begin(115200);
    delay(60);
    instance.begin();
    if (!(hw_get_device_online() & HW_PMU_ONLINE)) {
        Serial.println("[SURV] WARN: PMU not online, button wake may not work");
    }
    setCpuFrequencyMhz(40);

    g_sv.boot_count++;

    const uint64_t now_epoch = rtc_now_epoch();
    const uint32_t batt      = read_battery_percent();
    const uint32_t cpu_mhz   = getCpuFrequencyMhz();

    // Двойное нажатие: RTC имеет секундное разрешение, поэтому окно = 1 секунда.
    const bool double_press =
        (g_sv.last_press_epoch > 0) &&
        (now_epoch > 0) &&
        (now_epoch - g_sv.last_press_epoch <= 1);

    // uptime с момента входа в режим
    char up[16] = "?";
    if (g_sv.entry_epoch && now_epoch >= g_sv.entry_epoch) {
        fmt_uptime(up, sizeof(up), now_epoch - g_sv.entry_epoch);
    }

    char batt_str[8];
    if (batt == 0xFFFFFFFFu) strcpy(batt_str, "?");
    else                     snprintf(batt_str, sizeof(batt_str), "%lu", (unsigned long)batt);

    Serial.printf("\n[SURV] boot #%lu  reason=%s  cpu=%luMHz  batt=%s%%  uptime=%s\n",
                  (unsigned long)g_sv.boot_count,
                  wake_reason_str(wr),
                  (unsigned long)cpu_mhz,
                  batt_str,
                  up);

    if (double_press) {
        Serial.println("[SURV] DOUBLE PRESS -> exiting survival mode, rebooting normally");
        nvs_set_armed(false);
        delay(150);
        esp_restart();
        while (true) {}
    }

    g_sv.last_press_epoch = now_epoch;

    wait_for_button_release();
    sleep_now();
}