// Test with controlling with 3 BUTTONS and 0.96" Oled Display instead of Serial Monitor.


#include <RCSwitch.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

RCSwitch mySwitch = RCSwitch();

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Definovanie tlačidiel
#define RX_BUTTON_PIN 14
#define TX_BUTTON_PIN 12
#define CLEAR_BUTTON_PIN 13

// Globálne premenné
long lastReceivedCode = -1;
int bitLength = 0;
bool isReceiving = false;
bool isTransmitting = false;
bool transmissionDone = false;

void setup() {
  // Inicializácia OLED displeja
  if (!display.begin(SSD1306_I2C_ADDRESS, OLED_RESET)) {
    for (;;); // Ak zlyhá inicializácia, zastav program
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("System ready.");
  display.println("Use buttons to:");
  display.println("RX: Receive");
  display.println("TX: Transmit");
  display.println("CLEAR: Clear");
  display.display();

  // Inicializácia tlačidiel
  pinMode(RX_BUTTON_PIN, INPUT_PULLUP);
  pinMode(TX_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CLEAR_BUTTON_PIN, INPUT_PULLUP);

  // Inicializácia RCSwitch
  mySwitch.enableReceive(2);
  mySwitch.enableTransmit(4);
}

void loop() {
  // Kontrola tlačidiel
  if (digitalRead(RX_BUTTON_PIN) == LOW) {
    isReceiving = true;
    isTransmitting = false;
    transmissionDone = false;
    updateDisplay("Switched to RX mode.");
    delay(500); // Anti-bounce delay
  }

  if (digitalRead(TX_BUTTON_PIN) == LOW) {
    isReceiving = false;
    isTransmitting = true;
    transmissionDone = false;
    if (lastReceivedCode != -1) {
      transmitCode(lastReceivedCode);
    } else {
      updateDisplay("No signal stored.");
    }
    delay(500); // Anti-bounce delay
  }

  if (digitalRead(CLEAR_BUTTON_PIN) == LOW) {
    isReceiving = false;
    isTransmitting = false;
    transmissionDone = false;
    lastReceivedCode = -1;
    bitLength = 0;
    updateDisplay("Cleared all modes.");
    delay(500); // Anti-bounce delay
  }

  // Prijímanie signálov
  if (isReceiving && mySwitch.available()) {
    lastReceivedCode = mySwitch.getReceivedValue();
    bitLength = mySwitch.getReceivedBitlength();
    updateDisplay("Captured signal: " + String(lastReceivedCode) + " (" + String(bitLength) + " bits)");
    mySwitch.resetAvailable();
  }

  // Vysielanie signálov
  if (isTransmitting && !transmissionDone && lastReceivedCode != -1) {
    transmitCode(lastReceivedCode);
    transmissionDone = true;
  }
}

void transmitCode(long code) {
  if (bitLength == 0) bitLength = 24;
  for (int i = 0; i < 3; i++) {
    mySwitch.send(code, bitLength);
    updateDisplay("Transmitted: " + String(code) + " (" + String(bitLength) + " bits)");
    delay(1000);
  }
  updateDisplay("Transmission complete.");
}

void updateDisplay(String message) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(message);
  display.display();
}
