// src/core/GpsLog.h
#pragma once
#include <Arduino.h>

namespace gpslog {

constexpr int MAX_ENTRIES = 64;

struct Entry {
    uint32_t ms;          // millis() в момент события
    char     text[72];
};

void begin();
void poll();               // дергать из loop()
int  count();
const Entry& at(int i);    // 0 = самый свежий

} // namespace gpslog