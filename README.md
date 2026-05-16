# DSPxLab

**Active DSP Preamplifier System** — browser-based crossover designer, driver corrector and room EQ tool for audiophile two-way/ up to 4 ways stereo systems built around ADAU1452 and ADAU1701.

---

## Overview

DSPxLab is a commercial-grade audiophile active preamplifier system housed in a **Galaxy GX288** cabinet. It provides a complete signal chain from analog input to speaker output, with real-time DSP processing, browser-based control and measurement, and Bluetooth/USB audio inputs.

Two companion web apps (single-file HTML/JS, no server required) drive the system:

| App | Purpose |
|-----|---------|
| **DSPxLab** | Crossover design, driver correction (FIR/IIR), gain alignment, system simulation, upload to DSP |
| **AutoRoomEQ** | Multi-point room correction, FIR export for convolution players |

---

## Hardware Architecture

```
Analog In (XLR/RCA)
      │
   PCM1808 ADC (I²S)
      │
   ADAU1452 ──── SPI boot ──── (future: SD/EEPROM)
   192kHz / 8 out via CS42448
      │  TDM
   ADAU1701 (Wondom AA-AP23123)
   Self-boot via AT24C512 EEPROM (I²C 0xA0)
   ICP5 via I²C 0xD0/0x68
      │
   4× DAC output → Power Amplifiers → Drivers
      │
   Tweeter L/R  (HP channel, FIR 262 tap eq-only)
   Woofer  L/R  (LP channel, IIR 13-band Peak EQ)

Digital Inputs:
   XMOS XU208  ──── USB Audio Class 2 (UAC2)
   QCC5125     ──── Bluetooth aptX-HD

Controller:
   Waveshare ESP32-S3-LCD-3.16
   ├── ST7701S LCD (SPI, GPIO6 backlight)
   ├── I²C bus: GPIO15=SDA, GPIO7=SCL
   ├── PCF8574 I²C expander (GPIO4=IRQ)
   ├── PCF85063 RTC
   └── WebSocket server → DSPxLab browser UI

Power:
   Mean Well IRM-15-5 (5V/3A)
   LC filters 100µH + 470µF + 1Ω on DSP board lines
```

---

## DSP Architecture

### ADAU1701 (fixed, SigmaStudio)

| Channel | Correction | Taps/Bands |
|---------|-----------|------------|
| HP-L / HP-R (Tweeter) | FIR linear-phase eq-only | 262 tap each |
| LP-L / LP-R (Woofer)  | IIR 13-band Peak EQ | 13 biquad each |

- IIR crossover (LR4 biquad) always active — crossover slope is in the DSP, not baked into the FIR
- FIR corrects driver response in passband only (`1.5×fc → nTaps×30 Hz`)
- Peak EQ corrects woofer response in sub-bass (`20 Hz → 0.72×fc`)
- Tap budget: 262 tap per HP channel → max reliable correction ~7.8 kHz

### ADAU1452 (future, SigmaStudio)

| Channel | Correction | Taps |
|---------|-----------|------|
| Each of up to 8 channels | FIR linear-phase eq-only | 735 tap |

- TDM 192 kHz, 8 outputs via CS42448
- IIR LR4 crossover separate from FIR eq
- Supports 2-way stereo, 2.1, 2.1m, 3-way, 4-way topologies

---

## ESP32 Firmware

**Platform:** Arduino / ESP-IDF on Waveshare ESP32-S3-LCD-3.16

### Features
- **WebSocket server** — real-time bidirectional control from DSPxLab browser UI
- **NVS persistence** — ADAU1452 parameters saved to flash, restored at boot (+200 ms delay)
  - Opcodes: `0x15 / 0x16 / 0x17 / 0x21`, namespace `"dsp_p"`, auto-flush 2 s
- **ICP5 proxy** — relays I²C writes to ADAU1701 via JAB3 firmware (COM5, CH340)
- **DSP output routing** — `espSetRoutingForChannel` / `espRestoreAllChannels`
- **LCD UI** — ST7701S display with real-time level meters and status
- **PCF8574** — GPIO expander for front-panel buttons (IRQ on GPIO4)
- **PCF85063** — RTC for timestamped logs

### GPIO Map

| GPIO | Function |
|------|---------|
| 15 | I²C SDA |
| 7  | I²C SCL |
| 6  | LCD backlight |
| 4  | PCF8574 IRQ |

> ⚠️ GPIO 8/9 occupied by RGB display bus. GPIO 16 conflicts with LCD Reset — do not use.

### Build & Flash
```bash
# Arduino IDE or PlatformIO
# Board: ESP32-S3 Dev Module
# Flash: 4 MB, PSRAM enabled
# Partition: default or custom with NVS enlarged

pio run --target upload
```

---

## DSPxLab Web App

Single HTML file, no install, no server. Open in Chrome/Edge.

### Workflow

```
1. Setup      → select DSP chip, topology, crossover frequency
2. Measure    → log-sweep via microphone (IXO22 + ECM8000/IMM-6)
               → or load virtual measurements (manufacturer FR data)
3. Elaborate  → calculates:
                 · IIR LR4 crossover biquad sections
                 · FIR linear-phase eq (262 or 735 tap, Kaiser β=8)
                 · IIR 13-band Peak EQ for woofer (ADAU1701)
                 · Auto gain alignment at crossover frequency (skipFir mode)
4. Review     → simulated response chart:
                 · raw measurement
                 · solo XO (measurement × IIR crossover)
                 · corr pre-XO (measurement × FIR/PEQ)
                 · final (measurement × FIR × IIR × gain)
                 · system sum (coherent HP+LP)
5. Export     → Structure (.params): IIR XO + gain + mute + delay
              → Correction (.params): FIR coefficients + PEQ biquad
6. Upload     → direct write to ADAU1701 via ICP5/ESP32 WebSocket
```

### FIR Design Rules

| Channel | Band | Max fHigh |
|---------|------|-----------|
| HP/Tweeter | `1.5×fc → nTaps×30 Hz` | 7.86 kHz (262 tap) / 22 kHz (735 tap) |
| LP/Woofer  | `20 Hz → 0.75×fc`     | ADAU1701: no FIR, Peak EQ only |
| BP/Mid     | `1.25×fcLo → 0.75×fcHi` | — |

- NFFT ≥ 65536 for sub-bass resolution
- Auto gain alignment uses `raw × IIR` only (FIR excluded) to avoid correction artifacts

### Supported Topologies

| Code | Description |
|------|------------|
| `stereo` | 2-channel full range |
| `21`     | 2-way stereo (HP-L, HP-R, LP-L, LP-R) |
| `21m`    | 2.1 mono sub (HP-L, HP-R, LP mono) |
| `3way`   | 3-way stereo |
| `4way`   | 4-way stereo |

---

## AutoRoomEQ Web App

Browser-based multi-point room correction. Single HTML file.

### Features
- 9-position measurement grid (3×3), weighted average
- Per-band aggressiveness sliders (bass / mid / treble)
- Export: WAV mono IR + EQ APO `.txt` config
- Harman target curves (LS / HF)
- FIR always calculated at native AudioContext SR to avoid resampling artifacts

---

## Measurement Chain

```
Speaker → Room → Microphone (ECM8000 or IMM-6)
                      │
                 IXO22 audio interface
                      │
                 Browser (WebAudio)
                 log-sweep → IR → FFT → FR curve
                      │
                 DSPxLab Elaborate tab
```

Reference: REW (Room EQ Wizard) for validation.

---

## File Structure

```
DSPxLab/
├── DSPxLab_v5d.html          # Main DSP controller + measurement app
├── AutoRoomEQ_v4.html        # Room correction app
├── firmware/
│   ├── esp32_dspxlab/        # ESP32-S3 Arduino sketch
│   │   ├── esp32_dspxlab.ino
│   │   ├── ws_handler.cpp    # WebSocket + NVS
│   │   ├── lcd_ui.cpp        # ST7701S display
│   │   └── icp5_bridge.cpp   # ADAU1701 ICP5 proxy
│   └── adau1701/
│       └── DSPxLab_1701.dspproj  # SigmaStudio project
├── sigmastudio/
│   └── DSPxLab_1452.dspproj  # ADAU1452 SigmaStudio project (WIP)
└── README.md
```

---

## Known Limitations / TODO

- [ ] ADAU1452 SigmaStudio project: add Single Volume blocks for sub-L/R, re-export `.params`
- [ ] ADAU1701 EEPROM persistence: Wondom E2Prom.Hex is partial (hardware registers only, no Parameter RAM) — solution under evaluation: ESP32-C3FH4 SuperMini + DPDT relay to replay calibration via ICP5 at boot
- [ ] FIR → biquad conversion (for systems without FIR block budget)
- [ ] AutoRoomEQ: WAV export SR mismatch with player → verify player SR matches FIR calculation SR

---

## Dependencies

All dependencies are CDN-loaded in the HTML files — no npm, no build step.

| Library | Version | Use |
|---------|---------|-----|
| Chart.js | 4.4.1 | Frequency response charts |

---

## License

Proprietary — commercial audiophile product. All rights reserved.

---

## Author

Paolo — Genova, Italy  
Project started 2026
