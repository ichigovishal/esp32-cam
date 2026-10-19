#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_camera.h"

bool motionInit();
float motionAnalyze(const camera_fb_t* fb);
void motionReset();
void motionDeinit();
