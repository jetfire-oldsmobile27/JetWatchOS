#include "AppManager.h"
#include "PowerManager.h"
#include "ui/ui_common.h"
#include "core/CrashLog.h"

extern lv_obj_t* ui_build_lock(lv_obj_t* parent);
extern lv_obj_t* ui_build_menu(lv_obj_t* parent, lv_group_t* g);

AppManager& AppManager::inst() { static AppManager m; return m; }

void AppManager::begin()
{
    ui_init_theme();

    _group = lv_group_create();
    lv_group_set_wrap(_group, false);
    set_default_group(_group);

    _tileview = lv_tileview_create(UI_SCR_ACT());
    lv_obj_set_size(_tileview, LV_PCT(100), LV_PCT(100));
    lv_obj_set_scrollbar_mode(_tileview, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(_tileview, lv_color_black(), 0);
    lv_obj_set_style_radius(_tileview, 0, 0);

    _tile_lock = lv_tileview_add_tile(_tileview, 0, 0, LV_DIR_VER);
    _tile_menu = lv_tileview_add_tile(_tileview, 0, 1, LV_DIR_VER);

    ui_build_lock(_tile_lock);
    ui_build_menu(_tile_menu, _group);

    _applyState(State::Locked);
}

void AppManager::_applyState(State s)
{
    _state = s;
    if (s == State::Locked) {
        lv_tileview_set_tile(_tileview, _tile_lock, LV_ANIM_ON);
    } else if (s == State::Menu) {
        lv_tileview_set_tile(_tileview, _tile_menu, LV_ANIM_ON);
    }
}

void AppManager::showLock() { _applyState(State::Locked); }
void AppManager::unlock()   { _applyState(State::Menu);   }
void AppManager::showMenu() { _applyState(State::Menu);   }

static void on_gesture(lv_event_t* e);

void AppManager::launchApp(std::unique_ptr<App> app)
{
    crash_mark(app->name());
    _closeCurrent();

    _current = std::move(app);

    // Слой приложения — на активный ЭКРАН, поверх tileview.
    // Маска скругления висит на lv_layer_top(), значит останется сверху.
    _app_layer = lv_obj_create(lv_screen_active());
    lv_obj_set_size(_app_layer, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_border_width(_app_layer, 0, 0);
    lv_obj_set_style_radius(_app_layer, 0, 0);
    lv_obj_set_style_pad_all(_app_layer, 0, 0);
    lv_obj_set_style_bg_color(_app_layer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_app_layer, LV_OPA_COVER, 0);
    lv_obj_center(_app_layer);
    lv_obj_move_foreground(_app_layer);
    lv_obj_clear_flag(_app_layer, LV_OBJ_FLAG_SCROLLABLE);

    _current->onEnter(_app_layer);
    lv_obj_add_event_cb(_app_layer, on_gesture, LV_EVENT_GESTURE, nullptr);
    ui_enable_gesture_bubble(_app_layer);
    _state = State::App;

    if (_current->tickPeriodMs() > 0) {
        _tick_timer = lv_timer_create([](lv_timer_t*) {
            auto& m = AppManager::inst();
            if (m._current) m._current->onTick();
        }, _current->tickPeriodMs(), nullptr);
    }
}


void AppManager::onCrownClick()
{
    if (PowerManager::inst().isScreenOff()) { PowerManager::inst().wake(); return; }
    uint32_t now = lv_tick_get();
    if (now - _last_click_ms < DOUBLE_MS) {
        _last_click_ms = 0;
        PowerManager::inst().sleepNow();   // мгновенный сон
        return;
    }
    _last_click_ms = now;

    // Одиночный клик — через короткий таймер, чтобы дать шанс двойному
    lv_timer_t* t = lv_timer_create([](lv_timer_t* t) {
        auto& m = AppManager::inst();
        if (m._last_click_ms == 0) { lv_timer_del(t); return; }

        if (m.isLocked())       m.unlock();
        else if (m.isInApp())   m.back();
        else if (m.isInMenu())  m.showLock();   // ← коронка из меню → циферблат
        lv_timer_del(t);
    }, DOUBLE_MS, nullptr);
    lv_timer_set_repeat_count(t, 1);
}

void AppManager::onCrownLongPress()
{
    goHome();
}

void AppManager::registerApp(const char* name, const lv_img_dsc_t* icon,
                 std::function<std::unique_ptr<App>()> factory)
{
    _apps.push_back({name, icon, std::move(factory)});
}

void AppManager::tick()
{
    // Заглушка; вся логика в tick-таймере приложения
}

void AppManager::_closeCurrent()
{
    if (_tick_timer) { lv_timer_del(_tick_timer); _tick_timer= nullptr; }
    if (_current) { _current->onExit(); _current.reset(); }
    if (_app_layer)  { UI_OBJ_DEL(_app_layer); _app_layer =nullptr; }
}
void AppManager::_schedule(State s)
{
    _pending = s;
    if (_pending_flag) return;
    _pending_flag = true;
    lv_async_call([](void*) {
        auto& m = AppManager::inst();
        m._pending_flag = false;
        m._closeCurrent();
        m._applyState(m._pending);
    }, nullptr);
}
void AppManager::back()   { _schedule(State::Menu);   }
void AppManager::goHome() { _schedule(State::Locked); }

static void on_gesture(lv_event_t* e)
{
    lv_indev_t* indev = lv_event_get_indev(e);
    if (!indev) return;
    if (lv_indev_get_gesture_dir(indev) != LV_DIR_RIGHT) return;

    PowerManager::inst().resetActivity();
    AppManager::inst().back();
}