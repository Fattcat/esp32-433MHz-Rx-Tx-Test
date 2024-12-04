#include <RCSwitch.h>
RCSwitch mySwitch = RCSwitch();
void setup() {
  Serial.begin(9600);
  mySwitch.enableReceive(0); // Prijímač je pripojený na D2
  Serial.println("Receiver ready.");
}

void loop() {
  if (mySwitch.available()) {
    unsigned long receivedCode = mySwitch.getReceivedValue();

    if (receivedCode == 0) {
      Serial.println("Unknown encoding");
    } else {
      Serial.print("Received code: ");
      Serial.println(receivedCode);

      // Dekóduje prijatý kód späť na text
      String message = "";
      while (receivedCode > 0) {
        char c = receivedCode % 256;
        message = c + message;
        receivedCode /= 256;
      }

      Serial.print("Decoded message: ");
      Serial.println(message);
    }

    mySwitch.resetAvailable(); // Reset prijímača
  }
}
