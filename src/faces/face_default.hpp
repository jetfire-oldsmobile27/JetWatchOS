#pragma once
#include <lvgl.h>

// Строит виджеты дефолтного локскрина в указанный parent
// (обычно это отдельный «source screen», созданный FaceManager).
void face_default_build(lv_obj_t* parent);

// Вызывается FaceManager::tick() раз в секунду.
void face_default_update();