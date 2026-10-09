#include "BleService.h"
#include "Protocol.h"         
#include "Database.h"
#include "PowerManager.h"
#include "AppManager.h"
#include "CrashLog.h"
#include "hal/hw.h"            
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <freertos/queue.h>

static NimBLEServer*         s_srv  {nullptr};
static NimBLECharacteristic* s_tx   {nullptr};
static NimBLECharacteristic* s_rx   {nullptr};



struct BleLine { char s[256]; };
static QueueHandle_t s_q  {nullptr};

void ble_set_active(bool on) { /* no-op, активность ведётся через PowerManager */ }

BleService& BleService::inst() { static BleService m; return m; }

class RxCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
        std::string v = c->getValue();
        auto& s = BleService::inst();
        for (char ch : v) {
            if (ch == '\n' || ch == '\r') {
                if (s._line_len > 0) {
                    s._line[s._line_len] = 0;
                    s.feedLine(s._line);
                    s._line_len = 0;
                }
            } else if (s._line_len < sizeof(s._line) - 1) {
                s._line[s._line_len++] = ch;
            }
        }
    }
};
static RxCB s_rx_cb;

void BleService::begin(const char* name)
{
    NimBLEDevice::init(name);
    s_srv = NimBLEDevice::createServer();

    auto* svc = s_srv->createService(proto::SERVICE_UUID);
    s_rx = svc->createCharacteristic(proto::RX_UUID,
              NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    s_tx = svc->createCharacteristic(proto::TX_UUID,
              NIMBLE_PROPERTY::NOTIFY);
    s_rx->setCallbacks(&s_rx_cb);
    // svc->start() удалено — в NimBLE 2.x сервисы стартуют вместе с сервером

    auto* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(proto::SERVICE_UUID);
    adv->enableScanResponse(true);          // было setScanResponse(true)
    NimBLEDevice::startAdvertising();

    _active = true;
}

void BleService::end()
{
    NimBLEDevice::deinit(true);
    _active = false;
}

void BleService::_sendJson(JsonDocument& doc)
{
    if (!_active || !s_tx) return;
    String out;
    serializeJson(doc, out);
    out += '\n';
    s_tx->setValue(reinterpret_cast<const uint8_t*>(out.c_str()), out.length());
    s_tx->notify();
}

void BleService::feedLine(const char* s)
{
    if (!s_q) s_q = xQueueCreate(6, sizeof(BleLine));   
    BleLine l{}; strlcpy(l.s, s, sizeof(l.s));
    xQueueSend(s_q, &l, 0);                            
}

void BleService::poll()                                   // из loop(), т.е. из основного потока
{
    if (!s_q) return;
    BleLine l;
    while (xQueueReceive(s_q, &l, 0) == pdTRUE) {
        CRASH_MARK("ble.cmd");
        _handleLine(l.s);
    }
}

// events...
void BleService::eventStep(uint32_t total) {
    JsonDocument d; d["ev"]="step"; d["total"]=total; _sendJson(d);
}
void BleService::eventBattery(uint8_t pct, bool low) {
    JsonDocument d; d["ev"]="batt"; d["pct"]=pct; d["low"]=low; _sendJson(d);
}
void BleService::eventTrainingStart(const char* t, uint32_t ts) {
    JsonDocument d; d["ev"]="training_start"; d["type"]=t; d["ts"]=ts; _sendJson(d);
}
void BleService::eventTrainingEnd(uint32_t id, uint32_t s, uint32_t dist, uint32_t dur) {
    JsonDocument d; d["ev"]="training_end";
    d["id"]=id; d["steps"]=s; d["dist"]=dist; d["dur"]=dur; _sendJson(d);
}
void BleService::eventAlarm(uint8_t idx) {
    JsonDocument d; d["ev"]="alarm"; d["idx"]=idx; _sendJson(d);
}

// --- реплай ---
static void reply(JsonDocument& d, int id, bool ok)
{
    d["id"] = id; d["ok"] = ok;
    BleService::inst().sendJson(d);
}

void BleService::_handleLine(const char* s)
{
    JsonDocument d;
    if (deserializeJson(d, s) != DeserializationError::Ok) {
        JsonDocument r; r["ok"]=false; r["err"]="json"; sendJson(r);
        return;
    }

    int id = d["id"] | 0;
    const char* cmd = d["cmd"] | "";
    auto& db = Database::inst();
    auto& cfg = db.config();

    if (!strcmp(cmd, "ping")) {
        JsonDocument r;
        r["fw"]   = proto::FIRMWARE_VER;
        r["batt"] = PowerManager::inst().batteryPct();
        reply(r, id, true);
    }
    else if (!strcmp(cmd, "wifi")) {
        const char* ssid = d["ssid"] | "";
        const char* pwd  = d["pwd"]  | "";
        strlcpy(cfg.wifi_ssid, ssid, sizeof(cfg.wifi_ssid));
        strlcpy(cfg.wifi_pwd,  pwd,  sizeof(cfg.wifi_pwd));
        cfg.wifi_on = true;
        db.saveConfig();

        wifi_conn_params_t p;
        p.ssid = ssid; p.password = pwd;
        hw_set_wifi_connect(p);

        JsonDocument r; reply(r, id, true);
    }
    else if (!strcmp(cmd, "wifi_off")) {
        WiFi.disconnect(true);
        cfg.wifi_on = false; db.saveConfig();
        JsonDocument r; reply(r, id, true);
    }
    else if (!strcmp(cmd, "bt")) {
        bool on = d["on"] | true;
        cfg.bt_on = on; db.saveConfig();
        JsonDocument r; reply(r, id, true);
        if (!on) {
            lv_timer_t* t = lv_timer_create([](lv_timer_t* t){
                BleService::inst().end();
                PowerManager::inst().setBtActive(false);
                lv_timer_del(t);
            }, 300, nullptr);
            lv_timer_set_repeat_count(t, 1);
        }
    }
    else if (!strcmp(cmd, "city")) {
        strlcpy(cfg.city, d["name"] | "Moscow", sizeof(cfg.city));
        cfg.lat = d["lat"] | 55.75f;
        cfg.lon = d["lon"] | 37.62f;
        db.saveConfig();
        JsonDocument r; reply(r, id, true);
    }
    else if (!strcmp(cmd, "brightness")) {
        cfg.brightness = d["v"] | 128;
        hw_set_disp_backlight(cfg.brightness);
        db.saveConfig();
        JsonDocument r; reply(r, id, true);
    }
    else if (!strcmp(cmd, "timeout")) {
        cfg.disp_timeout_s = d["s"] | 15;
        PowerManager::inst().setScreenTimeout(cfg.disp_timeout_s * 1000UL);
        db.saveConfig();
        JsonDocument r; reply(r, id, true);
    }
    else if (!strcmp(cmd, "alarm_set")) {
        int idx = d["idx"] | 0;
        if (idx >= 0 && idx < 5) {
            cfg.alarms[idx].enabled   = d["en"] | false;
            cfg.alarms[idx].hour      = d["h"]  | 0;
            cfg.alarms[idx].minute    = d["m"]  | 0;
            cfg.alarms[idx].days_mask = d["days"] | 0;
            db.saveConfig();
            JsonDocument r; reply(r, id, true);
        } else {
            JsonDocument r; reply(r, id, false);
        }
    }
    else if (!strcmp(cmd, "alarm_get")) {
        JsonDocument r;
        JsonArray arr = r["alarms"].to<JsonArray>();
        for (int i = 0; i < 5; i++) {
            JsonObject o = arr.add<JsonObject>();
            o["idx"]=i;
            o["en"] =cfg.alarms[i].enabled;
            o["h"]  =cfg.alarms[i].hour;
            o["m"]  =cfg.alarms[i].minute;
            o["days"]=cfg.alarms[i].days_mask;
        }
        reply(r, id, true);
    }
    else if (!strcmp(cmd, "notify")) {
        extern void app_notifications_push(const char*, const char*, const char*);
        app_notifications_push(d["app"] | "App", d["title"] | "", d["body"] | "");
        JsonDocument r; reply(r, id, true);
    }
    else if (!strcmp(cmd, "steps")) {
        uint16_t days = d["days"] | 7;
        auto list = db.stepsRange(days);
        JsonDocument r;
        JsonArray arr = r["days"].to<JsonArray>();
        for (auto& sd : list) {
            JsonObject o = arr.add<JsonObject>();
            o["ymd"] = sd.ymd; o["steps"] = sd.steps;
        }
        reply(r, id, true);
    }
    else if (!strcmp(cmd, "training_list")) {
        auto list = db.trainingList();
        JsonDocument r;
        JsonArray arr = r["items"].to<JsonArray>();
        for (auto& t : list) {
            JsonObject o = arr.add<JsonObject>();
            o["id"]=t.id; o["type"]=t.type; o["start"]=t.start_ts;
            o["dur"]=t.duration_s; o["steps"]=t.steps; o["dist"]=t.distance_m;
        }
        reply(r, id, true);
    }
    else if (!strcmp(cmd, "training_clear")) {
        db.trainingClear(d["id"] | 0);
        JsonDocument r; reply(r, id, true);
    }
    else if (!strcmp(cmd, "time_sync")) {
        time_t ts = d["unix"] | 0;
        struct tm tm; gmtime_r(&ts, &tm);
        if (hw_get_device_online() & HW_RTC_ONLINE) {
            instance.rtc.convertUtcToTimezone(tm, GMT_OFFSET_SECOND);
            instance.rtc.setDateTime(tm);
        }
        JsonDocument r; reply(r, id, true);
    }
    else {
        JsonDocument r; r["err"] = "unknown cmd";
        reply(r, id, false);
    }
}