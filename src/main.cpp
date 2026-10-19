#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "camera.h"
#include "config.h"
#include "motion_detector.h"
#include "recorder.h"
#include "storage.h"
#include "wifi_manager.h"

enum class SystemState : uint8_t { Boot, Idle, Recording, Cooldown, Error };

static SystemState g_state = SystemState::Boot;
static char g_error[128] = "";
static uint32_t g_motionHits = 0;
static uint32_t g_lastCheckMs = 0;
static uint32_t g_lastCaptureMs = 0;
static uint32_t g_lastMotionSeenMs = 0;
static uint32_t g_cooldownUntilMs = 0;
static bool g_reqStart = false;
static bool g_reqStop = false;
static bool g_ntpStarted = false;

static const char* stateName(SystemState s) {
  switch (s) {
    case SystemState::Boot:      return "boot";
    case SystemState::Idle:      return "idle";
    case SystemState::Recording: return "recording";
    case SystemState::Cooldown:  return "cooldown";
    case SystemState::Error:     return "error";
  }
  return "?";
}

static const char* framesizeLabel(framesize_t fs) {
  switch (fs) {
    case FRAMESIZE_QVGA: return "320x240";
    case FRAMESIZE_VGA:  return "640x480";
    case FRAMESIZE_SVGA: return "800x600";
    case FRAMESIZE_HD:   return "1280x720";
    case FRAMESIZE_SXGA: return "1280x1024";
    case FRAMESIZE_UXGA: return "1600x1200";
    default:             return "other";
  }
}

static void setError(const char* msg) {
  snprintf(g_error, sizeof(g_error), "%s", msg);
  Serial.printf("[err] %s\n", g_error);
}

static bool timeValid() {
  return time(nullptr) > 1700000000;
}

static void updateLed(uint32_t now) {
#if LED_INDICATE
  switch (g_state) {
    case SystemState::Recording:
      digitalWrite(STATUS_LED_PIN, HIGH);
      break;
    case SystemState::Cooldown:
      digitalWrite(STATUS_LED_PIN, ((now / 500) & 1) ? HIGH : LOW);
      break;
    case SystemState::Error:
      digitalWrite(STATUS_LED_PIN, ((now / 150) & 1) ? HIGH : LOW);
      break;
    default:
      digitalWrite(STATUS_LED_PIN, LOW);
      break;
  }
#else
  (void)now;
  digitalWrite(STATUS_LED_PIN, LOW);
#endif
}

static float runMotionCheck() {
  camera_fb_t* fb = cameraCapture(MOTION_FRAMESIZE);
  if (fb == nullptr) return -1.0f;
  float frac = motionAnalyze(fb);
  cameraRelease(fb);
  return frac;
}

static bool startRecording(const char* why) {
  if (g_state == SystemState::Recording) return false;
  if (!recorderStartSession()) {
    setError(recorderLastError()[0] ? recorderLastError() : "session start failed");
    g_state = SystemState::Error;
    return false;
  }
  motionReset();
  g_motionHits = 0;
  g_lastCheckMs = millis();
  g_lastCaptureMs = 0;
  g_lastMotionSeenMs = millis();
  g_state = SystemState::Recording;
  Serial.printf("[app] recording started (%s)\n", why);
  return true;
}

static void endRecording(const char* why) {
  recorderEndSession();
  motionReset();
  g_motionHits = 0;
  g_state = SystemState::Cooldown;
  g_cooldownUntilMs = millis() + SESSION_COOLDOWN_MS;
  Serial.printf("[app] recording ended (%s)\n", why);
}

static String statusJson() {
  bool wifi = wifiReady();
  char timeBuf[24] = "no ntp sync";
  if (timeValid()) {
    time_t now = time(nullptr);
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tmv);
  }

  String json = "{";
  json += "\"state\":\"" + String(stateName(g_state)) + "\"";
  json += ",\"session\":\"" + String(recorderActive() ? recorderSessionName() : "") + "\"";
  json += ",\"frames\":" + String(recorderFrameCount());
  json += ",\"free_mb\":" + String((unsigned long)(storageFreeBytes() / (1024 * 1024)));
  json += ",\"total_mb\":" + String((unsigned long)(storageTotalBytes() / (1024 * 1024)));
  json += ",\"interval_s\":" + String(CAPTURE_INTERVAL_MS / 1000UL);
  json += ",\"capture\":\"" + String(framesizeLabel(CAPTURE_FRAMESIZE)) + " @ q" + String(CAPTURE_JPEG_QUALITY) + "\"";
  json += ",\"time\":\"" + String(timeBuf) + "\"";
  json += ",\"ip\":\"" + String(wifi ? WiFi.localIP().toString() : "0.0.0.0") + "\"";
  json += ",\"rssi\":" + String(wifi ? WiFi.RSSI() : 0);
  json += ",\"uptime_s\":" + String(millis() / 1000UL);
  json += ",\"error\":\"" + String(g_error) + "\"";
  json += "}";
  return json;
}

static bool webRequestStart() {
  if (g_state == SystemState::Recording) return false;
  g_reqStart = true;
  return true;
}

static bool webRequestStop() {
  if (g_state != SystemState::Recording) return false;
  g_reqStop = true;
  return true;
}

static void webRequestReboot() {
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("[app] ESP32-CAM 3D Print Timelapse Recorder");

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  bool camOk = cameraInit();
  bool sdOk = storageMount();
  bool motionOk = motionInit();

  wifiConnect(WIFI_TIMEOUT_MS);
  webBegin(statusJson, webRequestStart, webRequestStop, webRequestReboot);

  if (!camOk) {
    setError(cameraLastError()[0] ? cameraLastError() : "camera init failed");
    g_state = SystemState::Error;
  } else if (!sdOk) {
    setError(storageLastError()[0] ? storageLastError() : "SD init failed");
    g_state = SystemState::Error;
  } else if (!motionOk) {
    setError("motion detector init failed (PSRAM?)");
    g_state = SystemState::Error;
  } else {
    g_state = SystemState::Idle;
  }

  uint32_t now = millis();
  g_lastCheckMs = now;
  g_lastMotionSeenMs = now;
}

void loop() {
  webLoop();

  if (!g_ntpStarted && wifiReady()) {
    setenv("TZ", TZ, 1);
    tzset();
    configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
    g_ntpStarted = true;
    Serial.println("[time] NTP started");
  }

  if (g_reqStart) {
    g_reqStart = false;
    startRecording("manual");
  }
  if (g_reqStop) {
    g_reqStop = false;
    if (g_state == SystemState::Recording) endRecording("manual");
  }

  uint32_t now = millis();
  updateLed(now);

  switch (g_state) {
    case SystemState::Idle: {
      if (now - g_lastCheckMs >= MOTION_CHECK_MS) {
        g_lastCheckMs = now;
        float frac = runMotionCheck();
        if (frac <= -2.0f) {
          setError("motion detector out of memory");
          g_state = SystemState::Error;
          break;
        }
        if (frac >= MOTION_THRESHOLD) {
          g_motionHits++;
          g_lastMotionSeenMs = now;
        } else {
          g_motionHits = 0;
        }
        if (g_motionHits >= MOTION_START_HITS) {
          startRecording("motion");
        }
      }
      break;
    }

    case SystemState::Recording: {
      if (now - g_lastCaptureMs >= CAPTURE_INTERVAL_MS) {
        g_lastCaptureMs += CAPTURE_INTERVAL_MS;
        if (now - g_lastCaptureMs >= CAPTURE_INTERVAL_MS) g_lastCaptureMs = now;

        camera_fb_t* fb = cameraCapture(CAPTURE_FRAMESIZE);
        if (fb != nullptr) {
          recorderSaveFrame(fb);
          cameraRelease(fb);
        } else {
          Serial.printf("[cam] capture failed: %s\n", cameraLastError());
        }

        if (recorderErrorCount() >= 10) {
          setError("too many SD write errors");
          endRecording("sd errors");
          break;
        }
        if (recorderFrameCount() > 0 && recorderFrameCount() % 200 == 0) {
          storageEnforceSpace(recorderSessionName());
        }
        if (storageFreeBytes() < (uint64_t)STORAGE_MIN_FREE_MB * 1024 * 1024) {
          setError("storage low, session closed");
          endRecording("storage low");
          break;
        }
      }

      if (now - g_lastCheckMs >= MOTION_CHECK_MS) {
        g_lastCheckMs = now;
        float frac = runMotionCheck();
        if (frac >= MOTION_THRESHOLD) {
          g_lastMotionSeenMs = now;
        } else if (now - g_lastMotionSeenMs >= PRINT_END_TIMEOUT_MS) {
          endRecording("print finished");
        }
      }
      break;
    }

    case SystemState::Cooldown: {
      if ((int32_t)(now - g_cooldownUntilMs) >= 0) {
        motionReset();
        g_motionHits = 0;
        g_lastCheckMs = now;
        g_state = SystemState::Idle;
        Serial.println("[app] back to idle");
      }
      break;
    }

    case SystemState::Error:
    case SystemState::Boot:
    default:
      break;
  }

  delay(2);
}
