#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

void setup() {
  Serial.begin(9600);
  mySwitch.enableTransmit(10); // Transmit pin je D10
  mySwitch.setRepeatTransmit(10); // Nastavenie počtu opakovaní na spoľahlivejší prenos
  Serial.println("Transmitter ready. Enter a message:");
}

void loop() {
  if (Serial.available() > 0) {
    String message = Serial.readStringUntil('\n'); // Načíta správu zo Serial Monitora
    message.trim(); // Odstráni biele znaky

    if (message.length() > 0) {
      // Konvertujeme správu na číselný kód
      unsigned long code = 0;
      for (int i = 0; i < message.length(); i++) {
        code = (code * 256) + message[i];
      }

      Serial.print("Sending: ");
      Serial.println(code);
      mySwitch.send(code, 24); // Pošle 24-bitový binárny kód
    }
  }
}
