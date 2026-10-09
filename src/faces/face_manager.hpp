#pragma once
#include <lvgl.h>
#include <vector>
#include <stdint.h>

// Сигнатура колбэка из сгенерированных файлов (Felix Biego).
typedef void (*FaceRegisterCb)(const char* name,
                               const lv_img_dsc_t* preview,
                               lv_obj_t** root);

// Сигнатура update_check_* из сгенерированных файлов.
typedef void (*FaceUpdateFn)(lv_obj_t* parent,
                             int second, int minute, int hour,
                             bool mode, bool am,
                             int day, int month, int year, int weekday,
                             int temp, int icon,
                             int battery, bool connection,
                             int steps, int distance, int kcal,
                             int bpm, int oxygen);

class FaceManager {
public:
    struct Entry {
        char                 name[32]  = {0};
        const lv_img_dsc_t*  preview   = nullptr;
        lv_obj_t*            source    = nullptr;   // где лежат виджеты, когда face не активен
        FaceUpdateFn         update    = nullptr;   // для сгенерированных
        void               (*def_update)() = nullptr; // для дефолтного
    };

    static FaceManager& inst();

    void begin();                                  // идемпотентно
    void addEntry(const Entry& e);

    void mountCurrent(lv_obj_t* container);        // повесить выбранный face в контейнер
    void tick();                                   // вызывается раз в секунду

    int  count()        const { return (int)_entries.size(); }
    int  currentIndex() const { return _current; }
    const Entry& at(int i) const { return _entries[i]; }

    void select(int idx, bool save = true);
    void next();
    void prev();

private:
    FaceManager() = default;

    std::vector<Entry> _entries;
    int        _current  = 0;
    int        _mounted  = -1;
    lv_obj_t*  _container = nullptr;
    lv_timer_t* _timer    = nullptr;
    bool        _started  = false;

    void _unmountActive();
    void _mountIndex(int idx);
};