// This code support Controlling Code TX via WiFi so u need to use Mobile Phone or Notebook
// Code is there BUT It works ONLY 1 st time and then it will automatically RESTART The ESP32
// IDK WHYYY

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RCSwitch.h>
// ------------------------------------------------------------------
// Watch INFO UP !
// -----------------------------------------------------
// Watch INFO UP !
// --------------------------------------------

// Konfigurácia WiFi
const char* ssid = "esp32-WiFi";
const char* password = "esp32-PassWord";

// HTML kód pre web stránku
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Control</title>
  <style>
    body {
      display: flex;
      justify-content: center;
      align-items: center;
      height: 100vh;
      background-color: #f0f0f0;
      font-family: Arial, sans-serif;
    }
    .container {
      text-align: center;
      background-color: white;
      padding: 20px;
      border-radius: 8px;
      box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);
    }
    input[type="text"] {
      padding: 10px;
      width: 100%;
      margin: 10px 0;
      font-size: 18px;
    }
    button {
      padding: 10px 20px;
      font-size: 18px;
      color: white;
      background-color: green;
      border: none;
      cursor: pointer;
      border-radius: 5px;
    }
    .message {
      margin-top: 20px;
      font-size: 16px;
      color: red;
    }
  </style>
</head>
<body>
  <div class="container">
    <h2>Type Code for Transmit</h2>
    <input type="text" id="codeInput" placeholder="Enter code">
    <button onclick="sendCode()">TX</button>
    <div id="message" class="message"></div>
  </div>

  <script>
    function sendCode() {
      const code = document.getElementById('codeInput').value;
      const xhr = new XMLHttpRequest();
      xhr.open("POST", "/transmit", true);
      xhr.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
      xhr.onload = function() {
        if (xhr.status === 200) {
          document.getElementById('message').innerText = xhr.responseText;
        } else {
          document.getElementById('message').innerText = "Server error";
        }
      };
      xhr.send("code=" + encodeURIComponent(code));
    }
  </script>
</body>
</html>
)rawliteral";

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

// Globálne premenné
bool isReceiving = false;
bool isTransmitting = false;
bool transmissionDone = false;
long lastReceivedCode = -1;
int bitLength = 0;
unsigned long rxStartTime = 0;

// Vytvorenie inštancie SSD1306 displeja
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RCSwitch mySwitch = RCSwitch();

AsyncWebServer server(80); // Web server na porte 80

void setup() {
  Serial.begin(115200); // Zmenená rýchlosť

  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true); // Zastaví vykonávanie v nekonečnej slučke
  }

  pinMode(RX_BUTTON_PIN, INPUT_PULLUP);
  pinMode(TX_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CLEAR_BUTTON_PIN, INPUT_PULLUP);

  mySwitch.enableReceive(2);
  mySwitch.enableTransmit(4);

  displayMenu();

  WiFi.softAP(ssid, password);
  Serial.println("Access Point Created");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);  // Zobrazí web stránku
  });

  server.on("/transmit", HTTP_POST, [](AsyncWebServerRequest *request){
    String inputMessage;
    if (request->hasParam("code", true)) {
      inputMessage = request->getParam("code", true)->value();
      long code = inputMessage.toInt();
      if (code > 0) {
        transmitCode(code);
        request->send(200, "text/plain", "kód odoslaný");
      } else {
        request->send(200, "text/plain", "kód neodoslaný ! skús to znova !");
      }
    } else {
      request->send(200, "text/plain", "kód neodoslaný ! skús to znova !");
    }
  });

  server.begin();
}

void loop() {
  handleButtons();
  handleRFSignals();
}

void handleButtons() {
  if (digitalRead(RX_BUTTON_PIN) == LOW) {
    isReceiving = true;
    rxStartTime = millis();
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
    isReceiving = false;
    isTransmitting = false;
    transmissionDone = false;
    lastReceivedCode = -1;
    bitLength = 0;
    updateDisplay("Cleared all modes.");
    delay(2000);
    displayMenu();
    delay(500);
  }
}

void handleRFSignals() {
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

      if (millis() - rxStartTime > 5000) {
        isReceiving = false;
        if (lastReceivedCode != -1) {
          updateDisplay("Saving signal...");
          delay(2000);
        } else {
          updateDisplay("No signal received.");
          delay(2000);
        }
        displayMenu();
      }
    }
  }
}

void transmitCode(long code) {
  if (bitLength == 0) bitLength = 24;
  for (int i = 0; i < 2; i++) {
    mySwitch.send(code, bitLength);
    updateDisplay("Transmitted: " + String(code) + "\n (" + String(bitLength) + " bits)");
    delay(1000);
  }
  updateDisplay("Transmission complete.");
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
  display.println("Creator : Fattcat");
  display.println("github.com/Fattcat");
  display.println("System ready.");
  display.println("Use buttons:");
  display.println("RX: Receive");
  display.println("TX: Transmit");
  display.println("CLEAR: Clear signal");
  display.display();
}
