#pragma once

// ---------------------------------------------------------------------------
// WiFi — create include/secrets.h (copy secrets.h.example) to keep
// credentials out of git, or just edit the defaults below.
// ---------------------------------------------------------------------------
#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

#define HOSTNAME        "printcam"   // http://printcam.local
#define WIFI_TIMEOUT_MS 20000

// ---------------------------------------------------------------------------
// Time (NTP) — POSIX TZ string.
// India: "IST-5:30"  |  UTC: "UTC0"  |  US Eastern: "EST5EDT,M3.2.0,M11.1.0"
// Valid time is used for session folder names; without it fallback names
// PRINT_000001 are used, so recording works offline too.
// ---------------------------------------------------------------------------
#define TZ           "IST-5:30"
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"

// ---------------------------------------------------------------------------
// Capture (the timelapse itself)
// One HD JPEG every CAPTURE_INTERVAL_MS while a print is detected.
// PC assembles frames at 30 fps later → speed-up = 30 * interval.
// 3 s interval → 90x fast-forward: a 2 h print becomes an 80 s Short.
// ---------------------------------------------------------------------------
#define CAPTURE_FRAMESIZE    FRAMESIZE_HD   // 1280x720; FRAMESIZE_UXGA = 1600x1200
#define CAPTURE_JPEG_QUALITY 12             // 5 = best/biggest ... 25 = small/lossy
#define CAPTURE_INTERVAL_MS  3000UL

// ---------------------------------------------------------------------------
// Motion detection (print start / print end) — "nozzle only" via ROI
// Low-res JPEG frames are decoded and frame-differenced inside the ROI.
// Motion outside the ROI (people, pets, room) is ignored.
// ---------------------------------------------------------------------------
#define MOTION_FRAMESIZE     FRAMESIZE_QVGA  // 320x240 analysis frames
#define MOTION_CHECK_MS      2000UL          // how often motion is analyzed
#define MOTION_PIXEL_DELTA   18              // per-pixel change to count as different
#define MOTION_THRESHOLD     0.035f          // fraction of ROI that must change
#define MOTION_START_HITS    6               // consecutive hits = print started
                                              // (6 x 2 s = 12 s of motion)
#define PRINT_END_TIMEOUT_MS (5UL * 60UL * 1000UL)  // 5 min without motion = done
#define SESSION_COOLDOWN_MS  60000UL         // ignore motion after a session ends

// Region of interest as fractions of the motion frame (0.0 - 1.0).
// Point this at the build plate / nozzle area only. Measure by looking at
// the live scene (or a test frame) and noting which fraction of the image
// the printer occupies. Examples:
//   full frame          0.0, 0.0, 1.0, 1.0
//   lower 70% of image  0.0, 0.30, 1.0, 1.0
#define MOTION_ROI_X0 0.05f
#define MOTION_ROI_Y0 0.30f
#define MOTION_ROI_X1 0.95f
#define MOTION_ROI_Y1 1.00f

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------
#define CAM_XCLK_HZ  20000000
#define CAM_VFLIP    0    // 1 = flip image vertically (aiming aid)
#define CAM_HMIRROR  0    // 1 = mirror image horizontally

// ---------------------------------------------------------------------------
// Storage
// microSD: FAT32, Class 10 / U3 recommended. 1-bit SDMMC mode is used so
// GPIO4 (flash LED) and GPIO12 stay free.
// ---------------------------------------------------------------------------
#define SD_MOUNTPOINT     "/sdcard"
#define PRINTS_DIR        "/prints"       // sessions: /sdcard/prints/<name>/
#define STORAGE_MAX_USED  0.90f           // delete oldest sessions above 90% full
#define STORAGE_MIN_FREE_MB 50UL          // end session if free space drops below

// ---------------------------------------------------------------------------
// Behaviour
// ---------------------------------------------------------------------------
#define STATUS_LED_PIN 4   // AI-Thinker flash LED
#define LED_INDICATE   1   // 0 = LED never turns on (use if light washes out video)
