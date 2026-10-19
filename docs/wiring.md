# Wiring Guide

## ESP32-CAM AI-Thinker → USB-Serial (for flashing)

| ESP32-CAM | USB-Serial Adapter | Notes |
|---|---|---|
| 5V/VCC | 5V | Prefer external 5V 2A for first tests; avoid PC USB if unstable |
| GND | GND | Common ground |
| U0R (GPIO3) | TX | Transmit from adapter → ESP32 receive |
| U0T (GPIO1) | RX | ESP32 transmit → adapter receive |
| GPIO0 | GND | Bootloader mode (only while flashing) |
| IO16 | NC | Not needed |

**Flashing sequence:**
1. Connect GPIO0 → GND
2. Power up the ESP32-CAM (or reset)
3. Upload with `pio run -t upload`
4. Remove GPIO0–GND, press RESET

## microSD Card

- Insert a Class 10/U3 microSD (FAT32) into the ESP32-CAM slot
- 1-bit SDMMC mode is used (frees GPIO4/12) — no wiring changes needed

## Power Recommendations

- For recording HD frames every 3s, use **5V 2.0–2.5A regulated supply** into the 5V pin (not just USB)
- If powering from printer PSU, ensure clean regulation
- AI-Thinker flash LED (GPIO4) can be disabled via `LED_INDICATE 0` in `config.h` if it washes out the scene

## Camera Mounting (orientation for Shorts)

**Recommended:** Mount the camera **rotated 90°** (portrait) so the frame is already 9:16. This maximizes usable vertical area and avoids heavy cropping.

**Alternative:** Keep landscape and crop to 9:16 during ffmpeg post-processing.

## ROI Tuning (nozzle-only motion detection)

1. Open `http://printcam.local` while looking at the printer
2. Note the area where the nozzle moves during a real print (build plate center/work zone)
3. Edit `MOTION_ROI_X0,Y0,X1,Y1` in `include/config.h` as **fractions of the motion frame** (0.0 = left/top, 1.0 = right/bottom). Use QVGA perspective.
4. Rebuild/upload, test with a small print (benchy) to avoid false-starts from bed heating or people.
5. Tune: increase `MOTION_START_HITS` (more patience), adjust `MOTION_THRESHOLD`, or narrow ROI.
