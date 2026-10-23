#pragma once
#include <Arduino.h>
#include <cstdint>
#include <vector>
#include <time.h>

struct StepDay {
    char ymd[11];       // "2026-10-07"
    uint32_t steps;
};

struct TrainingRecord {
    uint32_t id;        // монотонный
    char type[12];      // "walk", "run", ...
    uint32_t start_ts;  // unix
    uint32_t duration_s;
    uint32_t steps;
    uint32_t distance_m;
};

struct AlarmEntry {
    bool enabled;
    uint8_t hour;
    uint8_t minute;
    uint8_t days_mask;   // bit0=Mon ... bit6=Sun
};

struct UserConfig {
    char city[32];
    float lat, lon;
    uint8_t brightness;      // 0..255
    uint8_t keyboard_bl;     // 0..255
    uint16_t disp_timeout_s; // 0 = never
    bool bt_on;
    char wifi_ssid[33];
    char wifi_pwd[65];
    bool wifi_on;
    AlarmEntry alarms[5];
    uint8_t face_idx = 0;
    bool    gps_on        = true;
    bool    gps_time_sync = false; 
    int16_t tz_offset_min = 180; // +3:00
    bool sounds_on = true;
    bool power_save = false;
    uint16_t goal{10'000}; // steps target
};

class Database {
public:
    static Database& inst();
    void begin();     // монтирует FFat

    // --- Steps ---
    void addSteps(uint32_t delta);
    uint32_t stepsToday();
    std::vector<StepDay> stepsRange(uint16_t days_back);
    void flushSteps();                 // записать в файл (раз в минуту)

    // --- Trainings ---
    uint32_t trainingStart(const char* type);
    void trainingStop(uint32_t id, uint32_t duration_s, uint32_t steps, uint32_t dist_m);
    std::vector<TrainingRecord> trainingList();
    void trainingClear(uint32_t id);   // 0 = все

    // --- Config / Alarms ---
    UserConfig& config() { return _cfg; }
    void saveConfig();
    void setStepsToday(uint32_t steps);

    void forceTodayFlush();

private:
    UserConfig _cfg{};
    uint32_t   _steps_today = 0;
    char       _today_ymd[11] = {0};
    uint32_t   _last_flush_ms = 0;
    uint32_t   _next_training_id = 1;

    void _loadStepsFile();
    void _saveStepsFile();
    void _rolloverDay();
    void _applyDefaults();
};