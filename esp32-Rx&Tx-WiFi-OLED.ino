#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RCSwitch.h>
#include <EEPROM.h>

// === Nastavenia ===
const char* ssid = "ESP32_Control";
const char* password = "12345678";

#define OLED_ADDRESS 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA_PIN 22
#define OLED_SCL_PIN 21
#define OLED_RESET -1

#define RX_PIN 2
#define TX_PIN 4
#define EEPROM_SIZE 512

// === Globálne premenné ===
RCSwitch mySwitch = RCSwitch();
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
AsyncWebServer server(80);

bool isReceiving = false;
unsigned long rxStartTime = 0;
long lastReceivedCode = -1;
int bitLength = 0;

// === HTML stránka ===
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="sk">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>ESP32 RF Control</title>
  <style>
    body {
      font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
      background: linear-gradient(135deg, #6a11cb 0%, #2575fc 100%);
      color: #333;
      margin: 0;
      padding: 20px;
      min-height: 100vh;
    }
    .container {
      max-width: 900px;
      margin: 0 auto;
      background: white;
      border-radius: 15px;
      box-shadow: 0 10px 30px rgba(0, 0, 0, 0.2);
      overflow: hidden;
    }
    header {
      background: #2c3e50;
      color: white;
      padding: 20px;
      text-align: center;
    }
    header h1 {
      margin: 0;
      font-size: 28px;
    }
    .content {
      padding: 20px;
    }
    .section {
      margin-bottom: 30px;
      padding: 20px;
      background: #f9f9f9;
      border-radius: 10px;
      border: 1px solid #e0e0e0;
    }
    .section h2 {
      margin-top: 0;
      color: #2c3e50;
      border-bottom: 2px solid #3498db;
      padding-bottom: 10px;
    }
    input[type="text"] {
      width: 100%;
      padding: 12px;
      margin: 10px 0;
      border: 2px solid #ddd;
      border-radius: 8px;
      font-size: 16px;
      box-sizing: border-box;
    }
    input[type="text"]:focus {
      border-color: #3498db;
      outline: none;
    }
    button {
      background: #3498db;
      color: white;
      border: none;
      padding: 12px 20px;
      margin: 5px;
      border-radius: 8px;
      cursor: pointer;
      font-size: 16px;
      transition: background 0.3s;
    }
    button:hover {
      background: #2980b9;
    }
    button.danger {
      background: #e74c3c;
    }
    button.danger:hover {
      background: #c0392b;
    }
    .placeholder {
      background: #fffde7;
      padding: 15px;
      border-radius: 8px;
      border: 1px solid #ffe082;
      font-size: 14px;
      line-height: 1.6;
      color: #5d4037;
    }
    .codes-list {
      max-height: 200px;
      overflow-y: auto;
      border: 1px solid #ddd;
      border-radius: 8px;
      padding: 10px;
      background: #f8f9fa;
    }
    .code-item {
      padding: 8px;
      margin: 5px 0;
      background: white;
      border: 1px solid #ddd;
      border-radius: 5px;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .code-item button {
      margin: 0;
      padding: 5px 10px;
      font-size: 14px;
    }
    .message {
      margin-top: 10px;
      padding: 10px;
      background: #d4edda;
      color: #155724;
      border: 1px solid #c3e6cb;
      border-radius: 5px;
      display: none;
    }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>|RF Control Panel</h1>
    </header>

    <div class="content">
      <!-- Input pre kód -->
      <div class="section">
        <h2>🔧 Manuálny vstup kódu</h2>
        <input type="text" id="codeInput" placeholder="Zadaj kód (napr. 1234567)" />
        <div class="placeholder">
          <strong>Podporované kódy:</strong> Celé čísla od 1 do 16777215 (24-bitové).<br>
          <strong>Nepodporované:</strong> Desatinné čísla, písmená, medzery, znaky ako -, +, @.<br>
          <strong>Príklad:</strong> 1234567 – OK | abc123 – ZLE | 0.5 – ZLE
        </div>
      </div>

      <!-- Tlačidlá pre odosielanie -->
      <div class="section">
        <h2>📤 Odoslanie kódu</h2>
        <button onclick="transmitCode()">Transmit</button>
        <button onclick="transmit3Times()">Transmit 3x (1s medzera)</button>
        <button onclick="startTransmitLoop()">Transmit Loop (ON)</button>
        <button onclick="stopTransmitLoop()" class="danger">Stop Loop</button>
      </div>

      <!-- Prijímanie a ukladanie -->
      <div class="section">
        <h2>📥 Prijímanie a ukladanie</h2>
        <button onclick="receiveAndSave()">Receive & Save</button>
        <button onclick="clearAllCodes()" class="danger">Vymazať všetky kódy</button>
      </div>

      <!-- Zoznam uložených kódov -->
      <div class="section">
        <h2>💾 Uložené kódy</h2>
        <div id="codesList" class="codes-list">
          Načítavam...
        </div>
      </div>

      <div id="message" class="message"></div>
    </div>
  </div>

  <script>
    let loopInterval = null;

    function showMessage(text, isError = false) {
      const msg = document.getElementById('message');
      msg.style.display = 'block';
      msg.textContent = text;
      msg.style.background = isError ? '#f8d7da' : '#d4edda';
      msg.style.color = isError ? '#721c24' : '#155724';
      setTimeout(() => msg.style.display = 'none', 3000);
    }

    function updateCodesList() {
      fetch('/list')
        .then(res => res.json())
        .then(codes => {
          const list = document.getElementById('codesList');
          list.innerHTML = '';
          if (codes.length === 0) {
            list.innerHTML = '<p>Žiadne uložené kódy.</p>';
            return;
          }
          codes.forEach(code => {
            const item = document.createElement('div');
            item.className = 'code-item';
            item.innerHTML = `
              <span>${code}</span>
              <button onclick="sendStored(${code})">Odoslať</button>
              <button onclick="deleteCode(${code})">Vymazať</button>
            `;
            list.appendChild(item);
          });
        });
    }

    function receiveAndSave() {
      fetch('/receive', { method: 'POST' })
        .then(res => res.text())
        .then(text => {
          showMessage(text);
          updateCodesList();
        })
        .catch(() => showMessage('Chyba pri prijímaní', true));
    }

    function transmitCode() {
      const code = document.getElementById('codeInput').value.trim();
      if (!code || isNaN(code) || code <= 0 || code > 16777215) {
        showMessage('Neplatný kód!', true);
        return;
      }
      fetch('/transmit', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'code=' + encodeURIComponent(code)
      }).then(() => showMessage('Kód odoslaný!'));
    }

    function transmit3Times() {
      const code = document.getElementById('codeInput').value.trim();
      if (!code || isNaN(code) || code <= 0 || code > 16777215) {
        showMessage('Neplatný kód!', true);
        return;
      }
      fetch('/transmit3', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'code=' + encodeURIComponent(code)
      }).then(() => showMessage('Odoslané 3x!'));
    }

    function startTransmitLoop() {
      const code = document.getElementById('codeInput').value.trim();
      if (!code || isNaN(code) || code <= 0 || code > 16777215) {
        showMessage('Neplatný kód!', true);
        return;
      }
      if (loopInterval) clearInterval(loopInterval);
      loopInterval = setInterval(() => {
        fetch('/transmit', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: 'code=' + encodeURIComponent(code)
        });
      }, 1000);
      showMessage('Loop spustený (1x za sekundu)');
    }

    function stopTransmitLoop() {
      if (loopInterval) clearInterval(loopInterval);
      loopInterval = null;
      showMessage('Loop zastavený');
    }

    function sendStored(code) {
      fetch('/transmit', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'code=' + encodeURIComponent(code)
      }).then(() => showMessage(`Odoslané: ${code}`));
    }

    function deleteCode(code) {
      fetch('/delete?code=' + code, { method: 'GET' })
        .then(() => {
          showMessage('Kód vymazaný');
          updateCodesList();
        });
    }

    function clearAllCodes() {
      if (confirm('Naozaj chceš vymazať všetky kódy?')) {
        fetch('/clear', { method: 'GET' })
          .then(() => {
            showMessage('Všetky kódy vymazané');
            updateCodesList();
          });
      }
    }

    // Načítaj kódy pri načítaní stránky
    updateCodesList();
  </script>
</body>
</html>
)rawliteral";

// === EEPROM správa kódov ===
#define MAX_CODES 20
long savedCodes[MAX_CODES];
int codeCount = 0;

void loadCodesFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  codeCount = 0;
  for (int i = 0; i < MAX_CODES; i++) {
    long code = EEPROM.readLong(i * sizeof(long));
    if (code != 0 && code != 0xFFFFFFFF) {
      savedCodes[codeCount++] = code;
    } else {
      break;
    }
  }
  EEPROM.end();
}

void saveCodeToEEPROM(long code) {
  if (codeCount >= MAX_CODES) {
    Serial.println("EEPROM plná!");
    return;
  }
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.writeLong(codeCount * sizeof(long), code);
  EEPROM.commit();
  EEPROM.end();
  savedCodes[codeCount++] = code;
}

void deleteCodeFromEEPROM(long code) {
  EEPROM.begin(EEPROM_SIZE);
  int index = -1;
  for (int i = 0; i < codeCount; i++) {
    if (savedCodes[i] == code) {
      index = i;
      break;
    }
  }
  if (index != -1) {
    // Posuň všetky kódy dozadu
    for (int i = index; i < codeCount - 1; i++) {
      savedCodes[i] = savedCodes[i + 1];
    }
    codeCount--;
    // Prepíš EEPROM
    for (int i = 0; i <= codeCount; i++) {
      EEPROM.writeLong(i * sizeof(long), savedCodes[i]);
    }
    EEPROM.commit();
  }
  EEPROM.end();
}

void clearAllCodesInEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < MAX_CODES; i++) {
    EEPROM.writeLong(i * sizeof(long), 0);
  }
  EEPROM.commit();
  EEPROM.end();
  codeCount = 0;
}

// === Inicializácia ===
void setup() {
  Serial.begin(115200);

  // OLED
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("OLED zlyhal"));
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Zapínam...");
  display.display();

  // WiFi
  WiFi.softAP(ssid, password);
  delay(500);
  display.println("WiFi OK");
  display.display();

  // RCSwitch
  mySwitch.enableReceive(RX_PIN);
  mySwitch.enableTransmit(TX_PIN);
  display.println("RCSwitch OK");
  display.display();

  // EEPROM
  loadCodesFromEEPROM();
  display.println("EEPROM načítané");
  display.display();

  // === Web server ===
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "[";
    for (int i = 0; i < codeCount; i++) {
      json += String(savedCodes[i]);
      if (i < codeCount - 1) json += ",";
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  server.on("/receive", HTTP_POST, [](AsyncWebServerRequest *request){
    isReceiving = true;
    rxStartTime = millis();
    request->send(200, "text/plain", "Prijímanie spustené...");
  });

  server.on("/transmit", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("code", true)) {
      String input = request->getParam("code", true)->value();
      long code = input.toInt();
      if (code > 0 && code <= 16777215) {
        mySwitch.send(code, 24);
        request->send(200, "text/plain", "Odoslané: " + String(code));
      } else {
        request->send(200, "text/plain", "Neplatný kód!");
      }
    } else {
      request->send(200, "text/plain", "Chýba kód!");
    }
  });

  server.on("/transmit3", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("code", true)) {
      String input = request->getParam("code", true)->value();
      long code = input.toInt();
      if (code > 0 && code <= 16777215) {
        for (int i = 0; i < 3; i++) {
          mySwitch.send(code, 24);
          delay(1000);
        }
        request->send(200, "text/plain", "Odoslané 3x");
      } else {
        request->send(200, "text/plain", "Neplatný kód!");
      }
    } else {
      request->send(200, "text/plain", "Chýba kód!");
    }
  });

  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("code")) {
      long code = request->getParam("code")->value().toInt();
      deleteCodeFromEEPROM(code);
      request->send(200, "text/plain", "Kód vymazaný");
    } else {
      request->send(200, "text/plain", "Chyba");
    }
  });

  server.on("/clear", HTTP_GET, [](AsyncWebServerRequest *request){
    clearAllCodesInEEPROM();
    request->send(200, "text/plain", "Všetko vymazané");
  });

  server.begin();
  display.println("Server spustený");
  display.display();
}

// === Loop ===
void loop() {
  // Prijímanie signálu
  if (isReceiving && mySwitch.available()) {
    long value = mySwitch.getReceivedValue();
    int bits = mySwitch.getReceivedBitlength();
    if (bits == 24 && value > 0) {
      saveCodeToEEPROM(value);
      lastReceivedCode = value;
      bitLength = bits;
      isReceiving = false;
    }
    mySwitch.resetAvailable();
  }

  // Timeout prijímania
  if (isReceiving && (millis() - rxStartTime) > 5000) {
    isReceiving = false;
  }

  delay(10); // Nutné pre WiFi a WDT
}
