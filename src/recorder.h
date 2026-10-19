#pragma once

#include <stdint.h>

#include "esp_camera.h"

bool recorderStartSession();
bool recorderActive();
bool recorderSaveFrame(camera_fb_t* fb);
void recorderEndSession();
uint32_t recorderFrameCount();
uint32_t recorderErrorCount();
const char* recorderSessionName();
const char* recorderLastError();
