#include <SPI.h>
#include <RF24.h>
#include <Joystick.h>
#include "Keyboard.h"
#include <Wire.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>

// ===== ДИСПЛЕЙ (без буфера в RAM - экономично для Pro Micro) =====
// ВАЖНО: SSD1306Ascii поддерживает только ASCII - весь текст на дисплее только на английском!
#define OLED_ADDR 0x3C
SSD1306AsciiWire oled;

// ===== РЕЖИМЫ РАБОТЫ (джойстик/клавиатура) =====
#define MODE_JOYSTICK 0
#define MODE_KEYBOARD 1
byte currentMode = MODE_JOYSTICK;

#define MODE_BUTTON_PIN 4
bool lastModeButtonState = HIGH;
unsigned long lastModeButtonPress = 0;

// ===== КНОПКА КАЛИБРОВКИ / БЛОКИРОВКИ РУЛЯ =====
#define CALIB_BUTTON_PIN 7
bool lastCalibButtonState = HIGH;
unsigned long lastCalibButtonPress = 0;
#define BUTTON_DEBOUNCE 250

bool calibrationDone = false;
bool steeringLocked = false;

enum CalibStep {
  CAL_STEER_LEFT,
  CAL_STEER_RIGHT,
  CAL_STEER_CENTER,
  CAL_THROTTLE_MIN,
  CAL_THROTTLE_MAX,
  CAL_BRAKE_MIN,
  CAL_BRAKE_MAX,
  CAL_DONE_STEP
};
CalibStep calibStep = CAL_STEER_LEFT;

// ===== РАДИО (nRF24L01) =====
#define CE_PIN 9
#define CSN_PIN 10
RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "PEDAL";

struct PedalData {
  int throttle;
  int brake;
};
PedalData pedalData = {0, 0};
unsigned long lastRadioReceive = 0;
#define RADIO_TIMEOUT 500

// ===== ДЖОЙСТИК =====
Joystick_ Joystick(JOYSTICK_DEFAULT_REPORT_ID, JOYSTICK_TYPE_GAMEPAD,
                   0, 0,
                   true, false, false,
                   false, false, false,
                   false, true, false,
                   true, false);

#define STEERING_PIN A0

// ===== СГЛАЖИВАНИЕ РУЛЯ =====
#define SMOOTH_READINGS 8
int steerReadings[SMOOTH_READINGS];
int steerTotal = 0;
byte readIndex = 0;

struct CalibData { int min, max, center; };
CalibData steering = {0, 1023, 512};
CalibData throttle = {1023, 0, 0};
CalibData brake = {1023, 0, 0};

#define STEERING_DEADBAND 40
#define PEDAL_DEADBAND 15
#define MIN_CHANGE 5
#define UPDATE_INTERVAL 20

int lastSteeringValue = 0;
int lastThrottleValue = 0;
int lastBrakeValue = 0;
unsigned long lastUpdateTime = 0;

unsigned long lastDisplayUpdate = 0;
#define DISPLAY_UPDATE_INTERVAL 200

#define KB_STEER_THRESHOLD 300
#define KB_PEDAL_THRESHOLD 200
bool keyA = false, keyD = false, keyW = false, keyS = false;

// Прототипы (нужны для PlatformIO)
void checkModeButton();
void checkCalibButton();
void advanceCalibration();
void showCalibStep();
void updateStatusDisplay(int steer, int thr, int brk);
void receiveRadioData();
void outputJoystick(int steer, int thr, int brk);
void outputKeyboard(int steer, int thr, int brk);
void releaseAllKeys();
int convertSteering(int value);
int convertPedal(int value, CalibData* calib);
int getAverage(int pin);
int getAveragePedalThrottle();
int getAveragePedalBrake();

void setup() {
  Serial.begin(9600);
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CALIB_BUTTON_PIN, INPUT_PULLUP);

  Wire.begin();
  oled.begin(&Adafruit128x32, OLED_ADDR);
  oled.setFont(Adafruit5x7);
  oled.clear();

  radio.begin();
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.startListening();

  Joystick.begin(false);
  Joystick.setXAxisRange(-1000, 1000);
  Joystick.setThrottleRange(0, 1000);
  Joystick.setBrakeRange(0, 1000);
  Keyboard.begin();

  memset(steerReadings, 0, sizeof(steerReadings));

  showCalibStep(); // первый шаг калибровки сразу на экран
}

void loop() {
  checkModeButton();
  checkCalibButton();
  receiveRadioData();

  if (!calibrationDone) {
    return; // ждём, пока откалибруют кнопкой
  }

  if (millis() - lastUpdateTime < UPDATE_INTERVAL) return;
  lastUpdateTime = millis();

  int rawSteer = analogRead(STEERING_PIN);
  steerTotal -= steerReadings[readIndex];
  steerReadings[readIndex] = rawSteer;
  steerTotal += rawSteer;
  readIndex = (readIndex + 1) % SMOOTH_READINGS;
  int smoothSteer = steerTotal / SMOOTH_READINGS;

  int steeringValue = steeringLocked ? lastSteeringValue : convertSteering(smoothSteer);
  int throttleValue = convertPedal(pedalData.throttle, &throttle);
  int brakeValue = convertPedal(pedalData.brake, &brake);

  if (currentMode == MODE_JOYSTICK) {
    outputJoystick(steeringValue, throttleValue, brakeValue);
  } else {
    outputKeyboard(steeringValue, throttleValue, brakeValue);
  }

  updateStatusDisplay(steeringValue, throttleValue, brakeValue);
}

void checkModeButton() {
  bool state = digitalRead(MODE_BUTTON_PIN);
  if (state == LOW && lastModeButtonState == HIGH && millis() - lastModeButtonPress > BUTTON_DEBOUNCE) {
    releaseAllKeys();
    currentMode = (currentMode == MODE_JOYSTICK) ? MODE_KEYBOARD : MODE_JOYSTICK;
    lastModeButtonPress = millis();
  }
  lastModeButtonState = state;
}

void checkCalibButton() {
  bool state = digitalRead(CALIB_BUTTON_PIN);
  if (state == LOW && lastCalibButtonState == HIGH && millis() - lastCalibButtonPress > BUTTON_DEBOUNCE) {
    lastCalibButtonPress = millis();
    if (!calibrationDone) {
      advanceCalibration();
    } else {
      // после калибровки кнопка работает как блокировка руля
      steeringLocked = !steeringLocked;
      oled.clear();
      oled.println(steeringLocked ? F("WHEEL LOCKED") : F("Wheel active"));
    }
  }
  lastCalibButtonState = state;
}

void advanceCalibration() {
  oled.clear();
  oled.println(F("Measuring..."));

  switch (calibStep) {
    case CAL_STEER_LEFT:
      steering.min = getAverage(STEERING_PIN);
      calibStep = CAL_STEER_RIGHT;
      break;
    case CAL_STEER_RIGHT:
      steering.max = getAverage(STEERING_PIN);
      calibStep = CAL_STEER_CENTER;
      break;
    case CAL_STEER_CENTER:
      steering.center = getAverage(STEERING_PIN);
      calibStep = CAL_THROTTLE_MIN;
      break;
    case CAL_THROTTLE_MIN:
      throttle.min = getAveragePedalThrottle();
      calibStep = CAL_THROTTLE_MAX;
      break;
    case CAL_THROTTLE_MAX:
      throttle.max = getAveragePedalThrottle();
      calibStep = CAL_BRAKE_MIN;
      break;
    case CAL_BRAKE_MIN:
      brake.min = getAveragePedalBrake();
      calibStep = CAL_BRAKE_MAX;
      break;
    case CAL_BRAKE_MAX:
      brake.max = getAveragePedalBrake();
      calibStep = CAL_DONE_STEP;
      break;
    default:
      break;
  }

  if (calibStep == CAL_DONE_STEP) {
    calibrationDone = true;
    oled.clear();
    oled.println(F("Calibration"));
    oled.println(F("complete!"));
    oled.println(F(""));
    oled.println(F("Button now"));
    oled.println(F("locks wheel"));
    delay(1500); // время прочитать сообщение, дальше экран перейдёт в статус
  } else {
    showCalibStep();
  }
}

void updateStatusDisplay(int steer, int thr, int brk) {
  if (millis() - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL) return;
  lastDisplayUpdate = millis();

  oled.clear();

  if (steeringLocked) {
    oled.println(F("WHEEL LOCKED"));
    oled.print(F("Gas:"));
    oled.print(thr);
    oled.print(F(" Brk:"));
    oled.println(brk);
    return;
  }

  if (currentMode == MODE_JOYSTICK) {
    oled.println(F("MODE: JOYSTICK"));
    oled.print(F("Wheel: "));
    oled.println(steer);
    oled.print(F("Gas:"));
    oled.print(thr);
    oled.print(F(" Brk:"));
    oled.println(brk);
  } else {
    oled.println(F("MODE: KEYBOARD"));
    oled.print(keyA ? F("A ") : F("- "));
    oled.print(keyD ? F("D ") : F("- "));
    oled.print(keyW ? F("W ") : F("- "));
    oled.println(keyS ? F("S") : F("-"));
  }
}

void showCalibStep() {
  oled.clear();
  oled.println(F("CALIBRATION"));
  oled.println(F(""));
  switch (calibStep) {
    case CAL_STEER_LEFT:
      oled.println(F("Wheel LEFT,"));
      oled.println(F("press button"));
      break;
    case CAL_STEER_RIGHT:
      oled.println(F("Wheel RIGHT,"));
      oled.println(F("press button"));
      break;
    case CAL_STEER_CENTER:
      oled.println(F("Wheel CENTER,"));
      oled.println(F("press button"));
      break;
    case CAL_THROTTLE_MIN:
      oled.println(F("Throttle OFF,"));
      oled.println(F("press button"));
      break;
    case CAL_THROTTLE_MAX:
      oled.println(F("Throttle FULL,"));
      oled.println(F("press button"));
      break;
    case CAL_BRAKE_MIN:
      oled.println(F("Brake OFF,"));
      oled.println(F("press button"));
      break;
    case CAL_BRAKE_MAX:
      oled.println(F("Brake FULL,"));
      oled.println(F("press button"));
      break;
    default:
      break;
  }
}

void receiveRadioData() {
  if (radio.available()) {
    radio.read(&pedalData, sizeof(pedalData));
    lastRadioReceive = millis();
  }
  if (millis() - lastRadioReceive > RADIO_TIMEOUT) {
    pedalData.throttle = throttle.min;
    pedalData.brake = brake.min;
  }
}

void outputJoystick(int steer, int thr, int brk) {
  bool updated = false;
  if (abs(steer - lastSteeringValue) >= MIN_CHANGE) {
    Joystick.setXAxis(steer);
    lastSteeringValue = steer;
    updated = true;
  }
  if (abs(thr - lastThrottleValue) >= MIN_CHANGE) {
    Joystick.setThrottle(thr);
    lastThrottleValue = thr;
    updated = true;
  }
  if (abs(brk - lastBrakeValue) >= MIN_CHANGE) {
    Joystick.setBrake(brk);
    lastBrakeValue = brk;
    updated = true;
  }
  if (updated) Joystick.sendState();
}

void outputKeyboard(int steer, int thr, int brk) {
  if (steer < -KB_STEER_THRESHOLD && !keyA) {
    Keyboard.press('a'); keyA = true;
  } else if (steer >= -KB_STEER_THRESHOLD && keyA) {
    Keyboard.release('a'); keyA = false;
  }
  if (steer > KB_STEER_THRESHOLD && !keyD) {
    Keyboard.press('d'); keyD = true;
  } else if (steer <= KB_STEER_THRESHOLD && keyD) {
    Keyboard.release('d'); keyD = false;
  }
  if (thr > KB_PEDAL_THRESHOLD && !keyW) {
    Keyboard.press('w'); keyW = true;
  } else if (thr <= KB_PEDAL_THRESHOLD && keyW) {
    Keyboard.release('w'); keyW = false;
  }
  if (brk > KB_PEDAL_THRESHOLD && !keyS) {
    Keyboard.press('s'); keyS = true;
  } else if (brk <= KB_PEDAL_THRESHOLD && keyS) {
    Keyboard.release('s'); keyS = false;
  }
}

void releaseAllKeys() {
  if (keyA) { Keyboard.release('a'); keyA = false; }
  if (keyD) { Keyboard.release('d'); keyD = false; }
  if (keyW) { Keyboard.release('w'); keyW = false; }
  if (keyS) { Keyboard.release('s'); keyS = false; }
}

int convertSteering(int value) {
  int deviation = value - steering.center;
  if (abs(deviation) < STEERING_DEADBAND) return 0;
  if (value > steering.center + STEERING_DEADBAND) {
    int adjustedMax = steering.max - steering.center - STEERING_DEADBAND;
    int adjustedValue = value - steering.center - STEERING_DEADBAND;
    return constrain(map(adjustedValue, 0, adjustedMax, 0, 1000), 0, 1000);
  } else {
    int adjustedMin = steering.center - steering.min - STEERING_DEADBAND;
    int adjustedValue = steering.center - STEERING_DEADBAND - value;
    return constrain(map(adjustedValue, 0, adjustedMin, 0, -1000), -1000, 0);
  }
}

int convertPedal(int value, CalibData* calib) {
  bool inverted = (calib->min > calib->max);
  if (inverted) {
    if (abs(value - calib->min) < PEDAL_DEADBAND) return 0;
    return constrain(map(value, calib->max, calib->min - PEDAL_DEADBAND, 1000, 0), 0, 1000);
  } else {
    if (abs(value - calib->min) < PEDAL_DEADBAND) return 0;
    return constrain(map(value, calib->min + PEDAL_DEADBAND, calib->max, 0, 1000), 0, 1000);
  }
}

int getAverage(int pin) {
  long sum = 0;
  for (int i = 0; i < 20; i++) {
    sum += analogRead(pin);
    delay(15);
  }
  return sum / 20;
}

int getAveragePedalThrottle() {
  long sum = 0;
  int count = 0;
  unsigned long start = millis();
  while (millis() - start < 300) {
    receiveRadioData();
    sum += pedalData.throttle;
    count++;
    delay(15);
  }
  return count > 0 ? sum / count : pedalData.throttle;
}

int getAveragePedalBrake() {
  long sum = 0;
  int count = 0;
  unsigned long start = millis();
  while (millis() - start < 300) {
    receiveRadioData();
    sum += pedalData.brake;
    count++;
    delay(15);
  }
  return count > 0 ? sum / count : pedalData.brake;
}