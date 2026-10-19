# Setup Guide

## Hardware checklist
- [ ] ESP32-CAM AI-Thinker (OV2640)
- [ ] USB-serial adapter
- [ ] Dupont wires
- [ ] 5V 2A power supply (recommended)
- [ ] microSD Class 10/U3, FAT32
- [ ] Mounting for camera (rotate 90° preferred)

## Flashing procedure
```bash
cd "ESP32-Cam-Ender3-S1-Pro-Firmware"
export PATH="$HOME/.local/bin:$PATH"
# set creds
cp include/secrets.h.example include/secrets.h && $EDITOR include/secrets.h
# build + upload
pio run -t upload
# monitor
pio device monitor --baud 115200
```

During upload: GPIO0→GND, power reset. After upload: remove jumper, reset.

## First boot sanity
Expect: `[cam] init ok ...`, `[sd] mounted ...`, `[web] mDNS: http://printcam.local`. Open URL on phone/laptop (same WiFi).

## ROI calibration
Run `pio device monitor` and watch for motion hits. Aim ROI at the nozzle/build plate only. Example: lower 70% → Y0=0.30, Y1=1.0, X0=0.05, X1=0.95 is often safe for Ender 3 S1 Pro.

## SD management
- Oldest sessions auto-deleted at 90% usage
- Stop session if free < 50MB (`STORAGE_MIN_FREE_MB`)
- `session.json` + `frame_*.jpg` per session

## Tips
- If camera fails to init: check 5V, PSRAM present (AI-Thinker has 4MB)
- If SD fails: try different card, or reformat FAT32
- If web unreachable: check `printcam.local` mDNS, or use IP from serial
- Disable LED: set `LED_INDICATE 0` to avoid reflection
