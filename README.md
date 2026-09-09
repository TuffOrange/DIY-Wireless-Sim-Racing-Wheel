# DIY-Wireless-Sim-Racing-Wheel
DIY Wireless Sim Racing Wheel which is homemade opportunity to play games with steering wheel controller which is cool aint it? Cause Im mentioning not only racing games it supports ALL games
A homemade steering wheel with pedals built on Arduino: wireless pedals (nRF24L01), a switchable joystick/keyboard mode (for rally sims and games without wheel support), an OLED display with button-driven step-by-step calibration, and a wheel-lock function.

 Features
Analog steering wheel (potentiometer) with noise smoothing and adjustable deadzone
Throttle and brake pedals are wireless (nRF24L01) — no need to run a cable from the floor to the desk
Two operating modes, switchable on the fly with a button:
Joystick — full analog input, for rally sims
Keyboard (WASD) — for games without wheel support
OLED display (128×32, I2C):
Step-by-step calibration triggered by a button (no more waiting on 3-second timers)
Real-time status: wheel/throttle/brake position (joystick mode) or currently pressed keys (keyboard mode)
Wheel-lock button (active after calibration) — freezes input, handy during a pause
Automatic failsafe on radio loss — pedals are treated as released if no signal for 500ms
🛠 Hardware
Component	Purpose
Arduino Pro Micro (ATmega32u4)	Main unit — wheel, pedal data receiver, USB HID to PC
Arduino Nano (ATmega328)	Pedal unit — reads potentiometers, transmits over radio
nRF24L01 ×2	Wireless link between the two units
B10K potentiometer	Steering wheel
5K linear potentiometer	Throttle pedal
Potentiometer (any)	Brake pedal
OLED 128×32 (SSD1306, I2C)	Status / calibration display
DC-DC converter (5V→3.3V)	Powers the nRF24L01 on the Pro Micro (no onboard 3.3V pin)
Button ×2	Mode switch (D4) and calibration/lock (D7)
 Repository structure
├── main_unit/              # Pro Micro — wheel + pedal receiver + display
│   ├── platformio.ini
│   └── src/
│       └── main.cpp
├── pedal_unit/              # Nano — pedals + radio transmitter
│   ├── platformio.ini
│   └── src/
│       └── main.cpp
└── README.md

Each folder is a separate PlatformIO project (different boards = different firmware, they can't be combined into one project).

 Wiring
Main unit (Pro Micro)
Signal	Pin
Steering wheel (potentiometer)	A0
nRF24 CE	9
nRF24 CSN	10
nRF24 MOSI	16 (not 11!)
nRF24 MISO	14 (not 12!)
nRF24 SCK	15 (not 13!)
nRF24 VCC	3.3V (via DC-DC converter)
OLED SDA	2
OLED SCL	3
Mode button	4 → GND
Calibration/lock button	7 → GND

 The Pro Micro has a non-standard hardware SPI pinout — not 11/12/13 like on an Uno/Nano, but 16/14/15.

Pedal unit (Nano)
Signal	Pin
Throttle pedal	A0
Brake pedal	A1
nRF24 CE	9
nRF24 CSN	10
nRF24 MOSI/MISO/SCK	11/12/13 (standard)
nRF24 VCC	3.3V

 On both nRF24 modules, add a capacitor (10-47µF electrolytic) right across VCC/GND, as close to the module as possible — without it the link can be unstable or fail to establish at all.
 Flashing

The project is built with PlatformIO (VS Code extension), not the Arduino IDE — it's much easier to work with two different boards and different libraries side by side this way.

Install VS Code + the PlatformIO IDE extension
Open main_unit/ or pedal_unit/ as a separate project (File → Open Folder)
Connect the corresponding board
Click Upload (the arrow at the bottom of VS Code) or run pio run --target upload

Important: keep the project path free of non-ASCII characters (e.g. D:\projects\..., not a path containing accented/Cyrillic/etc. characters) — otherwise PlatformIO/avrdude can fail with path-related errors.

If the Nano won't flash (avrdude: stk500_getsync(): not in sync), try switching board = nanoatmega328 ↔ nanoatmega328new in platformio.ini — different clone batches ship with different bootloader versions.

 Usage
First boot — calibration

On power-up the display shows "CALIBRATION" with instructions for the current step. Perform the action (turn the wheel the requested way / press the pedal) and press the button on D7 — the step is confirmed and it moves to the next one. Seven steps total: wheel (left/right/center), throttle (released/pressed), brake (released/pressed).

Mode switch

The button on D4 toggles between joystick and keyboard mode at any time, including mid-game. On switching, any held keys are automatically released so nothing stays "stuck."

Wheel lock

After calibration, the button on D7 changes function — it now locks/unlocks wheel input (e.g. during a pause).

On-screen status

During normal use, the display shows live wheel/throttle/brake values (joystick mode) or which keys are currently held (keyboard mode).

 Known limitations
The display library (SSD1306Ascii) only supports ASCII — all on-screen text is in English, Cyrillic (or other non-ASCII text) won't render
Don't use keyboard emulation in anti-cheat-protected online games without checking that game/platform's rules around gamepad/input-device support first
🗺 Possible future improvements
AS5600 magnetic encoder instead of a wheel potentiometer (no physical wear, more precise)
A gearbox on a separate linear potentiometer
Quick-release wheel hub
3D-printed enclosure (in progress)
