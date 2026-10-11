#pragma once
#include <stdint.h>

class AlarmManager {
public:
    static AlarmManager& inst();

    void begin();                  // вызывать из setup() после Database::begin()
    void tick();                   // из loop() (или из PowerManager::tick)

private:
    AlarmManager() = default;
    int      _last_minute = -1;
    bool     _active      = false;
    int      _active_idx  = -1;

    void _check();
    void _fire(int idx);
    void _dismiss();
};