#include "camera.h"

#include <Arduino.h>
#include <stdio.h>

#include "config.h"

static bool s_ok = false;
static framesize_t s_framesize = FRAMESIZE_INVALID;
static char s_error[64] = "";

static void fsDims(framesize_t fs, uint16_t* w, uint16_t* h) {
  switch (fs) {
    case FRAMESIZE_96X96:   *w = 96;   *h = 96;   break;
    case FRAMESIZE_QQVGA:   *w = 160;  *h = 120;  break;
    case FRAMESIZE_QCIF:    *w = 176;  *h = 144;  break;
    case FRAMESIZE_HQVGA:   *w = 240;  *h = 176;  break;
    case FRAMESIZE_QVGA:    *w = 320;  *h = 240;  break;
    case FRAMESIZE_CIF:     *w = 400;  *h = 296;  break;
    case FRAMESIZE_VGA:     *w = 640;  *h = 480;  break;
    case FRAMESIZE_SVGA:    *w = 800;  *h = 600;  break;
    case FRAMESIZE_XGA:     *w = 1024; *h = 768;  break;
    case FRAMESIZE_HD:      *w = 1280; *h = 720;  break;
    case FRAMESIZE_SXGA:    *w = 1280; *h = 1024; break;
    case FRAMESIZE_UXGA:    *w = 1600; *h = 1200; break;
    default:                *w = 0;    *h = 0;    break;
  }
}

bool cameraInit() {
  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer = LEDC_TIMER_0;
  cfg.pin_d0 = 5;
  cfg.pin_d1 = 18;
  cfg.pin_d2 = 19;
  cfg.pin_d3 = 21;
  cfg.pin_d4 = 36;
  cfg.pin_d5 = 39;
  cfg.pin_d6 = 34;
  cfg.pin_d7 = 35;
  cfg.pin_xclk = 0;
  cfg.pin_pclk = 22;
  cfg.pin_vsync = 25;
  cfg.pin_href = 23;
  cfg.pin_sccb_sda = 26;
  cfg.pin_sccb_scl = 27;
  cfg.pin_pwdn = 32;
  cfg.pin_reset = -1;
  cfg.xclk_freq_hz = CAM_XCLK_HZ;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.frame_size = CAPTURE_FRAMESIZE;
  cfg.jpeg_quality = CAPTURE_JPEG_QUALITY;
  cfg.fb_count = 2;
  cfg.fb_location = CAMERA_FB_IN_PSRAM;
  cfg.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound() == false) {
    snprintf(s_error, sizeof(s_error), "PSRAM not found");
    return false;
  }

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    snprintf(s_error, sizeof(s_error), "esp_camera_init 0x%x", (unsigned)err);
    return false;
  }

  sensor_t* s = esp_camera_sensor_get();
  if (s != nullptr) {
    s->set_vflip(s, CAM_VFLIP);
    s->set_hmirror(s, CAM_HMIRROR);
  }

  s_framesize = CAPTURE_FRAMESIZE;
  s_ok = true;
  s_error[0] = '\0';
  uint16_t iw = 0, ih = 0;
  fsDims(s_framesize, &iw, &ih);
  Serial.printf("[cam] init ok (%ux%u jpeg q%d fb=%d psram=%u)\n",
                (unsigned)iw, (unsigned)ih, CAPTURE_JPEG_QUALITY, (int)cfg.fb_count,
                (unsigned)ESP.getFreePsram());
  return true;
}

camera_fb_t* cameraCapture(framesize_t framesize) {
  if (!s_ok) return nullptr;

  sensor_t* s = esp_camera_sensor_get();
  if (s != nullptr && framesize != s_framesize) {
    if (s->set_framesize(s, framesize) != 0) {
      snprintf(s_error, sizeof(s_error), "set_framesize failed");
      return nullptr;
    }
    s_framesize = framesize;
  }

  uint16_t ew = 0, eh = 0;
  fsDims(framesize, &ew, &eh);

  for (int attempt = 0; attempt < 3; attempt++) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb == nullptr) {
      snprintf(s_error, sizeof(s_error), "fb_get null");
      return nullptr;
    }
    if (ew == 0 || (fb->width == ew && fb->height == eh)) return fb;
    esp_camera_fb_return(fb);
  }

  snprintf(s_error, sizeof(s_error), "framesize mismatch");
  return nullptr;
}

void cameraRelease(camera_fb_t* fb) {
  if (fb != nullptr) esp_camera_fb_return(fb);
}

const char* cameraLastError() {
  return s_error;
}
