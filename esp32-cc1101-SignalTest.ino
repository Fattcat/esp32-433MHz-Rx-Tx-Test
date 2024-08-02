#include <ELECHOUSE_CC1101.h>

// ----------------------
// CONNECTION :
// cc1101 --> esp32
// VCC -->3.3V
// GND --> GND
// MISO --> GPIO19
// MOSI -->	GPIO23
// SCK --> GPIO18
// CSN --> GPIO5
// GDO0 --> GPIO4
// ----------------------

long lastReceivedCode = -1;  // Uložený kód pre TX mód
int bitLength = 0;          // Dĺžka bitov signálu
bool isReceiving = false;    // Flag pre RX mód
bool isTransmitting = false; // Flag pre TX mód
bool transmissionDone = false; // Flag na kontrolu dokončenia vysielania

void setup() {
  Serial.begin(9600);

  // Inicializácia CC1101
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setCCMode(1);       // Nastaviť na správny mód
  ELECHOUSE_cc1101.setMHZ(433.92);     // Nastaviť frekvenciu (v závislosti od použitého modulu)
  
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
      ELECHOUSE_cc1101.SetReceive(); // Nastaviť CC1101 do RX módu
      Serial.println("Switched to RX mode.");
    }
    else if (command.equalsIgnoreCase("tx")) {
      isReceiving = false;
      isTransmitting = true;
      transmissionDone = false; // Resetovať flag pre dokončenie vysielania
      ELECHOUSE_cc1101.SetTX(); // Nastaviť CC1101 do TX módu
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
      ELECHOUSE_cc1101.setSidle(); // Nastaviť CC1101 do IDLE módu
      Serial.println("Cleared all modes and memory.");
    }
  }

  // Prijímanie signálov
  if (isReceiving) {
    byte receiveBuffer[64];  // Buffer pre prijímanie dát
    int packetLength = ELECHOUSE_cc1101.ReceiveData(receiveBuffer);

    if (packetLength > 0) {
      lastReceivedCode = decodeReceivedData(receiveBuffer, packetLength);
      bitLength = packetLength * 8; // Počet bitov v prijatom balíku
      Serial.print("Captured signal: ");
      Serial.print(lastReceivedCode);
      Serial.print(" (");
      Serial.print(bitLength);
      Serial.println(" bits)");
    }
  }

  // Vysielanie signálov
  if (isTransmitting && !transmissionDone && lastReceivedCode != -1) {
    transmitCode(lastReceivedCode);
    transmissionDone = true; // Nastaviť flag pre dokončenie vysielania
  }
}

long decodeReceivedData(byte* data, int length) {
  // Implementujte dekódovanie podľa vašich potrieb
  long code = 0;
  // Dekódovanie dát do čísla (závisí od použitého protokolu)
  for (int i = 0; i < length; i++) {
    code = (code << 8) | data[i];
  }
  return code;
}

void transmitCode(long code) {
  if (bitLength == 0) bitLength = 24; // Predpokladáme predvolených 24 bitov, ak nebolo určené
  byte transmitBuffer[4];
  // Naplniť buffer dátami
  for (int i = 3; i >= 0; i--) {
    transmitBuffer[i] = code & 0xFF;
    code >>= 8;
  }

  for (int i = 0; i < 3; i++) {
    ELECHOUSE_cc1101.SendData(transmitBuffer, 4); // Odoslať dáta (4 bajty)
    Serial.print("Transmitted signal: ");
    Serial.print(lastReceivedCode);
    Serial.print(" (");
    Serial.print(bitLength);
    Serial.println(" bits)");
    delay(1000); // Čaká 1 sekundu pred ďalším vysielaním
  }
  Serial.println("Transmission complete.");
}
