#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RCSwitch.h>
#include <WiFi.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// OLED displej
#define OLED_ADDRESS 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA_PIN 22
#define OLED_SCL_PIN 21
#define OLED_RESET -1

// Tlačidlá
#define RX_BUTTON_PIN 14
#define TX_BUTTON_PIN 12
#define CLEAR_BUTTON_PIN 13
#define ROLLINGCODE_BUTTON_PIN 27  // Nové tlačidlo pre RollingCode

// SD karta
#define CS_PIN 5
#define FILENAME "/KeyFob-SignalCodes.txt"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RCSwitch mySwitch = RCSwitch();

// Globálne premenné
long lastReceivedCode = -1;
int bitLength = 0;
bool isReceiving = false;

void setup() {
  Serial.begin(9600);
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  pinMode(RX_BUTTON_PIN, INPUT_PULLUP);
  pinMode(TX_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CLEAR_BUTTON_PIN, INPUT_PULLUP);
  pinMode(ROLLINGCODE_BUTTON_PIN, INPUT_PULLUP);  // Inicializácia RollingCode tlačidla

  mySwitch.enableReceive(2);
  mySwitch.enableTransmit(4);

  if (!SD.begin(CS_PIN)) {
    Serial.println("SD card initialization failed!");
    updateDisplay("SD init failed!");
    for (;;);
  }
  Serial.println("SD card initialized.");
  updateDisplay("SD card initialized.");
  displayMenu();
}

void loop() {
  if (digitalRead(RX_BUTTON_PIN) == LOW) {
    isReceiving = true;
    updateDisplay("Receiving...");
    delay(500);
  }

  if (digitalRead(TX_BUTTON_PIN) == LOW) {
    if (lastReceivedCode != -1) {
      transmitCode(lastReceivedCode);
    } else {
      updateDisplay("No signal stored.");
      delay(2000);
      displayMenu();
    }
    delay(500);
  }

  if (digitalRead(CLEAR_BUTTON_PIN) == LOW) {
    lastReceivedCode = -1;
    updateDisplay("Cleared all modes.");
    delay(2000);
    displayMenu();
    delay(500);
  }

  if (digitalRead(ROLLINGCODE_BUTTON_PIN) == LOW) {
    updateDisplay("Rolling codes...");
    delay(500);  // Anti-bounce delay
    transmitRollingCodes();  // Zavoláme funkciu na odosielanie kódov
  }

  if (isReceiving) {
    if (mySwitch.available()) {
      long receivedValue = mySwitch.getReceivedValue();
      int receivedBitLength = mySwitch.getReceivedBitlength();
      if (receivedBitLength == 24) {
        lastReceivedCode = receivedValue;
        bitLength = receivedBitLength;
        updateDisplay("Captured: " + String(lastReceivedCode) + " (" + String(bitLength) + " bits)");
        delay(2000);
      } else {
        updateDisplay("Invalid signal.");
        delay(2000);
      }
      mySwitch.resetAvailable();
    }
  }
}

void transmitCode(long code) {
  if (bitLength == 0) bitLength = 24;
  for (int i = 0; i < 2; i++) {
    mySwitch.send(code, bitLength);
    updateDisplay("Transmitted: " + String(code));
    delay(1000);
  }
  updateDisplay("Transmission complete.");
  delay(2000);
  displayMenu();
}

void transmitRollingCodes() {
  for (unsigned long long code = 1; code <= 9999999999; code++) {
    String formattedCode = String(code);
    while (formattedCode.length() < 10) {
      formattedCode = "0" + formattedCode;
    }
    
    mySwitch.send(formattedCode.toInt(), 24);
    updateDisplay("Transmitting: " + formattedCode);
    delay(100);
  }
  updateDisplay("Rolling codes complete.");
  delay(2000);
  displayMenu();
}

void updateDisplay(String message) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println(message);
  display.display();
}

void displayMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Creator: Fattcat");
  display.println("System ready.");
  display.println("Use buttons:");
  display.println("RX: Receive");
  display.println("TX: Transmit");
  display.println("CLEAR: Clear signal");
  display.display();
}