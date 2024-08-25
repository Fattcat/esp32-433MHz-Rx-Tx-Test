#include <ELECHOUSE_CC1101_SRC_DRV.h>

// ----------------------
// CONNECTION :
// cc1101 --> esp32
// VCC -->3.3V
// GND --> GND
// MISO --> GPIO19
// MOSI --> GPIO23
// SCK --> GPIO18
// CSN --> GPIO5
// GDO0 --> GPIO4
// ----------------------

// Define the pins used for CC1101 communication
#define CC1101_MISO 19
#define CC1101_MOSI 23
#define CC1101_SCK 18
#define CC1101_CSN 5
#define CC1101_GDO0 4r  // Define GDO0 pin

long lastReceivedCode = -1;  // Stored code for TX mode
int bitLength = 0;           // Length of signal bits
bool isReceiving = false;    // Flag for RX mode
bool isTransmitting = false; // Flag for TX mode

void setup() {
  Serial.begin(9600);

  // Initialize CC1101 with specific pin configuration
  ELECHOUSE_cc1101.setGDO0(CC1101_GDO0);  // Set GDO0 pin
  ELECHOUSE_cc1101.setSpiPin(CC1101_SCK, CC1101_MISO, CC1101_MOSI, CC1101_CSN);
  ELECHOUSE_cc1101.Init();
  ELECHOUSE_cc1101.setCCMode(1);       // Set to correct mode
  ELECHOUSE_cc1101.setMHZ(433.92);     // Set frequency (depends on the module used)
  
  Serial.println("System ready. Type 'rx' to start receiving, 'tx' to start transmitting, or 'clear' to stop and clear.");
}

void loop() {
  // Command processing from Serial Monitor
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim(); // Remove white spaces from both ends

    if (command.equalsIgnoreCase("rx")) {
      isReceiving = true;
      isTransmitting = false;
      ELECHOUSE_cc1101.SetRx(); // Set CC1101 to RX mode
      Serial.println("Switched to RX mode. Listening for 5 seconds...");
      
      unsigned long startTime = millis();  // Get the current time
      bool signalCaptured = false;

      while (millis() - startTime < 5000) {  // Wait for 5 seconds
        if (isReceiving) {
          signalCaptured = tryCaptureSignal();
          if (signalCaptured) {
            Serial.println("Signal was successfully captured and saved.");
            break;
          }
        }
      }

      if (!signalCaptured) {
        Serial.println("Signal was not captured.");
      }

      ELECHOUSE_cc1101.setSidle(); // Set CC1101 to IDLE mode after listening
      isReceiving = false;  // Reset flag
    }
    else if (command.equalsIgnoreCase("tx")) {
      isReceiving = false;
      isTransmitting = true;
      if (lastReceivedCode != -1) {
        transmitCode(lastReceivedCode);
        Serial.println("Transmission initiated.");
      } else {
        Serial.println("No signal stored. Please receive a signal first.");
      }
    }
    else if (command.equalsIgnoreCase("clear")) {
      isReceiving = false;
      isTransmitting = false;
      lastReceivedCode = -1;
      bitLength = 0;
      ELECHOUSE_cc1101.setSidle(); // Set CC1101 to IDLE mode
      Serial.println("Cleared all modes and memory.");
    }
  }
}

bool tryCaptureSignal() {
  byte receiveBuffer[64];  // Buffer for receiving data
  int packetLength = ELECHOUSE_cc1101.ReceiveData(receiveBuffer);

  if (packetLength > 0) {
    lastReceivedCode = decodeReceivedData(receiveBuffer, packetLength);
    bitLength = packetLength * 8; // Number of bits in the received packet
    Serial.print("Captured signal: ");
    Serial.print(lastReceivedCode);
    Serial.print(" (");
    Serial.print(bitLength);
    Serial.println(" bits)");
    return true; // Signal captured successfully
  }
  return false; // No signal captured
}

long decodeReceivedData(byte* data, int length) {
  // Implement decoding according to your needs
  long code = 0;
  // Decoding data into a number (depends on the protocol used)
  for (int i = 0; i < length; i++) {
    code = (code << 8) | data[i];
  }
  return code;
}

void transmitCode(long code) {
  if (bitLength == 0) bitLength = 24; // Assume default 24 bits if not specified
  byte transmitBuffer[4];
  // Fill buffer with data
  for (int i = 3; i >= 0; i--) {
    transmitBuffer[i] = code & 0xFF;
    code >>= 8;
  }

  ELECHOUSE_cc1101.SetTx(); // Set CC1101 to TX mode
  for (int i = 0; i < 3; i++) {
    ELECHOUSE_cc1101.SendData(transmitBuffer, 4); // Send data (4 bytes)
    Serial.print("Transmitted signal: ");
    Serial.print(lastReceivedCode);
    Serial.print(" (");
    Serial.print(bitLength);
    Serial.println(" bits)");
    delay(1000); // Wait 1 second before the next transmission
  }
  ELECHOUSE_cc1101.setSidle(); // Set CC1101 back to IDLE mode after transmission
  Serial.println("Transmission complete.");
}
