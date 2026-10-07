#pragma once
void crash_init();                   // первым делом после Serial.begin
void crash_persist();                // после Database::begin (FFat смонтирован)
void crash_tick();                   // из loop()
void crash_mark(const char* where);  // "хлебные крошки"
#define CRASH_MARK(s) crash_mark(s)