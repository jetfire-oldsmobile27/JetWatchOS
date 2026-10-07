#pragma once

#include "ui/ui_common.h"
#include "core/AppManager.h"

#include <memory>

std::unique_ptr<App> app_stopwatch_make();
std::unique_ptr<App> app_weather_make();
std::unique_ptr<App> app_pedometer_make();
std::unique_ptr<App> app_alarm_make();
std::unique_ptr<App> app_music_make();
std::unique_ptr<App> app_flashlight_make();
std::unique_ptr<App> app_training_make();
std::unique_ptr<App> app_terminal_make();
std::unique_ptr<App> app_settings_make();
std::unique_ptr<App> app_notifications_make();

void app_notifications_push(const char* app, const char* title, const char* body);