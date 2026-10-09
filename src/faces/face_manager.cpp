#include "face_manager.hpp"
#include "face_default.hpp"
#include "core/Database.h"
#include "core/PowerManager.h"
#include "hal/hw.h"

// =============================================================================
//  СГЕНЕРИРОВАННЫЕ ЦИФЕРБЛАТЫ
//  Раскомментируй те, что у тебя уже лежат в src/faces/
// =============================================================================
#include "756_2/756_2.h"
// #include "34_2_dial/34_2_dial.h"
// #include "75_2_dial/75_2_dial.h"
// #include "79_2_dial/79_2_dial.h"
// #include "116_2_dial/116_2_dial.h"
// #include "b_w_resized/b_w_resized.h"
// #include "kenya/kenya.h"
// #include "pixel_resized/pixel_resized.h"
// #include "radar/radar.h"
// #include "smart_resized/smart_resized.h"
// #include "tix_resized/tix_resized.h"
// #include "wfb_resized/wfb_resized.h"

extern "C" void onFaceEvent(lv_event_t*) {} // @note monkey patch

FaceManager& FaceManager::inst() { static FaceManager m; return m; }


// ---------------------------------------------------------------------------
//  helpers
// ---------------------------------------------------------------------------
static void move_children(lv_obj_t* from, lv_obj_t* to)
{
    if (!from || !to || from == to) return;
    while (lv_obj_get_child_count(from) > 0) {
        lv_obj_t* c = lv_obj_get_child(from, 0);
        lv_obj_set_parent(c, to);
    }
}

namespace {
    struct RegBuf {
        char                  name[32] = {0};
        const lv_img_dsc_t*   preview  = nullptr;
        lv_obj_t*             screen   = nullptr;
    };
    RegBuf* g_buf = nullptr;

    void gen_cb(const char* name, const lv_img_dsc_t* preview, lv_obj_t** root)
    {
        if (!g_buf) return;
        strlcpy(g_buf->name, name ? name : "", sizeof(g_buf->name));
        g_buf->preview = preview;
        g_buf->screen  = root ? *root : nullptr;
    }
}

static void register_generated(const char* fallback_name,
                               void (*init_fn)(FaceRegisterCb),
                               FaceUpdateFn update_fn)
{
    if (!init_fn) return;

    RegBuf r;
    g_buf = &r;
    init_fn(gen_cb);
    g_buf = nullptr;

    FaceManager::Entry e;
    strlcpy(e.name, r.name[0] ? r.name : fallback_name, sizeof(e.name));
    e.preview    = r.preview;
    e.source     = r.screen;
    e.update     = update_fn;
    e.def_update = nullptr;
    FaceManager::inst().addEntry(e);
}


// ---------------------------------------------------------------------------
//  begin
// ---------------------------------------------------------------------------
void FaceManager::begin()
{
    if (_started) return;
    _started = true;

    // ---------- 0: дефолтный face (твой существующий локскрин) ----------
    {
        Entry e;
        strlcpy(e.name, "Default", sizeof(e.name));
        e.source     = lv_obj_create(nullptr);   // создаём отдельный screen как «хранилище»
        e.preview    = nullptr;
        e.update     = nullptr;
        e.def_update = face_default_update;
        face_default_build(e.source);
        addEntry(e);
    }

    // ---------- 1..N: сгенерированные ----------
    register_generated("756_2", init_face_756_2, update_check_756_2);
    // register_generated("34_2_dial",  init_face_34_2_dial,  update_check_34_2_dial);
    // register_generated("75_2_dial",  init_face_75_2_dial,  update_check_75_2_dial);

    // Восстановить выбор из конфига
    _current = Database::inst().config().face_idx;
    if (_current < 0 || _current >= (int)_entries.size()) _current = 0;

    // Тик раз в секунду
    _timer = lv_timer_create([](lv_timer_t*){
        FaceManager::inst().tick();
    }, 1000, nullptr);
}


void FaceManager::addEntry(const Entry& e) { _entries.push_back(e); }


// ---------------------------------------------------------------------------
//  mount / unmount
// ---------------------------------------------------------------------------
void FaceManager::mountCurrent(lv_obj_t* container)
{
    if (!container) return;
    _container = container;
    _mountIndex(_current);
}

void FaceManager::_unmountActive()
{
    if (_mounted < 0 || _mounted >= (int)_entries.size()) return;
    Entry& e = _entries[_mounted];
    if (!e.source || !_container) return;
    move_children(_container, e.source);
    _mounted = -1;
}

void FaceManager::_mountIndex(int idx)
{
    if (!_container) return;
    if (idx < 0 || idx >= (int)_entries.size()) return;
    if (_mounted == idx) return;

    _unmountActive();

    Entry& e = _entries[idx];
    if (e.source) {
        move_children(e.source, _container);
    } else {
        Serial.printf("[Face] no source for '%s'\n", e.name);
    }
    _mounted = idx;
    _current = idx;
    tick();   // сразу обновим значения
}


// ---------------------------------------------------------------------------
//  selection
// ---------------------------------------------------------------------------
void FaceManager::select(int idx, bool save)
{
    if (idx < 0 || idx >= (int)_entries.size()) return;
    _mountIndex(idx);
    if (save) {
        Database::inst().config().face_idx = (uint8_t)idx;
        Database::inst().saveConfig();
    }
}

void FaceManager::next()
{
    int n = (int)_entries.size();
    if (n <= 0) return;
    select((_current + 1) % n);
}

void FaceManager::prev()
{
    int n = (int)_entries.size();
    if (n <= 0) return;
    select((_current - 1 + n) % n);
}


// ---------------------------------------------------------------------------
//  tick
// ---------------------------------------------------------------------------
void FaceManager::tick()
{
    if (_mounted < 0 || _mounted >= (int)_entries.size()) return;
    Entry& e = _entries[_mounted];

    if (e.def_update) { e.def_update(); return; }
    if (!e.update || !e.source) return;

    struct tm t; hw_get_date_time(t);

    monitor_params_t mp;
    hw_get_monitor_params(mp);

    uint32_t steps = Database::inst().stepsToday();
    int distance_m = (int)(steps * 0.75f);     // ~75 см/шаг
    int kcal       = (int)(steps * 0.04f);     // грубо

    int  battery   = mp.battery_percent;
    bool connected = PowerManager::inst().isBtActive() || hw_get_wifi_connected();

    bool is24h = true;                          // TODO: из конфига, если появится
    bool am    = t.tm_hour < 12;

    // Погода/пульс/SpO2 в проекте пока не интегрированы — отдаём нули.
    // Когда захочешь — можно кэшировать данные WeatherApp в Database.
    int temp = 0, icon = 0, bpm = 0, oxygen = 0;

    e.update(e.source,
             t.tm_sec, t.tm_min, t.tm_hour,
             is24h, am,
             t.tm_mday, t.tm_mon + 1, t.tm_year + 1900, t.tm_wday,
             temp, icon, battery, connected,
             (int)steps, distance_m, kcal, bpm, oxygen);
}