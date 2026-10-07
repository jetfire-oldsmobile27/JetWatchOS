#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

class RxCB;   // forward

class BleService {
public:
    static BleService& inst();
    void begin(const char* name);
    void end();
    bool active() const { return _active; }
    void poll();

    void eventStep(uint32_t total);
    void eventBattery(uint8_t pct, bool low);
    void eventTrainingStart(const char* type, uint32_t ts);
    void eventTrainingEnd(uint32_t id, uint32_t steps, uint32_t dist, uint32_t dur);
    void eventAlarm(uint8_t idx);

    // Публичный шлюз для RxCB
    void feedLine(const char* s); 
    void sendJson(JsonDocument& doc) { _sendJson(doc); }

private:
    friend class RxCB;
    bool _active = false;
    char _line[512] = {0};
    size_t _line_len = 0;

    void _handleLine(const char* s);
    void _sendJson(JsonDocument& doc);
};