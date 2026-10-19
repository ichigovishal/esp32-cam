# ESP32-CAM 3D Print Timelapse Recorder

Motion-detection based timelapse recorder for an ESP32-CAM AI-Thinker. Captures one HD JPEG every few seconds while printing, ignoring motion outside a user-defined ROI ("nozzle only"). Sessions are stored on microSD in timestamped folders and can be listed/downloaded from the onboard web UI. Designed to be fast-forwarded into vertical 9:16 Shorts later on your PC.

## Features

- ROI-based frame-differencing (detects only the print area/nozzle)
- Start on persistent motion, end after configurable idle timeout
- HD JPEG capture (FRAMESIZE_HD 1280×720 by default; easy to bump to UXGA)
- Timestamped session folders; `session.json` with metadata
- Auto-deletes oldest sessions at 90% SD usage; write error throttling
- STA WiFi + mDNS (`http://printcam.local`) with a minimal status web page
- Buttons to start/stop manually
- Files list/download endpoints (`/files`, `/download`)

## Directory layout

```
ESP32-Cam-Ender3-S1-Pro-Firmware/
├── include/
│   ├── config.h         # Tunables (interval, ROI, thresholds, NTP, WiFi defaults)
│   └── secrets.h.example # Copy to secrets.h and put real WiFi creds
├── src/
│   ├── camera.{h,cpp}
│   ├── motion_detector.{h,cpp}
│   ├── recorder.{h,cpp}
│   ├── storage.{h,cpp}
│   ├── wifi_manager.{h,cpp}
│   └── main.cpp
├── platformio.ini
├── .gitignore
└── docs/README.md (this one, also setup/wiring below)
```

## Quick start

### 1. Prerequisites

- PlatformIO Core v6+ (`pio --version` works)
- USB-serial adapter (FTDI/CH340), jumper wires
- ESP32-CAM AI-Thinker (with OV2640)
- MicroSD card (Class 10 / U3 recommended) formatted FAT32

### 2. Clone / enter project

```bash
cd "ESP32-Cam-Ender3-S1-Pro-Firmware"
```

### 3. WiFi credentials

```bash
cp include/secrets.h.example include/secrets.h
# edit include/secrets.h with your SSID/PASSWORD
```

### 4. Tune config (recommended to do once)

Open `include/config.h` and adjust:

- `TZ` (POSIX timezone) — default `IST-5:30`
- `CAPTURE_INTERVAL_MS` — 3000 → 90× fast-forward
- `CAPTURE_FRAMESIZE` — `FRAMESIZE_HD` or `FRAMESIZE_UXGA`
- `MOTION_FRAMESIZE` — `FRAMESIZE_QVGA` (cheap)
- `MOTION_ROI_X0/Y0/X1/Y1` — crop to only build plate/nozzle (fractions 0..1)
- `MOTION_THRESHOLD`, `MOTION_START_HITS`, `PRINT_END_TIMEOUT_MS`
- `STORAGE_MAX_USED`, `STORAGE_MIN_FREE_MB`

### 5. Build

```bash
export PATH="$HOME/.local/bin:$PATH"
pio run
```

### 6. Flash

Connect ESP32-CAM: GPIO0 → GND, power via 5V 2A (USB 2.0 on PC can be borderline). Upload:

```bash
pio run -t upload
```

Remove GPIO0–GND jumper after flash, press reset.

### 7. Check logs

```bash
pio device monitor --baud 115200
```

Look for `[cam] init ok`, `[sd] mounted`, `[wifi] connected`, `[web] mDNS: http://printcam.local`

### 8. Access & test

- Open `http://printcam.local` (or your IP)
- Press **Start** to begin a session (good for testing frame capture/SD)
- Press **Stop** to finish
- Frames land in `/sdcard/prints/YYYYMMDD_HHMMSS/frame_*.jpg`
- `session.json` contains start/end/duration/frames/count info

## Post-processing (Fast-forward into 9:16 Shorts)

Assumptions: camera mounted **rotated 90°** (portrait) for minimal cropping. If it remains landscape, you'll crop to 9:16.

### Extract on PC

Option A: Download via web — `curl http://printcam.local/files` to list, then `curl -o <name>.zip ...`? Or just use a file browser to pull the entire session folder.

Option B: Remove SD card and copy files to PC.

### Assemble with ffmpeg

**If camera was rotated 90° (true vertical 9:16)** — just stack frames in order. Example:

```bash
# path to frames
cd /path/to/SESSION_NAME
ffmpeg -framerate 30 -pattern_type glob -i "frame_*.jpg" \
  -vf "scale=720:1280,format=yuv420p" \
  -c:v libx264 -crf 23 -preset medium out_720x1280.mp4
```

**If camera left in landscape 4:3** — crop a centered 9:16 strip (best effort):

```bash
ffmpeg -framerate 30 -pattern_type glob -i "frame_*.jpg" \
  -vf "crop=iw*9/16:ih,scale=720:1280,format=yuv420p" \
  -c:v libx264 -crf 23 -preset medium out_720x1280.mp4
```

Tweak `-crf` (lower = better, larger) and preset to taste. 30fps is Shorts-friendly.

## Tips for motion detection ("nozzle only")

1. Power up, point cam at printer with a clear view of the nozzle/build plate.
2. Take a test snapshot mentally from logs/monitor: in `motion_detector.cpp`, ROI is fractional.
3. Narrow `MOTION_ROI_*` to cover only the working area where the nozzle actually moves during printing (exclude the frame edges, door, background).
4. Avoid direct light changes — strong sunlight moving across can trigger. Indirect lighting is best.
5. If false-starts happen: raise `MOTION_START_HITS` or lower `MOTION_THRESHOLD`.
6. If it misses start (rare): lower hits, raise sensitivity cautiously.

## Flashing notes

- AI-Thinker ESP32-CAM: pull GPIO0 to GND on power-up to enter bootloader
- RX/TX: connect to USB-serial adapter; many adapters need 3.3V logic (most modern do)
- Power with a solid 5V 2A+ source when recording; PC USB can dip under load
- SD: push in firmly; 1-bit mode is used by design

## Known caveats

- Decompression for motion is done in PSRAM; if PSRAM absent firmware will refuse to init
- Frame grabs must be in grab_mode LATEST (already set) to avoid mixing buffers
- Very fast printhead moves can blur frames; HD + 3s interval is a good balance
- NTP time sync is non-blocking — if no internet, session names fall back to `PRINT_#######`

## License

Same as your project context (GNU-ish assumptions don't apply here) — treat as-is for personal use.
