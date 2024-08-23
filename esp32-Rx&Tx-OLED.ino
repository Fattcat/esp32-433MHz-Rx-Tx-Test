#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RCSwitch.h>
//#include <WiFi.h>

// OLED displej
#define OLED_ADDRESS 0x3C        // I2C adresa OLED displeja
#define SCREEN_WIDTH 128         // Šírka obrazovky v pixeloch
#define SCREEN_HEIGHT 64         // Výška obrazovky v pixeloch
#define OLED_SDA_PIN 22          // Pin pre SDA (I2C data)
#define OLED_SCL_PIN 21          // Pin pre SCL (I2C clock)
#define OLED_RESET -1            // Reset pin (nepoužíva sa, -1)

// Tlačidlá
#define RX_BUTTON_PIN 14
#define TX_BUTTON_PIN 12
#define CLEAR_BUTTON_PIN 13

// Vytvorenie inštancie SSD1306 displeja
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RCSwitch mySwitch = RCSwitch();

// Globálne premenné
long lastReceivedCode = -1;
int bitLength = 0;
bool isReceiving = false;
bool isTransmitting = false;
bool transmissionDone = false;
unsigned long rxStartTime = 0;

void setup() {
  // Inicializácia sériového monitoru (na debugovanie)
  Serial.begin(9600);

  // Inicializácia I2C komunikácie s definovanými pinmi
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);

  // Inicializácia displeja s nastavenou adresou
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Zastaví vykonávanie v nekonečnej slučke
  }

  // Inicializácia tlačidiel
  pinMode(RX_BUTTON_PIN, INPUT_PULLUP);
  pinMode(TX_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CLEAR_BUTTON_PIN, INPUT_PULLUP);

  // Inicializácia RCSwitch
  mySwitch.enableReceive(2); // Prijímanie signálov na pine 2
  mySwitch.enableTransmit(4); // Vysielanie signálov na pine 4

  // Vyčistenie displeja a zobrazenie úvodného menu
  displayMenu();
}

void loop() {
  // Kontrola tlačidiel
  if (digitalRead(RX_BUTTON_PIN) == LOW) {
    isReceiving = true;
    rxStartTime = millis(); // Začiatok časovača
    updateDisplay("Receiving...");
    delay(500); // Anti-bounce delay
  }

  if (digitalRead(TX_BUTTON_PIN) == LOW) {
    if (lastReceivedCode != -1) {
      transmitCode(lastReceivedCode);
    } else {
      updateDisplay("No signal stored.");
      delay(2000); // Zobrazenie správy po dobu 2 sekúnd
      displayMenu(); // Zobrazenie úvodného menu
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
    delay(2000); // Zobrazenie správy po dobu 2 sekúnd
    displayMenu(); // Zobrazenie úvodného menu
    delay(500); // Anti-bounce delay
  }

  // Prijímanie signálov
  if (isReceiving) {
    if (mySwitch.available()) {
      long receivedValue = mySwitch.getReceivedValue();
      int receivedBitLength = mySwitch.getReceivedBitlength();

      // Overenie, či je signál 24-bitový
      if (receivedBitLength == 24) {
        lastReceivedCode = receivedValue;
        bitLength = receivedBitLength;
        updateDisplay("Captured: " + String(lastReceivedCode) + " (" + String(bitLength) + " bits)");
        delay(2000); // Zobrazenie zachyteného signálu po dobu 2 sekúnd
      } else {
        updateDisplay("Invalid signal.");
        delay(2000); // Zobrazenie správy po dobu 2 sekúnd
      }

      mySwitch.resetAvailable();
    }

    // Kontrola časovača
    if (millis() - rxStartTime > 5000) {
      isReceiving = false; // Zastavenie prijímania po 5 sekundách
      if (lastReceivedCode != -1) {
        updateDisplay("Saving signal...");
        delay(2000); // Zobrazenie správy po dobu 2 sekúnd
        displayMenu(); // Zobrazenie úvodného menu
      } else {
        updateDisplay("No signal received.");
        delay(2000); // Zobrazenie správy po dobu 2 sekúnd
        displayMenu(); // Zobrazenie úvodného menu
      }
    }
  }
}

void transmitCode(long code) {
  if (bitLength == 0) bitLength = 24; // Predpokladáme predvolených 24 bitov, ak nebolo určené
  for (int i = 0; i < 2; i++) {
    mySwitch.send(code, bitLength); // Počet bitov je dynamický
    updateDisplay("Transmitted: " + String(code) + "\n (" + String(bitLength) + " bits)");
    delay(1000); // Čaká 1 sekundu pred ďalším vysielaním
  }
  updateDisplay("Transmission complete.");
  delay(2000); // Zobrazenie správy po dobu 2 sekúnd
  displayMenu(); // Zobrazenie úvodného menu
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
  display.println("Creator : Fattcat");
  display.println("github.com/Fattcat");
  display.println("System ready.");
  display.println("Use buttons:");
  display.println("RX: Receive");
  display.println("TX: Transmit");
  display.println("CLEAR: Clear signal");
  display.display();
}
