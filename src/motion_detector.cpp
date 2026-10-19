#include "motion_detector.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <img_converters.h>
#include <string.h>

#include "config.h"

static uint8_t* s_rgb = nullptr;
static uint8_t* s_prev = nullptr;
static uint16_t s_w = 0, s_h = 0;
static uint16_t s_rx = 0, s_ry = 0, s_rw = 0, s_rh = 0;
static bool s_havePrev = false;
static bool s_allocFailed = false;
static bool s_warned = false;

static int clampi(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static bool allocBuffers(uint16_t w, uint16_t h) {
  int x0 = clampi((int)(MOTION_ROI_X0 * w), 0, w - 1);
  int y0 = clampi((int)(MOTION_ROI_Y0 * h), 0, h - 1);
  int x1 = clampi((int)(MOTION_ROI_X1 * w), 0, w);
  int y1 = clampi((int)(MOTION_ROI_Y1 * h), 0, h);
  if (x1 <= x0) x1 = w;
  if (y1 <= y0) y1 = h;

  s_w = w;
  s_h = h;
  s_rx = (uint16_t)x0;
  s_ry = (uint16_t)y0;
  s_rw = (uint16_t)(x1 - x0);
  s_rh = (uint16_t)(y1 - y0);

  s_rgb = (uint8_t*)heap_caps_malloc((size_t)w * h * 3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  s_prev = (uint8_t*)heap_caps_malloc((size_t)s_rw * s_rh, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (s_rgb == nullptr || s_prev == nullptr) {
    if (s_rgb) heap_caps_free(s_rgb);
    if (s_prev) heap_caps_free(s_prev);
    s_rgb = nullptr;
    s_prev = nullptr;
    s_allocFailed = true;
    Serial.println("[motion] PSRAM alloc failed");
    return false;
  }
  Serial.printf("[motion] %ux%u roi %u,%u %ux%u rgb=%u prev=%u\n",
                w, h, s_rx, s_ry, s_rw, s_rh,
                (unsigned)((size_t)w * h * 3), (unsigned)((size_t)s_rw * s_rh));
  return true;
}

bool motionInit() {
  motionDeinit();
  s_allocFailed = false;
  s_warned = false;
  s_havePrev = false;
  return true;
}

float motionAnalyze(const camera_fb_t* fb) {
  if (fb == nullptr || fb->format != PIXFORMAT_JPEG) return -1.0f;
  if (s_allocFailed) return -2.0f;

  if (s_rgb == nullptr) {
    if (!allocBuffers(fb->width, fb->height)) return -2.0f;
  }

  if (!fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, s_rgb)) {
    if (!s_warned) {
      Serial.println("[motion] jpeg decode failed");
      s_warned = true;
    }
    return -1.0f;
  }

  size_t changed = 0;
  const size_t total = (size_t)s_rw * s_rh;
  uint8_t* p = s_prev;

  for (uint16_t y = s_ry; y < (uint16_t)(s_ry + s_rh); y++) {
    const uint8_t* px = s_rgb + (size_t)y * s_w * 3 + (size_t)s_rx * 3;
    for (uint16_t x = 0; x < s_rw; x++, px += 3, p++) {
      uint8_t g = (uint8_t)((px[0] * 77 + px[1] * 150 + px[2] * 29) >> 8);
      if (s_havePrev) {
        int d = (int)g - (int)*p;
        if (d < 0) d = -d;
        if (d > MOTION_PIXEL_DELTA) changed++;
      }
      *p = g;
    }
  }

  if (!s_havePrev) {
    s_havePrev = true;
    return 0.0f;
  }
  return (float)changed / (float)total;
}

void motionReset() {
  s_havePrev = false;
}

void motionDeinit() {
  if (s_rgb) heap_caps_free(s_rgb);
  if (s_prev) heap_caps_free(s_prev);
  s_rgb = nullptr;
  s_prev = nullptr;
  s_w = s_h = s_rx = s_ry = s_rw = s_rh = 0;
  s_havePrev = false;
}
