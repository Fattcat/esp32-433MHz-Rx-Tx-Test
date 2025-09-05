#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RCSwitch.h>

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
long lastReceivedCode = -1;
int bitLength = 0;
unsigned long rxStartTime = 0;
unsigned long lastButtonPress = 0;

// Na správu stavu displeja
enum DisplayState {
  MENU,
  MESSAGE
} displayState = MENU;

String currentMessage = "";
unsigned long messageTimeout = 0;

// Vytvorenie inštancie SSD1306 displeja
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RCSwitch mySwitch = RCSwitch();

AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);

  // Inicializácia I2C pre OLED
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true);
  }

  // Tlačidlá s pull-up
  pinMode(RX_BUTTON_PIN, INPUT_PULLUP);
  pinMode(TX_BUTTON_PIN, INPUT_PULLUP);
  pinMode(CLEAR_BUTTON_PIN, INPUT_PULLUP);

  // RCSwitch – prijímač na pin 2, vysielač na pin 4
  mySwitch.enableReceive(2);
  mySwitch.enableTransmit(4);

  // Spustenie WiFi AP
  WiFi.softAP(ssid, password);
  Serial.println("Access Point Created");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // Web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/transmit", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("code", true)) {
      String inputMessage = request->getParam("code", true)->value();
      long code = inputMessage.toInt();
      if (code > 0) {
        // Nepoužívame delay – odosielame cez vlákno alebo jednorázovo
        transmitCodeAsync(code);  // Asynchrónne odoslanie
        request->send(200, "text/plain", "Kód odoslaný");
      } else {
        request->send(200, "text/plain", "Neplatný kód!");
      }
    } else {
      request->send(200, "text/plain", "Chýba kód!");
    }
  });

  server.begin();

  displayMenu();
}

// Globálne premené pre asynchrónne odosielanie
bool isTransmittingAsync = false;
long codeToTransmit = 0;
int transmitCount = 0;
unsigned long lastTransmitTime = 0;

void transmitCodeAsync(long code) {
  codeToTransmit = code;
  transmitCount = 0;
  isTransmittingAsync = true;
  lastTransmitTime = millis();
}

void handleAsyncTransmit() {
  if (isTransmittingAsync && millis() - lastTransmitTime >= 1000) {
    mySwitch.send(codeToTransmit, 24);
    transmitCount++;
    updateDisplay("TX: " + String(codeToTransmit) + " (" + String(transmitCount) + "/2)");

    if (transmitCount >= 2) {
      isTransmittingAsync = false;
      messageTimeout = millis();
      currentMessage = "Odoslané!";
      displayState = MESSAGE;
    } else {
      lastTransmitTime = millis(); // ďalší krok
    }
  }
}

void handleButtons() {
  unsigned long now = millis();

  if (digitalRead(RX_BUTTON_PIN) == LOW && now - lastButtonPress > 500) {
    isReceiving = true;
    rxStartTime = now;
    updateDisplay("Prijímanie...");
    lastButtonPress = now;
  }

  if (digitalRead(TX_BUTTON_PIN) == LOW && now - lastButtonPress > 500) {
    if (lastReceivedCode != -1) {
      transmitCodeAsync(lastReceivedCode);
    } else {
      showMessage("Žiadny kód!", 2000);
    }
    lastButtonPress = now;
  }

  if (digitalRead(CLEAR_BUTTON_PIN) == LOW && now - lastButtonPress > 500) {
    isReceiving = false;
    lastReceivedCode = -1;
    bitLength = 0;
    showMessage("Vymazané.", 2000);
    lastButtonPress = now;
  }
}

void handleRFSignals() {
  if (isReceiving && mySwitch.available()) {
    long receivedValue = mySwitch.getReceivedValue();
    int receivedBitLength = mySwitch.getReceivedBitlength();

    if (receivedBitLength == 24) {
      lastReceivedCode = receivedValue;
      bitLength = receivedBitLength;
      showMessage("Zachytené: " + String(receivedValue), 2000);
    } else {
      showMessage("Neplatný signál!", 2000);
    }

    mySwitch.resetAvailable();

    // Ukonči prijímanie po 5s alebo ihneď po zachytení
    if (millis() - rxStartTime > 5000) {
      isReceiving = false;
      if (lastReceivedCode == -1) {
        showMessage("Žiadny signál.", 2000);
      } else {
        showMessage("Uložené.", 2000);
      }
    }
  }
}

void showMessage(String msg, unsigned long duration) {
  currentMessage = msg;
  messageTimeout = millis() + duration;
  displayState = MESSAGE;
}

void updateDisplay(String message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
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
  display.println("Tlacidla:");
  display.println("RX: Prijimaj");
  display.println("TX: Odošli");
  display.println("CLEAR: Vymaž");
  display.display();
}

void loop() {
  handleButtons();
  handleRFSignals();
  handleAsyncTransmit();

  // Spravuj displej
  if (displayState == MESSAGE && millis() > messageTimeout) {
    displayState = MENU;
    displayMenu();
  }

  // Dôležité: uvoľni procesor pre WiFi a watchdog
  delay(10); // Malé oneskorenie na prevádzku WDT a WiFi stacku
}
