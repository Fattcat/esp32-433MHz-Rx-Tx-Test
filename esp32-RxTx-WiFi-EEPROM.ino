#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <RCSwitch.h>
#include <EEPROM.h>

// === WiFi nastavenia ===
const char* ssid = "ESP32_Control";
const char* password = "12345678";

// === RCSwitch ===
#define RX_PIN 2
#define TX_PIN 4

// === Ukladanie kódov ===
#define MAX_CODES 20
#define CODE_ITEM_SIZE sizeof(CodeItem)
#define EEPROM_SIZE (MAX_CODES * CODE_ITEM_SIZE)

struct CodeItem {
  long code;
  char name[33]; // 32 znakov + \0
};

CodeItem savedCodes[MAX_CODES];
int codeCount = 0;

// === Globálne premenné ===
RCSwitch mySwitch = RCSwitch();
AsyncWebServer server(80);

bool isReceiving = false;
unsigned long receiveStartTime = 0;
long lastValidCode = -1;
String pendingName = "Unknown";

// === HTML stránka s progress bar a animáciou signálu ===
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
    .flex {
      display: flex;
      gap: 10px;
      align-items: center;
    }
    .flex input {
      flex: 1;
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

    /* Progress bar */
    .progress-container {
      width: 100%;
      background: #eee;
      border-radius: 10px;
      padding: 2px;
      margin: 10px 0;
    }
    .progress-bar {
      height: 20px;
      border-radius: 8px;
      background: linear-gradient(90deg, #4caf50, #8bc34a);
      text-align: center;
      color: white;
      font-size: 14px;
      line-height: 20px;
      transition: width 0.4s;
    }

    /* Animácia signálu */
    .signal-animation {
      height: 60px;
      margin: 15px 0;
      position: relative;
      background: #f0f0f0;
      border-radius: 8px;
      overflow: hidden;
    }
    .signal-wave {
      position: absolute;
      bottom: 0;
      width: 100%;
      height: 0;
      background: #2980b9;
      border-radius: 8px 8px 0 0;
      transition: height 0.2s ease;
    }

    .codes-list {
      max-height: 250px;
      overflow-y: auto;
      border: 1px solid #ddd;
      border-radius: 8px;
      padding: 10px;
      background: #f8f9fa;
    }
    .code-item {
      padding: 12px;
      margin: 8px 0;
      background: white;
      border: 1px solid #ddd;
      border-radius: 8px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      font-size: 14px;
      box-shadow: 0 1px 3px rgba(0,0,0,0.1);
    }
    .code-info {
      flex: 1;
    }
    .code-info strong {
      font-size: 16px;
      color: #2c3e50;
    }
    .code-info code {
      font-family: monospace;
      background: #f0f0f0;
      padding: 4px 8px;
      border-radius: 4px;
      color: #c0392b;
    }
    .code-actions {
      display: flex;
      gap: 5px;
    }
    .code-actions button {
      padding: 6px 10px;
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
      <!-- Stav pamäte -->
      <div class="section">
        <h2>📊 Stav pamäte</h2>
        <div>Použité: <span id="usedSlots">0</span> / 20 kódov</div>
        <div class="progress-container">
          <div class="progress-bar" id="progressBar" style="width:0%">0%</div>
        </div>
      </div>

      <!-- Manuálny vstup -->
      <div class="section">
        <h2>🔧 Manuálny vstup kódu</h2>
        <input type="text" id="codeInput" placeholder="Zadaj kód (napr. 1234567)" />
        <div class="placeholder">
          <strong>Podporované kódy:</strong> Celé čísla od 1 do 16777215 (24-bitové).<br>
          <strong>Nepodporované:</strong> Desatinné čísla, písmená, medzery, znaky ako -, +, @.<br>
          <strong>Príklad:</strong> 1234567 – OK | abc123 – ZLE | 0.5 – ZLE
        </div>
      </div>

      <!-- Prijímanie s menom -->
      <div class="section">
        <h2>📥 Prijímanie a ukladanie</h2>
        <div class="flex">
          <input type="text" id="nameInput" placeholder="Názov (napr. Garáž)" />
          <button onclick="receiveAndSave()" id="receiveBtn">Receive & Save</button>
        </div>
        <button onclick="clearAllCodes()" class="danger">Vymazať všetky kódy</button>

        <!-- Animácia signálu -->
        <h3>Prijatý signál</h3>
        <div class="signal-animation">
          <div class="signal-wave" id="signalWave"></div>
        </div>
      </div>

      <!-- Odosielanie -->
      <div class="section">
        <h2>📤 Odoslanie kódu</h2>
        <button onclick="transmitCode()">Transmit</button>
        <button onclick="transmit3Times()">Transmit 3x (1s medzera)</button>
        <button onclick="startTransmitLoop()">Transmit Loop (ON)</button>
        <button onclick="stopTransmitLoop()" class="danger">Stop Loop</button>
      </div>

      <!-- Zoznam kódov -->
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

    function updateMemoryUsage(used) {
      const percent = Math.round((used / 20) * 100);
      document.getElementById('usedSlots').textContent = used;
      document.getElementById('progressBar').style.width = percent + '%';
      document.getElementById('progressBar').textContent = percent + '%';
    }

    function updateSignalStrength(height) {
      document.getElementById('signalWave').style.height = height + 'px';
      setTimeout(() => {
        if (parseInt(document.getElementById('signalWave').style.height) === height)
          document.getElementById('signalWave').style.height = '0px';
      }, 300);
    }

    function updateCodesList() {
      fetch('/list')
        .then(res => res.json())
        .then(codes => {
          const list = document.getElementById('codesList');
          list.innerHTML = '';
          updateMemoryUsage(codes.length);

          if (codes.length === 0) {
            list.innerHTML = '<p>Žiadne uložené kódy.</p>';
            return;
          }
          codes.forEach(item => {
            const div = document.createElement('div');
            div.className = 'code-item';
            div.dataset.code = item.code;

            div.innerHTML = `
              <div class="code-info">
                <div><strong id="name-${item.code}">${item.name}</strong></div>
                <div style="font-family:monospace;color:#c0392b">Kód: ${item.code}</div>
                <input type="text" id="edit-${item.code}" value="${item.name}" 
                       style="display:none;margin-top:4px;padding:5px;width:100%" />
              </div>
              <div class="code-actions">
                <button onclick="useCode(${item.code})" title="Použiť">📋</button>
                <button onclick="sendStored(${item.code})" title="Odoslať">📤</button>
                <button onclick="startEdit(${item.code})" title="Upraviť">✎</button>
                <button onclick="saveEdit(${item.code})" style="display:none" title="Uložiť">✔️</button>
                <button onclick="deleteCode(${item.code})" class="danger" title="Vymazať">🗑️</button>
              </div>
            `;
            list.appendChild(div);
          });
        })
        .catch(() => showMessage('Chyba pri načítaní', true));
    }

    function startEdit(code) {
      document.getElementById(`name-${code}`).style.display = 'none';
      const input = document.getElementById(`edit-${code}`);
      input.style.display = 'block';
      input.focus();
      document.querySelector(`[onclick="saveEdit(${code})"]`).style.display = 'inline-block';
      document.querySelector(`[onclick="startEdit(${code})"]`).style.display = 'none';
    }

    function saveEdit(code) {
      const newName = document.getElementById(`edit-${code}`).value.trim() || 'Nezmenovaný';
      fetch('/updateName', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'code=' + code + '&name=' + encodeURIComponent(newName)
      })
      .then(() => {
        showMessage('Meno zmenené: ' + newName);
        updateCodesList();
      })
      .catch(() => showMessage('Chyba', true));
    }

    function useCode(code) {
      document.getElementById('codeInput').value = code;
      showMessage(`Kód ${code} použitý`);
    }

    function sendStored(code) {
      useCode(code);
      transmitCode();
    }

    function receiveAndSave() {
      const name = document.getElementById('nameInput').value.trim() || 'Nezmenovaný';
      const btn = document.getElementById('receiveBtn');
      btn.textContent = 'Prijímanie... (3s)';
      btn.disabled = true;

      fetch('/receive', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'name=' + encodeURIComponent(name)
      })
      .then(() => {
        showMessage('Hľadám signál...');
      })
      .catch(() => showMessage('Chyba', true))
      .finally(() => {
        setTimeout(() => {
          btn.textContent = 'Receive & Save';
          btn.disabled = false;
        }, 3000);
      });
    }

    function transmitCode() {
      const input = document.getElementById('codeInput').value.trim();
      if (!input || isNaN(input) || input <= 0 || input > 16777215) {
        showMessage('Neplatný kód!', true);
        return;
      }
      const code = parseInt(input);
      fetch('/transmit', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'code=' + code
      }).then(() => showMessage('Odoslané: ' + code));
    }

    function transmit3Times() {
      const input = document.getElementById('codeInput').value.trim();
      if (!input || isNaN(input) || input <= 0 || input > 16777215) {
        showMessage('Neplatný kód!', true);
        return;
      }
      const code = parseInt(input);
      fetch('/transmit3', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'code=' + code
      }).then(() => showMessage('Odoslané 3x: ' + code));
    }

    function startTransmitLoop() {
      const input = document.getElementById('codeInput').value.trim();
      if (!input || isNaN(input) || input <= 0 || input > 16777215) {
        showMessage('Neplatný kód!', true);
        return;
      }
      const code = parseInt(input);
      if (loopInterval) clearInterval(loopInterval);
      loopInterval = setInterval(() => {
        fetch('/transmit', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: 'code=' + code
        });
      }, 1000);
      showMessage('Loop: každú sekundu');
    }

    function stopTransmitLoop() {
      if (loopInterval) clearInterval(loopInterval);
      loopInterval = null;
      showMessage('Loop zastavený');
    }

    function deleteCode(code) {
      if (confirm('Vymazať tento kód?')) {
        fetch('/delete?code=' + code, { method: 'GET' })
          .then(() => {
            showMessage('Kód vymazaný');
            updateCodesList();
          });
      }
    }

    function clearAllCodes() {
      if (confirm('Vymazať všetky kódy?')) {
        fetch('/clear', { method: 'GET' })
          .then(() => {
            showMessage('Všetko vymazané');
            updateCodesList();
          });
      }
    }

    // Pripojenie na signál
    setInterval(() => {
      fetch('/lastSignal').then(r => r.json()).then(data => {
        if (data.received) {
          updateSignalStrength(40 + Math.random() * 20);
        }
      }).catch(() => {});
    }, 800);

    updateCodesList();
  </script>
</body>
</html>
)rawliteral";

// === EEPROM operácie ===
void loadCodesFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  codeCount = 0;
  for (int i = 0; i < MAX_CODES; i++) {
    CodeItem item;
    EEPROM.get(i * CODE_ITEM_SIZE, item);
    if (item.code != 0 && item.code != 0xFFFFFFFF) {
      savedCodes[codeCount++] = item;
    } else {
      break;
    }
  }
  EEPROM.end();
}

int findCodeIndex(long code) {
  for (int i = 0; i < codeCount; i++) {
    if (savedCodes[i].code == code) return i;
  }
  return -1;
}

void saveCodeToEEPROM(long code, const char* name) {
  if (codeCount >= MAX_CODES) return;
  CodeItem item;
  item.code = code;
  strncpy(item.name, name, 32);
  item.name[32] = '\0';

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(codeCount * CODE_ITEM_SIZE, item);
  EEPROM.commit();
  EEPROM.end();

  savedCodes[codeCount++] = item;
}

void updateNameInEEPROM(long code, const char* newName) {
  int index = findCodeIndex(code);
  if (index == -1) return;

  // Aktualizuj RAM
  strncpy(savedCodes[index].name, newName, 32);
  savedCodes[index].name[32] = '\0';

  // Aktualizuj EEPROM
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(index * CODE_ITEM_SIZE, savedCodes[index]);
  EEPROM.commit();
  EEPROM.end();
}

void deleteCodeFromEEPROM(long code) {
  int index = findCodeIndex(code);
  if (index == -1) return;

  EEPROM.begin(EEPROM_SIZE);
  for (int i = index; i < codeCount - 1; i++) {
    savedCodes[i] = savedCodes[i + 1];
  }
  codeCount--;

  for (int i = 0; i < MAX_CODES; i++) {
    if (i < codeCount) {
      EEPROM.put(i * CODE_ITEM_SIZE, savedCodes[i]);
    } else {
      CodeItem empty = {0, ""};
      EEPROM.put(i * CODE_ITEM_SIZE, empty);
    }
  }
  EEPROM.commit();
  EEPROM.end();
}

void clearAllCodesInEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < MAX_CODES; i++) {
    CodeItem empty = {0, ""};
    EEPROM.put(i * CODE_ITEM_SIZE, empty);
  }
  EEPROM.commit();
  EEPROM.end();
  codeCount = 0;
}

// === Stav EEPROM ===
void printEEPROMStatus() {
  int used = codeCount;
  int total = MAX_CODES;
  float percent = (float)used / total * 100;
  int usedBytes = used * CODE_ITEM_SIZE;
  int totalBytes = EEPROM_SIZE;

  Serial.println("\n--- EEPROM Stav ---");
  Serial.printf("Kódy: %d / %d (%.1f %%)\n", used, total, percent);
  Serial.printf("Záznam: %d B\n", CODE_ITEM_SIZE);
  Serial.printf("Použité: %d / %d B\n", usedBytes, totalBytes);
  Serial.println("-------------------");
}

// === Setup ===
void setup() {
  Serial.begin(115200);

  // WiFi
  WiFi.softAP(ssid, password);
  Serial.println("");
  Serial.print("WiFi AP: ");
  Serial.println(ssid);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  // RCSwitch
  mySwitch.enableReceive(RX_PIN);
  mySwitch.enableTransmit(TX_PIN);
  Serial.println("RCSwitch: RX=2, TX=4");

  // EEPROM
  loadCodesFromEEPROM();
  printEEPROMStatus();

  // Web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "[";
    for (int i = 0; i < codeCount; i++) {
      json += "{\"name\":\"" + String(savedCodes[i].name) + "\",\"code\":" + String(savedCodes[i].code) + "}";
      if (i < codeCount - 1) json += ",";
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  bool signalReceived = false;
  server.on("/lastSignal", HTTP_GET, [&signalReceived](AsyncWebServerRequest *request){
    String json = "{\"received\":" + String(signalReceived ? "true" : "false") + "}";
    signalReceived = false;
    request->send(200, "application/json", json);
  });

  server.on("/receive", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("name", true)) {
      pendingName = request->getParam("name", true)->value();
    } else {
      pendingName = "Nezmenovaný";
    }
    isReceiving = true;
    lastValidCode = -1;
    receiveStartTime = millis();
    request->send(200, "text/plain", "Prijímanie (3s)...");
  });

  server.on("/transmit", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("code", true)) {
      long code = request->getParam("code", true)->value().toInt();
      if (code > 0 && code <= 16777215) {
        mySwitch.send(code, 24);
        request->send(200, "text/plain", "Odoslané");
      } else {
        request->send(200, "text/plain", "Neplatný kód");
      }
    } else {
      request->send(200, "text/plain", "Chýba kód");
    }
  });

  server.on("/transmit3", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("code", true)) {
      long code = request->getParam("code", true)->value().toInt();
      if (code > 0 && code <= 16777215) {
        for (int i = 0; i < 3; i++) {
          mySwitch.send(code, 24);
          delay(1000);
        }
        request->send(200, "text/plain", "Odoslané 3x");
      } else {
        request->send(200, "text/plain", "Neplatný kód");
      }
    } else {
      request->send(200, "text/plain", "Chýba kód");
    }
  });

  server.on("/updateName", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("code") && request->hasParam("name")) {
      long code = request->getParam("code")->value().toInt();
      String name = request->getParam("name")->value();
      updateNameInEEPROM(code, name.c_str());
      request->send(200, "text/plain", "Meno aktualizované");
      printEEPROMStatus();
    } else {
      request->send(200, "text/plain", "Chyba");
    }
  });

  server.on("/delete", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("code")) {
      long code = request->getParam("code")->value().toInt();
      deleteCodeFromEEPROM(code);
      request->send(200, "text/plain", "Vymazané");
      printEEPROMStatus();
    } else {
      request->send(200, "text/plain", "Chyba");
    }
  });

  server.on("/clear", HTTP_GET, [](AsyncWebServerRequest *request){
    clearAllCodesInEEPROM();
    request->send(200, "text/plain", "Všetko vymazané");
    printEEPROMStatus();
  });

  server.begin();
  Serial.println("Server: http://192.168.4.1");
}

// === Loop ===
void loop() {
  if (isReceiving && (millis() - receiveStartTime) < 3000) {
    if (mySwitch.available()) {
      long value = mySwitch.getReceivedValue();
      int bits = mySwitch.getReceivedBitlength();
      if (bits == 24 && value > 0) {
        lastValidCode = value;
        Serial.printf("📡 Zachytený signál: %ld\n", value);
      }
      mySwitch.resetAvailable();
    }
  } else if (isReceiving) {
    isReceiving = false;
    if (lastValidCode != -1) {
      saveCodeToEEPROM(lastValidCode, pendingName.c_str());
      Serial.printf("✅ Uložený: %ld (%s)\n", lastValidCode, pendingName.c_str());
      printEEPROMStatus();
    } else {
      Serial.println("❌ Žiadny signál");
    }
  }
  delay(10);
}
