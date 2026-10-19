#pragma once

#include "esp_camera.h"

bool cameraInit();
camera_fb_t* cameraCapture(framesize_t framesize);
void cameraRelease(camera_fb_t* fb);
const char* cameraLastError();
