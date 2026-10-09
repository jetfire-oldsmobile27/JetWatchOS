#include "hal/hw.h"        // ← первым

#include <LV_Helper.h>
#include <FFat.h>
#include "core/CrashLog.h"
#include "ui/ui_common.h"
#include "core/AppManager.h"
#include "core/PowerManager.h"
#include "core/Database.h"
#include "core/BleService.h"
#include "core/Protocol.h"
#include "apps/apps.h"

#include <expected>


// ============================================================================
//  BMA423 — педометр через IRQ (SENSOR_STEPS_UPDATED)
// ============================================================================
//  Опрос getPedometerCounter() в loop() — плохо:
//    * теряются события между опросами;
//    * на переполнении 0xFFFFFF -> 0 дельта считается неверно;
//    * параллельное чтение сбивает внутреннее окно счётчика в LilyGoLib.
//  Слушаем событие и считаем дельту через bma_step_delta().
//
#if defined(USING_BMA423_SENSOR)

#define BMA_STEP_MASK       0x00FFFFFFUL   // счётчик BMA423 — 24-битный
#define BMA_STEP_MAX_DELTA  64u            // отсечка мусора I²C

static volatile uint32_t s_last_bma_steps = 0;
static bool              s_bma_ready      = false;


static bool bma_start()
{
    uint32_t online = hw_get_device_online();
    Serial.printf("[BMA] online mask=0x%08lX, bma bit=%d\n",
                  (unsigned long)online, (online & HW_BMA423_ONLINE) ? 1 : 0);

    if (!(online & HW_BMA423_ONLINE)) {
        Serial.println("[BMA] NOT ONLINE: проверь бит для BMA423 в LilyGoLib");
        return false;
    }

    bool a = instance.sensor.configAccelerometer();
    bool b = instance.sensor.enableAccelerometer();
    bool c = instance.sensor.enablePedometer();
    instance.sensor.enablePedometerIRQ();

    Serial.printf("[BMA] cfg=%d en=%d ped=%d\n", (int)a, (int)b, (int)c);

    s_last_bma_steps = instance.sensor.getPedometerCounter() & BMA_STEP_MASK;
    Serial.printf("[BMA] counter at boot=%lu\n",
                  (unsigned long)s_last_bma_steps);

    return a && b && c;
}


// Корректная дельта 24-битного счётчика с учётом переполнения
static inline uint32_t bma_step_delta(uint32_t cur)
{
    uint32_t prev = s_last_bma_steps;
    uint32_t delta;

    if (cur >= prev) {
        delta = cur - prev;
    } else {
        delta = (BMA_STEP_MASK + 1u) - prev + cur;   // 0xFFFFFF -> 0x000000
    }
    s_last_bma_steps = cur;
    return delta;
}


static void on_sensor_event(DeviceEvent_t event, void* params, void*)
{
    if (event != SENSOR_EVENT) return;

    switch (instance.getSensorEventType(params)) {

    case SENSOR_STEPS_UPDATED: {
        if (!s_bma_ready) return;

        uint32_t cur   = instance.sensor.getPedometerCounter() & BMA_STEP_MASK;
        uint32_t delta = bma_step_delta(cur);

        if (delta == 0)                       return;
        if (delta > BMA_STEP_MAX_DELTA) {
            Serial.printf("[BMA] bogus delta=%lu ignored (cur=%lu)\n",
                          (unsigned long)delta, (unsigned long)cur);
            return;
        }

        Database::inst().addSteps(delta);

        // BLE-нотификация — не чаще раза в минуту
        static uint32_t last_notify = 0;
        uint32_t now = lv_tick_get();
        if (now - last_notify > 60000) {
            last_notify = now;
            BleService::inst().eventStep(Database::inst().stepsToday());
        }
        break;
    }

    case SENSOR_ACTIVITY_DETECTED:
    case SENSOR_TILT_DETECTED:
    case SENSOR_DOUBLE_TAP_DETECTED:
    case SENSOR_ANY_MOTION_DETECTED:
    default:
        break;
    }
}

#endif  // USING_BMA423_SENSOR


// ============================================================================
//  PMU (коронка)
// ============================================================================
static void on_pmu_event(DeviceEvent_t, void* params, void*)
{
    if (instance.getPMUEventType(params) != PMU_EVENT_KEY_CLICKED) return;
    AppManager::inst().onCrownClick();
}


// ============================================================================
//  Регистрация приложений
// ============================================================================
static void register_apps()
{
    auto& m = AppManager::inst();
    m.registerApp("Notifications", &img_notify,     app_notifications_make);
    m.registerApp("Weather",       &img_weather,    app_weather_make);
    m.registerApp("Steps",         &img_monitoring, app_pedometer_make);
    m.registerApp("Training",      &img_run,        app_training_make);
    m.registerApp("Alarm",         &img_alarm,      app_alarm_make);
    m.registerApp("Stopwatch",     &img_timer,      app_stopwatch_make);
    m.registerApp("Music",         &img_music,      app_music_make);
    m.registerApp("Flashlight",    &img_flash,      app_flashlight_make);
    m.registerApp("Terminal",      &img_test,       app_terminal_make);
    m.registerApp("Settings",      &img_settings,   app_settings_make);
}

// ============================================================================
//  Синхронизация RTC с хоста по Serial (post-flash hook)
//  Формат:  "TS YYYY MM DD hh mm ss\n"  — локальное время
// ============================================================================
static void poll_serial_time_sync()
{
    static char   buf[48];
    static size_t len = 0;

    while (Serial.available()) {
        int ci = Serial.read();
        if (ci < 0) break;
        char c = (char)ci;

        if (c == '\r') continue;

        if (c == '\n') {
            buf[len] = 0;
            if (len > 3 && !strncmp(buf, "TS ", 3)) {
                int y, mo, d, h, mi, s;
                if (sscanf(buf + 3, "%d %d %d %d %d %d",
                           &y, &mo, &d, &h, &mi, &s) == 6
                    && y >= 2020 && y <= 2099
                    && mo >= 1 && mo <= 12
                    && d  >= 1 && d  <= 31
                    && h  >= 0 && h  <= 23
                    && mi >= 0 && mi <= 59
                    && s  >= 0 && s  <= 60)
                {
                    struct tm t = {};
                    t.tm_year = y  - 1900;
                    t.tm_mon  = mo - 1;
                    t.tm_mday = d;
                    t.tm_hour = h;
                    t.tm_min  = mi;
                    t.tm_sec  = s;
                    hw_rtc_set_tm(t);
                    Serial.printf("[TIME] RTC set: "
                                  "%04d-%02d-%02d %02d:%02d:%02d\n",
                                  y, mo, d, h, mi, s);
                } else {
                    Serial.println("[TIME] bad payload");
                }
            }
            len = 0;
        } else if (len < sizeof(buf) - 1) {
            buf[len++] = c;
        } else {
            len = 0;   // overflow — сбрасываем буфер
        }
    }
}


// ============================================================================
//  setup / loop
// ============================================================================
void setup()
{
    Serial.begin(115200);
    crash_init();

    instance.begin();
    beginLvglHelper(instance);

    Database::inst().begin();
    crash_persist();
    PowerManager::inst().begin();
    register_apps();
    AppManager::inst().begin();

#if defined(USING_BMA423_SENSOR)
    // Обработчик — ДО bma_start(), чтобы не потерять первый IRQ
    instance.onEvent(on_sensor_event, SENSOR_EVENT, nullptr);

    s_bma_ready = bma_start();
    Serial.printf("[BMA] ready=%d\n", (int)s_bma_ready);
#endif

    auto& cfg = Database::inst().config();
    if (cfg.wifi_on && cfg.wifi_ssid[0]) {
        wifi_conn_params_t p;
        p.ssid = cfg.wifi_ssid;
        p.password = cfg.wifi_pwd;
        hw_set_wifi_connect(p);
    }

    if (cfg.bt_on) {
        BleService::inst().begin(proto::DEVICE_NAME);
        PowerManager::inst().setBtActive(true);
    }

    instance.onEvent(on_pmu_event, POWER_EVENT, nullptr);
    hw_set_disp_backlight(cfg.brightness);
    PowerManager::inst().resetActivity();
}


void loop()
{
    static uint32_t last_db_flush = 0;
    static uint32_t last_pm_tick  = 0;

    poll_serial_time_sync();

    CRASH_MARK("instance.loop");      instance.loop();
    CRASH_MARK("lv_task");            lv_task_handler();
    CRASH_MARK("power_manager.poll"); PowerManager::inst().poll();
    CRASH_MARK("app.tick");           AppManager::inst().tick();
    crash_tick();

    uint32_t now = lv_tick_get();
    if (now - last_pm_tick > 1000) {
        last_pm_tick = now;
        PowerManager::inst().tick();
    }
    if (now - last_db_flush > 60000) {
        last_db_flush = now;
        Database::inst().flushSteps();
    }

    // Опрос педометра убран — всё приходит через on_sensor_event()
    BleService::inst().poll();
    delay(5);
}

