
#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

// LED piny
const int ledPins[8] = {3, 4, 5, 6, 7, 8, 9, 10};

// Kódy pre každú LED
const unsigned long codes[8] = {
  1497064,  // LED1
  1497060,  // LED2
  1497068,  // LED3 THIS ALL CHANGE TO YOUR CODES
  1497058,  // LED4
  1497066,  // LED5
  1497062,  // LED6
  1497063,  // LED7
  1497071   // LED8
};

// Stav každej LED (false = OFF, true = ON)
bool ledStates[8] = {false, false, false, false, false, false, false, false};

void setup() {
  Serial.begin(9600);

  mySwitch.enableReceive(0);  // Prijímač na pine D2 (interrupt 0)

  // Nastav piny LED ako výstupy
  for (int i = 0; i < 8; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);  // Začni so všetkými LED zhasnutými
  }
}

unsigned long lastPrint = 0;

void loop() {
  if (mySwitch.available()) {
    unsigned long receivedCode = mySwitch.getReceivedValue();
    Serial.print("Prijatý kód: ");
    Serial.println(receivedCode);

    for (int i = 0; i < 8; i++) {
      if (receivedCode == codes[i]) {
        ledStates[i] = !ledStates[i];
        digitalWrite(ledPins[i], ledStates[i] ? HIGH : LOW);
        Serial.print("Prepínam LED ");
        Serial.println(i + 1);
        break;
      }
    }

    mySwitch.resetAvailable();
  } else {
    // vypíš len každú sekundu
    if (millis() - lastPrint > 1000) {
      Serial.println(F("ASI NIE JE pripojeni RCSwitch prijimac!"));
      lastPrint = millis();
    }
  }
}
