#include "Database.h"
#include <FFat.h>
#include <ArduinoJson.h>
#include "PowerManager.h"

static const char* F_CFG   = "/cfg.json";
static const char* F_STEPS = "/steps.json";
static const char* F_TRAIN = "/train.json";

Database& Database::inst() { static Database m; return m; }

void Database::_applyDefaults()
{
    _cfg = UserConfig{};
    strncpy(_cfg.city, "Moscow", sizeof(_cfg.city)-1);
    _cfg.lat = 55.75f; _cfg.lon = 37.62f;
    _cfg.brightness    = 128;
    _cfg.keyboard_bl   = 128;
    _cfg.disp_timeout_s = 15;
    _cfg.bt_on   = true;
    _cfg.wifi_on = true;
    strlcpy(_cfg.wifi_ssid, "JetLegacy_Bus", sizeof(_cfg.wifi_ssid));  // ←
    strlcpy(_cfg.wifi_pwd,  "50504022",      sizeof(_cfg.wifi_pwd));   // ←
    for (int i=0;i<5;i++) _cfg.alarms[i].enabled = false;
    _cfg.face_idx = 0;
}

void Database::begin()
{
    if (!FFat.begin(true)) {
        Serial.println("[DB] FFat mount failed");
    }
    _applyDefaults();

    // cfg
    File f = FFat.open(F_CFG, FILE_READ);
    if (f) {
        JsonDocument doc;
        if (deserializeJson(doc, f) == DeserializationError::Ok) {
            strlcpy(_cfg.city, doc["city"] | "Moscow", sizeof(_cfg.city));
            _cfg.lat = doc["lat"] | 55.75f;
            _cfg.lon = doc["lon"] | 37.62f;
            _cfg.brightness     = doc["brightness"] | 128;
            _cfg.keyboard_bl    = doc["kb_bl"] | 128;
            _cfg.disp_timeout_s = doc["timeout"] | 15;
            _cfg.bt_on = doc["bt_on"] | true;
            _cfg.wifi_on = doc["wifi_on"] | false;
            _cfg.face_idx = doc["face_idx"] | 0;
            strlcpy(_cfg.wifi_ssid, doc["wifi_ssid"] | "", sizeof(_cfg.wifi_ssid));
            strlcpy(_cfg.wifi_pwd,  doc["wifi_pwd"]  | "", sizeof(_cfg.wifi_pwd));
            JsonArray arr = doc["alarms"].as<JsonArray>();
            int i=0;
            for (JsonObject a : arr) {
                if (i>=5) break;
                _cfg.alarms[i].enabled  = a["en"] | false;
                _cfg.alarms[i].hour     = a["h"]  | 0;
                _cfg.alarms[i].minute   = a["m"]  | 0;
                _cfg.alarms[i].days_mask= a["d"]  | 0;
                i++;
            }
        }
        f.close();
    }
    _loadStepsFile();

    // rollover по дню
    struct tm t; hw_get_date_time(t);
    snprintf(_today_ymd, sizeof(_today_ymd), "%04d-%02d-%02d",
             t.tm_year+1900, t.tm_mon+1, t.tm_mday);
}

void Database::saveConfig()
{
    JsonDocument doc;
    doc["city"]      = _cfg.city;
    doc["lat"]       = _cfg.lat;
    doc["lon"]       = _cfg.lon;
    doc["brightness"]= _cfg.brightness;
    doc["kb_bl"]     = _cfg.keyboard_bl;
    doc["timeout"]   = _cfg.disp_timeout_s;
    doc["bt_on"]     = _cfg.bt_on;
    doc["wifi_on"]   = _cfg.wifi_on;
    doc["wifi_ssid"] = _cfg.wifi_ssid;
    doc["wifi_pwd"]  = _cfg.wifi_pwd;
    doc["face_idx"] = _cfg.face_idx;

    JsonArray al = doc["alarms"].to<JsonArray>();
    for (int i=0;i<5;i++) {
        JsonObject a = al.add<JsonObject>();
        a["en"] = _cfg.alarms[i].enabled;
        a["h"]  = _cfg.alarms[i].hour;
        a["m"]  = _cfg.alarms[i].minute;
        a["d"]  = _cfg.alarms[i].days_mask;
    }

    File f = FFat.open(F_CFG, FILE_WRITE);
    if (f) { serializeJson(doc, f); f.close(); }
}

void Database::_loadStepsFile()
{
    File f = FFat.open(F_STEPS, FILE_READ);
    if (!f) return;
    JsonDocument doc;
    if (deserializeJson(doc, f) == DeserializationError::Ok) {
        struct tm t; hw_get_date_time(t);
        char ymd[11];
        snprintf(ymd, sizeof(ymd), "%04d-%02d-%02d",
                 t.tm_year+1900, t.tm_mon+1, t.tm_mday);
        _steps_today = doc[ymd] | 0;
    }
    f.close();
}

void Database::_saveStepsFile()
{
    // Простое хранилище: ymd -> steps, максимум 30 дней
    JsonDocument doc;
    File f = FFat.open(F_STEPS, FILE_READ);
    if (f) { deserializeJson(doc, f); f.close(); }

    // Обрезаем старые, если больше 30
    if (doc.size() > 30) {
        // удаляем самые старые по строковому сравнению
        std::vector<String> keys;
        for (JsonPair kv : doc.as<JsonObject>()) keys.push_back(kv.key().c_str());
        std::sort(keys.begin(), keys.end());
        while (keys.size() > 30) {
            doc.remove(keys.front());
            keys.erase(keys.begin());
        }
    }
    doc[_today_ymd] = _steps_today;

    f = FFat.open(F_STEPS, FILE_WRITE);
    if (f) { serializeJson(doc, f); f.close(); }
}

void Database::_rolloverDay()
{
    struct tm t; hw_get_date_time(t);
    char ymd[11];
    snprintf(ymd, sizeof(ymd), "%04d-%02d-%02d",
             t.tm_year+1900, t.tm_mon+1, t.tm_mday);
    if (strcmp(ymd, _today_ymd) != 0) {
        _saveStepsFile();
        strlcpy(_today_ymd, ymd, sizeof(_today_ymd));
        _steps_today = 0;
    }
}

void Database::addSteps(uint32_t delta)
{
    _rolloverDay();
    _steps_today += delta;
}

uint32_t Database::stepsToday()
{
    _rolloverDay();
    return _steps_today;
}

void Database::flushSteps()
{
    _saveStepsFile();
}

std::vector<StepDay> Database::stepsRange(uint16_t days_back)
{
    std::vector<StepDay> out;
    File f = FFat.open(F_STEPS, FILE_READ);
    if (!f) return out;
    JsonDocument doc;
    if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return out; }
    f.close();

    std::vector<String> keys;
    for (JsonPair kv : doc.as<JsonObject>()) keys.push_back(kv.key().c_str());
    std::sort(keys.begin(), keys.end());
    if (keys.size() > days_back) keys.erase(keys.begin(), keys.end() - days_back);

    for (auto& k : keys) {
        StepDay d{};
        strlcpy(d.ymd, k.c_str(), sizeof(d.ymd));
        d.steps = doc[k.c_str()] | 0;
        out.push_back(d);
    }
    return out;
}

// --- Trainings ---

uint32_t Database::trainingStart(const char* type)
{
    return _next_training_id++;
}

void Database::trainingStop(uint32_t id, uint32_t dur_s, uint32_t steps, uint32_t dist_m)
{
    JsonDocument doc;
    File f = FFat.open(F_TRAIN, FILE_READ);
    if (f) { deserializeJson(doc, f); f.close(); }

    JsonArray arr = doc["items"].to<JsonArray>();
    JsonObject o = arr.add<JsonObject>();
    o["id"]    = id;
    o["type"]  = "walk";
    o["start"] = (uint32_t)time(nullptr);
    o["dur"]   = dur_s;
    o["steps"] = steps;
    o["dist"]  = dist_m;

    // ограничиваем 200 записей
    while (arr.size() > 200) arr.remove(0);

    f = FFat.open(F_TRAIN, FILE_WRITE);
    if (f) { serializeJson(doc, f); f.close(); }
}

std::vector<TrainingRecord> Database::trainingList()
{
    std::vector<TrainingRecord> out;
    File f = FFat.open(F_TRAIN, FILE_READ);
    if (!f) return out;
    JsonDocument doc;
    if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return out; }
    f.close();

    for (JsonObject o : doc["items"].as<JsonArray>()) {
        TrainingRecord r{};
        r.id         = o["id"]    | 0u;
        strlcpy(r.type, o["type"] | "walk", sizeof(r.type));
        r.start_ts   = o["start"] | 0u;
        r.duration_s = o["dur"]   | 0u;
        r.steps      = o["steps"] | 0u;
        r.distance_m = o["dist"]  | 0u;
        out.push_back(r);
    }
    return out;
}

void Database::trainingClear(uint32_t id)
{
    JsonDocument doc;
    File f = FFat.open(F_TRAIN, FILE_READ);
    if (f) { deserializeJson(doc, f); f.close(); }

    if (id == 0) {
        doc["items"].clear();
    } else {
        JsonArray arr = doc["items"].as<JsonArray>();
        JsonArray keep = doc["items"].to<JsonArray>();
        keep.clear();
        for (JsonObject o : arr) {
            if ((o["id"] | 0u) != id) keep.add(o);
        }
    }
    f = FFat.open(F_TRAIN, FILE_WRITE);
    if (f) { serializeJson(doc, f); f.close(); }
}