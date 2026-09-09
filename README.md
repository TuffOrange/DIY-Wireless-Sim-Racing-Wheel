<div align="center">

# DIY Wireless Sim Racing Wheel

**A homemade Arduino steering wheel with wireless pedals, dual input modes, and an on-board OLED status display.**

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/build-PlatformIO-orange.svg)
![Boards](https://img.shields.io/badge/boards-Pro%20Micro%20%2B%20Nano-informational.svg)

</div>

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware](#hardware)
- [Repository Structure](#repository-structure)
- [Wiring](#wiring)
- [Flashing](#flashing)
- [Usage](#usage)
- [Known Limitations](#known-limitations)
- [Roadmap](#roadmap)
- [License](#license)

---

## Overview

This project turns a pair of Arduino boards into a functional sim-racing wheel: a **main unit** built around a Pro Micro handles the wheel, mode switching, an OLED status display, and USB HID output to the PC, while a separate **pedal unit** built around a Nano reads the throttle and brake and transmits the data wirelessly over nRF24L01, eliminating the need to run a cable across the floor.

---

## Features

| | |
|---|---|
| **Wireless pedals** | nRF24L01 link between the pedal unit and the main unit — no floor cable |
| **Dual input mode** | Switch instantly between full analog joystick input and WASD keyboard emulation |
| **Guided calibration** | Button-driven, step-by-step — no fixed timers, confirm each step on your own pace |
| **Live status display** | 128×32 OLED shows wheel/pedal values, active mode, and held keys in real time |
| **Wheel lock** | Freeze wheel input with a button press — useful during a pause |
| **Radio failsafe** | Pedals default to "released" automatically if the wireless link drops for 500ms |

---

## Hardware

| Component | Role |
|---|---|
| Arduino Pro Micro (ATmega32u4) | Main unit — wheel input, pedal receiver, USB HID |
| Arduino Nano (ATmega328) | Pedal unit — reads pedals, radio transmitter |
| nRF24L01 ×2 | Wireless link between the two units |
| B10K potentiometer | Steering wheel |
| 5K linear potentiometer | Throttle pedal |
| Potentiometer (any) | Brake pedal |
| SSD1306 OLED, 128×32, I2C | Status and calibration display |
| DC-DC converter, 5V → 3.3V | Powers the nRF24L01 on the Pro Micro |
| Push button ×2 | Mode switch (D4), calibration / lock (D7) |

---

## Repository Structure

```
├── main_unit/                 Pro Micro — wheel, pedal receiver, display
│   ├── platformio.ini
│   └── src/
│       └── main.cpp
│
├── pedal_unit/                 Nano — pedals, radio transmitter
│   ├── platformio.ini
│   └── src/
│       └── main.cpp
│
└── README.md
```

> Each folder is an independent PlatformIO project. The two boards run different firmware and cannot share a single project.

---

## Wiring

### Main Unit — Pro Micro

| Signal | Pin |
|---|---|
| Steering wheel (potentiometer) | A0 |
| nRF24 — CE | 9 |
| nRF24 — CSN | 10 |
| nRF24 — MOSI | 16 |
| nRF24 — MISO | 14 |
| nRF24 — SCK | 15 |
| nRF24 — VCC | 3.3V, via DC-DC converter |
| OLED — SDA | 2 |
| OLED — SCL | 3 |
| Mode button | 4 → GND |
| Calibration / lock button | 7 → GND |

> **Note.** The Pro Micro's hardware SPI pins are **16 / 14 / 15**, not the 11 / 12 / 13 used on an Uno or Nano. This is the single most common wiring mistake on this board.

### Pedal Unit — Nano

| Signal | Pin |
|---|---|
| Throttle pedal | A0 |
| Brake pedal | A1 |
| nRF24 — CE | 9 |
| nRF24 — CSN | 10 |
| nRF24 — MOSI / MISO / SCK | 11 / 12 / 13 |
| nRF24 — VCC | 3.3V |

> **Note.** Place a 10–47µF electrolytic capacitor directly across each nRF24 module's VCC/GND pins. Without it, the radio link can be unstable or fail to establish entirely.

---

## Flashing

The project builds with **PlatformIO** rather than the Arduino IDE, which makes juggling two boards and two dependency sets considerably easier.

1. Install VS Code and the PlatformIO IDE extension.
2. Open `main_unit/` or `pedal_unit/` as its own project — *File → Open Folder*.
3. Connect the matching board.
4. Upload via the arrow icon at the bottom of VS Code, or:
   ```
   pio run --target upload
   ```

> **Note.** Keep the project path free of non-ASCII characters. A path containing accented, Cyrillic, or other non-ASCII characters can cause PlatformIO/avrdude to fail with path-related errors.

<details>
<summary><strong>Nano won't flash — "avrdude: stk500_getsync(): not in sync"</strong></summary>
<br>

Different clone batches ship with different bootloader versions. In `platformio.ini`, try switching:

```ini
board = nanoatmega328
```
to
```ini
board = nanoatmega328new
```
or vice versa.

</details>

---

## Usage

**First boot — calibration.** The display shows `CALIBRATION` with instructions for the current step. Perform the requested action — turn the wheel, press a pedal — then press the button on D7 to confirm and advance. Seven steps in total: wheel left / right / center, throttle released / pressed, brake released / pressed.

**Mode switch.** The button on D4 toggles between joystick and keyboard mode at any time, mid-session included. Any held keys are released automatically on switch, so nothing gets stuck.

**Wheel lock.** Once calibration is complete, the same D7 button changes role — it locks and unlocks wheel input, handy during a pause.

**On-screen status.** During normal operation the display shows live wheel/throttle/brake values in joystick mode, or the currently held keys in keyboard mode.

---

## Known Limitations

- The display library (**SSD1306Ascii**) renders ASCII only — all on-screen text is in English.
- Keyboard emulation may be restricted in anti-cheat-protected online titles; check the specific game's policy on input-device emulation before relying on it.

---

## Roadmap

<details>
<summary>Planned and possible future improvements</summary>
<br>

- AS5600 magnetic encoder in place of the wheel potentiometer — no mechanical wear, higher precision
- A gearbox on a dedicated linear potentiometer
- Quick-release wheel hub
- 3D-printed enclosure

</details>

---

## License

<div align="center">

Released under the **MIT License** — use, modify, and build on it freely.

</div>
