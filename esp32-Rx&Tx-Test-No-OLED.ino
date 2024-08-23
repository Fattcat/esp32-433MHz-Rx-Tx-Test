#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

// This code ONLY Capture OR Transmit "24 Bit"

long lastReceivedCode = -1;  // Uložený kód pre TX mód
int bitLength = 0;          // Dĺžka bitov signálu
bool isReceiving = false;    // Flag pre RX mód
bool isTransmitting = false; // Flag pre TX mód
bool transmissionDone = false; // Flag na kontrolu dokončenia vysielania

void setup() {
  Serial.begin(9600);
  mySwitch.enableReceive(2); // Prijímanie signálov na pine 2
  mySwitch.enableTransmit(4); // Vysielanie signálov na pine 4
  Serial.println("System ready. Type 'rx' to start receiving, 'tx' to start transmitting, or 'clear' to stop and clear.");
}

void loop() {
  // Spracovanie príkazov zo sériového monitora
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim(); // Odstrániť biele znaky z oboch strán

    if (command.equalsIgnoreCase("rx")) {
      isReceiving = true;
      isTransmitting = false;
      transmissionDone = false; // Resetovať flag pre dokončenie vysielania
      Serial.println("Switched to RX mode.");
    }
    else if (command.equalsIgnoreCase("tx")) {
      isReceiving = false;
      isTransmitting = true;
      transmissionDone = false; // Resetovať flag pre dokončenie vysielania
      if (lastReceivedCode != -1) {
        transmitCode(lastReceivedCode);
      } else {
        Serial.println("No signal stored. Please receive a signal first.");
      }
    }
    else if (command.equalsIgnoreCase("clear")) {
      isReceiving = false;
      isTransmitting = false;
      transmissionDone = false;
      lastReceivedCode = -1;
      bitLength = 0;
      Serial.println("Cleared all modes and memory.");
    }
  }

  // Prijímanie signálov
  if (isReceiving && mySwitch.available()) {
    lastReceivedCode = mySwitch.getReceivedValue();
    bitLength = mySwitch.getReceivedBitlength();
    Serial.print("Captured signal: ");
    Serial.print(lastReceivedCode);
    Serial.print(" (");
    Serial.print(bitLength);
    Serial.println(" bits)");
    mySwitch.resetAvailable();
  }

  // Vysielanie signálov
  if (isTransmitting && !transmissionDone && lastReceivedCode != -1) {
    transmitCode(lastReceivedCode);
    transmissionDone = true; // Nastaviť flag pre dokončenie vysielania
  }
}

void transmitCode(long code) {
  if (bitLength == 0) bitLength = 24; // Predpokladáme predvolených 24 bitov, ak nebolo určené
  for (int i = 0; i < 3; i++) {
    mySwitch.send(code, bitLength); // Počet bitov je dynamický
    Serial.print("Transmitted signal: ");
    Serial.print(code);
    Serial.print(" (");
    Serial.print(bitLength);
    Serial.println(" bits)");
    delay(1000); // Čaká 1 sekundu pred ďalším vysielaním
  }
  Serial.println("Transmission complete.");
}
