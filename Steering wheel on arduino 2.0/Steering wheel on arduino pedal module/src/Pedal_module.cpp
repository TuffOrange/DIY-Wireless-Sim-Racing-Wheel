#include <SPI.h>
#include <RF24.h>

#define CE_PIN 9
#define CSN_PIN 10
RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "PEDAL";

#define THROTTLE_PIN A0
#define BRAKE_PIN A1

struct PedalData {
  int throttle;
  int brake;
};
PedalData data;

void setup() {
  radio.begin();
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.stopListening();
}

void loop() {
  data.throttle = analogRead(THROTTLE_PIN);
  data.brake = analogRead(BRAKE_PIN);
  
  radio.write(&data, sizeof(data));
  
  delay(15); // ~60 отправок в секунду
}