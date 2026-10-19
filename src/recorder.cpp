#include "recorder.h"

#include <Arduino.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "storage.h"

static bool s_active = false;
static String s_name;
static String s_dir;
static uint32_t s_frames = 0;
static uint32_t s_errors = 0;
static uint32_t s_startedMs = 0;
static time_t s_startedEpoch = 0;
static char s_error[64] = "";

static const char* framesizeLabel(framesize_t fs) {
  switch (fs) {
    case FRAMESIZE_QVGA: return "QVGA";
    case FRAMESIZE_VGA:  return "VGA";
    case FRAMESIZE_SVGA: return "SVGA";
    case FRAMESIZE_HD:   return "HD";
    case FRAMESIZE_SXGA: return "SXGA";
    case FRAMESIZE_UXGA: return "UXGA";
    default:             return "OTHER";
  }
}

static String makeSessionName() {
  time_t now = time(nullptr);
  char buf[32];
  if (now > 1700000000) {
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tmv);
    return String(buf);
  }
  Preferences prefs;
  prefs.begin("printcam", false);
  uint32_t seq = prefs.getUInt("seq", 0) + 1;
  prefs.putUInt("seq", seq);
  prefs.end();
  snprintf(buf, sizeof(buf), "PRINT_%06lu", (unsigned long)seq);
  return String(buf);
}

bool recorderStartSession() {
  if (s_active) return false;
  if (!storageReady()) {
    snprintf(s_error, sizeof(s_error), "SD not ready");
    return false;
  }

  storageEnforceSpace();

  s_name = makeSessionName();
  s_dir = String(SD_MOUNTPOINT) + PRINTS_DIR + "/" + s_name;

  if (!SD_MMC.exists(s_dir.c_str())) {
    SD_MMC.mkdir(s_dir.c_str());
  }
  if (!SD_MMC.exists(s_dir.c_str())) {
    snprintf(s_error, sizeof(s_error), "mkdir failed");
    return false;
  }

  s_frames = 0;
  s_errors = 0;
  s_startedMs = millis();
  s_startedEpoch = time(nullptr);
  s_active = true;
  s_error[0] = '\0';
  Serial.printf("[rec] session started: %s\n", s_name.c_str());
  return true;
}

bool recorderActive() {
  return s_active;
}

bool recorderSaveFrame(camera_fb_t* fb) {
  if (!s_active || fb == nullptr || fb->format != PIXFORMAT_JPEG) return false;

  char path[160];
  snprintf(path, sizeof(path), "%s/frame_%05lu.jpg", s_dir.c_str(), (unsigned long)(s_frames + 1));

  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    s_errors++;
    snprintf(s_error, sizeof(s_error), "open failed");
    return false;
  }
  size_t written = f.write(fb->buf, fb->len);
  f.close();

  if (written != fb->len) {
    SD_MMC.remove(path);
    s_errors++;
    snprintf(s_error, sizeof(s_error), "short write");
    return false;
  }

  s_frames++;
  s_errors = 0;
  return true;
}

void recorderEndSession() {
  if (!s_active) return;

  time_t endEpoch = time(nullptr);
  uint32_t durationS = (millis() - s_startedMs) / 1000UL;

  char jsonPath[176];
  snprintf(jsonPath, sizeof(jsonPath), "%s/session.json", s_dir.c_str());
  File f = SD_MMC.open(jsonPath, FILE_WRITE);
  if (f) {
    f.printf("{\"name\":\"%s\",\"start_epoch\":%ld,\"end_epoch\":%ld,"
             "\"duration_s\":%lu,\"frames\":%lu,\"interval_ms\":%lu,"
             "\"framesize\":\"%s\",\"jpeg_quality\":%d,\"complete\":true}\n",
             s_name.c_str(), (long)s_startedEpoch, (long)endEpoch,
             (unsigned long)durationS, (unsigned long)s_frames,
             (unsigned long)CAPTURE_INTERVAL_MS, framesizeLabel(CAPTURE_FRAMESIZE),
             CAPTURE_JPEG_QUALITY);
    f.close();
  }

  Serial.printf("[rec] session ended: %s frames=%lu duration=%lus errors=%lu\n",
                s_name.c_str(), (unsigned long)s_frames, (unsigned long)durationS,
                (unsigned long)s_errors);
  s_active = false;
}

uint32_t recorderFrameCount() {
  return s_frames;
}

uint32_t recorderErrorCount() {
  return s_errors;
}

const char* recorderSessionName() {
  return s_name.c_str();
}

const char* recorderLastError() {
  return s_error;
}
