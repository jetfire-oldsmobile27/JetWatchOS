#pragma once
#include <LilyGoLib.h>
#include <lvgl.h>
#include <vector>
#include <functional>
#include <memory>

class App {
public:
    virtual ~App() = default;
    virtual const char* name() const = 0;
    virtual const lv_img_dsc_t* icon() const { return nullptr; }
    virtual void onEnter(lv_obj_t* parent) = 0;
    virtual void onExit() {}

    // Опционально: приложение хочет получать тик (для таймеров внутри приложения)
    virtual void onTick() {}
    virtual uint32_t tickPeriodMs() const { return 0; }
};

class AppManager {
public:
    struct AppEntry {
        const char* name;
        const lv_img_dsc_t* icon;
        std::function<std::unique_ptr<App>()> factory;
    };

    static AppManager& inst();

    void begin();
    void tick();

    void showLock();
    void unlock();
    void showMenu();
    void launchApp(std::unique_ptr<App> app);
    void back();
    void goHome();

    bool isLocked() const { return _state == State::Locked; }
    bool isInApp()  const { return _state == State::App; }
    bool isInMenu() const { return _state == State::Menu; }

    void registerApp(const char* name, const lv_img_dsc_t* icon,
                 std::function<std::unique_ptr<App>()> factory);
    const std::vector<AppEntry>& apps() const { return _apps; }

    lv_group_t* group() { return _group; }

    // Вызывается PowerManager'ом и BleService'ом
    void onCrownClick();     // короткое нажатие
    void onCrownLongPress();

private:
    enum class State { Locked, Menu, App };
    void _applyState(State s);
    void _buildMenuTile();

    State _state = State::Locked;
    std::unique_ptr<App> _current;
    lv_timer_t* _tick_timer  {nullptr};

    lv_obj_t* _tileview   {nullptr};
    lv_obj_t* _tile_lock  {nullptr};
    lv_obj_t* _tile_menu  {nullptr};
    lv_obj_t* _app_layer  {nullptr};   // оверлей для приложения

    lv_group_t* _group  {nullptr};
    std::vector<AppEntry> _apps;

    uint32_t _last_click_ms = 0;
    static constexpr uint32_t DOUBLE_MS = 400;
    void _closeCurrent();
    void _schedule(State s);
    State _pending = State::Menu;
    bool  _pending_flag = false;
};