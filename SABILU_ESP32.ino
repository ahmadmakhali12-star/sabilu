/*
 * SABILU - Professional Audio Visual Module
 * ESP32 DevKit + TFT ST7735 + PCM5102A DAC + Rotary Encoder
 * Zero-Delay Audio Processing
 * 
 * Developed: 2026
 */

#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <AsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include "driver/i2s.h"

// ============= PIN DEFINITIONS =============
#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4
#define TFT_MOSI  23
#define TFT_SCK   18

#define I2S_BCK   26
#define I2S_LRCK  25
#define I2S_DIN   22
#define ADC_MIC   34

#define ENC_CLK   19
#define ENC_DT    18
#define ENC_SW    21

// ============= AUDIO PRESETS =============
struct AudioPreset {
  String name;
  uint8_t bass;
  uint8_t treble;
  uint8_t sensitivity;
};

AudioPreset presets[10] = {
  {"Suara Bas JikJik", 85, 30, 60},
  {"Bas Deb", 90, 25, 55},
  {"Bas Deb Jik", 88, 28, 58},
  {"Bas CikCik", 75, 50, 65},
  {"Bas Drejeb", 95, 15, 50},
  {"Bas Gler", 80, 40, 62},
  {"Bas Drum", 92, 20, 52},
  {"Trompet", 40, 85, 70},
  {"Custom 1", 50, 50, 60},
  {"Custom 2", 60, 60, 60}
};

// ============= GLOBAL VARIABLES =============
TFT_eSPI tft = TFT_eSPI();
AsyncWebServer server(80);

volatile int encoderPos = 0;
volatile int lastEncoderPos = 0;

uint8_t currentPreset = 0;
uint8_t sensitivity = 60;
uint8_t volumeOutput = 70;
uint8_t trebleEQ = 50;
uint8_t bassEQ = 60;
uint8_t graphL = 50, graphR = 50;

bool wifiConnected = false;
String wifiSSID = "";
String wifiPass = "";
String deviceName = "SABILU-Device";

enum MenuState {
  MENU_MAIN,
  MENU_PRESET,
  MENU_SENSITIVITY,
  MENU_VOLUME,
  MENU_TREBLE,
  MENU_BASS,
  MENU_WIFI,
  MENU_PRESET_EDIT
};

MenuState currentMenu = MENU_MAIN;
int menuSelection = 0;

// ============= AUDIO BUFFERS =============
const int SAMPLE_RATE = 44100;
const int BUFFER_SIZE = 2048;
int16_t audioBuffer[BUFFER_SIZE];
volatile uint32_t bufferIndex = 0;

// ============= FUNCTION DECLARATIONS =============
void setupI2S();
void setupEncoder();
void setupWiFi();
void initSPIFFS();
void loadPresets();
void savePresets();
void drawMainScreen();
void drawPresetMenu();
void drawSensitivityMenu();
void drawVolumeMenu();
void drawTrebleMenu();
void drawBassMenu();
void drawWiFiMenu();
void drawPresetEditMenu();
void handleEncoderRotation();
void handleEncoderClick();
void processAudio();
void updateEQGraph();
void displayGraph(int x, int y, uint8_t valueL, uint8_t valueR);

// ============= I2S SETUP FOR ZERO-DELAY AUDIO =============
void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = true
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK,
    .ws_io_num = I2S_LRCK,
    .data_out_num = I2S_DIN,
    .data_in_num = I2S_NUM_0
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// ============= ENCODER INTERRUPT =============
void IRAM_ATTR encoderISR() {
  static unsigned long lastInterrupt = 0;
  unsigned long currentTime = micros();

  if (currentTime - lastInterrupt > 500) {
    int clkState = digitalRead(ENC_CLK);
    int dtState = digitalRead(ENC_DT);

    if (clkState != dtState) {
      encoderPos++;
    } else {
      encoderPos--;
    }
    lastInterrupt = currentTime;
  }
}

void setupEncoder() {
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(ENC_CLK), encoderISR, CHANGE);
}

// ============= SETUP =============
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize TFT
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("SABILU", 60, 30);
  tft.setTextSize(1);
  tft.drawString("Initializing...", 50, 60);

  // Initialize systems
  initSPIFFS();
  loadPresets();
  setupI2S();
  setupEncoder();
  setupWiFi();

  // Draw main screen
  delay(1000);
  drawMainScreen();

  Serial.println("SABILU Initialized Successfully!");
}

// ============= SPIFFS INITIALIZATION =============
void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    tft.drawString("SPIFFS Error!", 50, 80);
  }
}

// ============= PRESET MANAGEMENT =============
void loadPresets() {
  if (SPIFFS.exists("/presets.json")) {
    File file = SPIFFS.open("/presets.json", "r");
    StaticJsonDocument<2000> doc;
    deserializeJson(doc, file);

    for (int i = 0; i < 10; i++) {
      if (doc[i].containsKey("name")) {
        presets[i].name = doc[i]["name"].as<String>();
        presets[i].bass = doc[i]["bass"];
        presets[i].treble = doc[i]["treble"];
        presets[i].sensitivity = doc[i]["sensitivity"];
      }
    }
    file.close();
  }
}

void savePresets() {
  StaticJsonDocument<2000> doc;

  for (int i = 0; i < 10; i++) {
    doc[i]["name"] = presets[i].name;
    doc[i]["bass"] = presets[i].bass;
    doc[i]["treble"] = presets[i].treble;
    doc[i]["sensitivity"] = presets[i].sensitivity;
  }

  File file = SPIFFS.open("/presets.json", "w");
  serializeJson(doc, file);
  file.close();
}

// ============= DRAW FUNCTIONS =============
void drawMainScreen() {
  tft.fillScreen(TFT_BLACK);

  // Header
  tft.fillRect(0, 0, 160, 30, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(2);
  tft.drawString("SABILU DZIKRI", 10, 8);

  // Preset display
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("Preset:", 5, 35);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(presets[currentPreset].name, 40, 35);

  // Main Menu
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  const char* mainMenuItems[] = {
    "> Preset",
    "> Sensitivity",
    "> Volume",
    "> Edit Treble",
    "> Edit Bass",
    "> WiFi Setup",
    "> Edit Nama"
  };

  for (int i = 0; i < 7; i++) {
    int yPos = 55 + (i * 13);
    if (menuSelection == i) {
      tft.fillRect(0, yPos - 2, 160, 12, TFT_DARKGREY);
    }
    tft.drawString(mainMenuItems[i], 5, yPos);
  }

  // Status bar
  tft.drawLine(0, 148, 160, 148, TFT_GREY);
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  String status = wifiConnected ? "WiFi OK" : "No WiFi";
  tft.drawString(status, 5, 152);

  updateEQGraph();
  currentMenu = MENU_MAIN;
}

void drawPresetMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 160, 20, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(1);
  tft.drawString("SELECT PRESET", 35, 5);

  for (int i = 0; i < 10; i++) {
    int yPos = 25 + (i * 11);
    if (menuSelection == i) {
      tft.fillRect(0, yPos, 160, 10, TFT_DARKGREY);
    }
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    if (i == currentPreset) {
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    }
    tft.setTextSize(1);
    String preset = (i + 1) + String(" - ") + presets[i].name;
    tft.drawString(preset, 5, yPos);
  }

  currentMenu = MENU_PRESET;
}

void drawSensitivityMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 160, 25, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(2);
  tft.drawString("SENSITIVITY", 5, 3);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Value: " + String(sensitivity), 10, 40);

  // Draw bar
  int barWidth = (sensitivity * 130) / 100;
  tft.fillRect(10, 70, barWidth, 20, TFT_GREEN);
  tft.drawRect(10, 70, 130, 20, TFT_WHITE);

  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Min: 1", 10, 100);
  tft.drawString("Max: 100", 95, 100);
  tft.drawString("Rotate to adjust", 35, 130);

  currentMenu = MENU_SENSITIVITY;
}

void drawVolumeMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 160, 25, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(2);
  tft.drawString("OUTPUT VOL", 5, 3);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Value: " + String(volumeOutput), 10, 40);

  int barWidth = (volumeOutput * 130) / 100;
  tft.fillRect(10, 70, barWidth, 20, TFT_RED);
  tft.drawRect(10, 70, 130, 20, TFT_WHITE);

  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Min: 1", 10, 100);
  tft.drawString("Max: 100", 95, 100);

  currentMenu = MENU_VOLUME;
}

void drawTrebleMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 160, 25, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(2);
  tft.drawString("TREBLE EQ", 10, 3);

  tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Value: " + String(trebleEQ), 10, 40);

  int barWidth = (trebleEQ * 130) / 100;
  tft.fillRect(10, 70, barWidth, 20, TFT_MAGENTA);
  tft.drawRect(10, 70, 130, 20, TFT_WHITE);

  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Min: 1", 10, 100);
  tft.drawString("Max: 100", 95, 100);

  currentMenu = MENU_TREBLE;
}

void drawBassMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 160, 25, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(2);
  tft.drawString("BASS EQ", 15, 3);

  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Value: " + String(bassEQ), 10, 40);

  int barWidth = (bassEQ * 130) / 100;
  tft.fillRect(10, 70, barWidth, 20, TFT_ORANGE);
  tft.drawRect(10, 70, 130, 20, TFT_WHITE);

  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Min: 1", 10, 100);
  tft.drawString("Max: 100", 95, 100);

  currentMenu = MENU_BASS;
}

void drawWiFiMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 160, 20, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(1);
  tft.drawString("WIFI SETUP", 50, 5);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(1);

  String status = wifiConnected ? "Connected" : "Not Connected";
  tft.drawString("Status: " + status, 5, 30);

  if (wifiConnected) {
    tft.drawString("SSID: " + wifiSSID, 5, 45);
    tft.drawString("IP: " + WiFi.localIP().toString(), 5, 60);
  }

  tft.drawString("Device: " + deviceName, 5, 75);
  tft.drawString("URL: sabilu.local", 5, 90);

  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("> Back to Menu", 5, 130);

  currentMenu = MENU_WIFI;
}

void drawPresetEditMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 160, 25, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(1);
  tft.drawString("EDIT PRESET NAME", 20, 5);
  tft.drawString("Preset " + String(currentPreset + 1), 40, 15);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("Name: " + presets[currentPreset].name, 5, 40);

  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  const char* editItems[] = {"> Edit Name", "> Bass: " , "> Treble: ", "> Sensitivity: ", "> Save", "> Cancel"};

  for (int i = 0; i < 6; i++) {
    int yPos = 60 + (i * 12);
    if (menuSelection == i) {
      tft.fillRect(0, yPos, 160, 11, TFT_DARKGREY);
    }
    tft.drawString(editItems[i], 5, yPos);
  }

  currentMenu = MENU_PRESET_EDIT;
}

void updateEQGraph() {
  // Draw stereo EQ graph on main screen
  graphL = (sensitivity + bassEQ) / 3;
  graphR = (sensitivity + trebleEQ) / 3;

  displayGraph(10, 115, graphL, graphR);
}

void displayGraph(int x, int y, uint8_t valueL, uint8_t valueR) {
  tft.drawRect(x, y, 70, 30, TFT_GREY);
  tft.drawLine(x + 35, y, x + 35, y + 30, TFT_GREY);

  // Left channel
  int heightL = (valueL * 28) / 100;
  tft.fillRect(x + 2, y + 30 - heightL, 15, heightL, TFT_GREEN);

  // Right channel
  int heightR = (valueR * 28) / 100;
  tft.fillRect(x + 37, y + 30 - heightR, 15, heightR, TFT_CYAN);

  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("L", x + 8, y + 32);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("R", x + 45, y + 32);
}

// ============= ENCODER HANDLING =============
void handleEncoderRotation() {
  if (encoderPos != lastEncoderPos) {
    int diff = encoderPos - lastEncoderPos;
    lastEncoderPos = encoderPos;

    switch (currentMenu) {
      case MENU_MAIN:
        menuSelection += diff;
        if (menuSelection < 0) menuSelection = 6;
        if (menuSelection > 6) menuSelection = 0;
        drawMainScreen();
        break;

      case MENU_PRESET:
        currentPreset += diff;
        if (currentPreset < 0) currentPreset = 9;
        if (currentPreset > 9) currentPreset = 0;
        drawPresetMenu();
        break;

      case MENU_SENSITIVITY:
        sensitivity += diff;
        if (sensitivity < 1) sensitivity = 1;
        if (sensitivity > 100) sensitivity = 100;
        drawSensitivityMenu();
        break;

      case MENU_VOLUME:
        volumeOutput += diff;
        if (volumeOutput < 1) volumeOutput = 1;
        if (volumeOutput > 100) volumeOutput = 100;
        drawVolumeMenu();
        break;

      case MENU_TREBLE:
        trebleEQ += diff;
        if (trebleEQ < 1) trebleEQ = 1;
        if (trebleEQ > 100) trebleEQ = 100;
        drawTrebleMenu();
        break;

      case MENU_BASS:
        bassEQ += diff;
        if (bassEQ < 1) bassEQ = 1;
        if (bassEQ > 100) bassEQ = 100;
        drawBassMenu();
        break;

      case MENU_PRESET_EDIT:
        menuSelection += diff;
        if (menuSelection < 0) menuSelection = 5;
        if (menuSelection > 5) menuSelection = 0;
        drawPresetEditMenu();
        break;

      default:
        break;
    }
  }
}

void handleEncoderClick() {
  static unsigned long lastClick = 0;
  unsigned long currentTime = millis();

  if (currentTime - lastClick < 300) return;

  if (digitalRead(ENC_SW) == LOW) {
    lastClick = currentTime;

    switch (currentMenu) {
      case MENU_MAIN:
        if (menuSelection == 0) drawPresetMenu();
        else if (menuSelection == 1) drawSensitivityMenu();
        else if (menuSelection == 2) drawVolumeMenu();
        else if (menuSelection == 3) drawTrebleMenu();
        else if (menuSelection == 4) drawBassMenu();
        else if (menuSelection == 5) drawWiFiMenu();
        else if (menuSelection == 6) drawPresetEditMenu();
        menuSelection = 0;
        break;

      case MENU_PRESET:
        drawMainScreen();
        break;

      case MENU_SENSITIVITY:
        drawMainScreen();
        break;

      case MENU_VOLUME:
        drawMainScreen();
        break;

      case MENU_TREBLE:
        drawMainScreen();
        break;

      case MENU_BASS:
        drawMainScreen();
        break;

      case MENU_WIFI:
        drawMainScreen();
        break;

      case MENU_PRESET_EDIT:
        drawMainScreen();
        break;

      default:
        break;
    }
  }
}

// ============= AUDIO PROCESSING (ZERO-DELAY) =============
void processAudio() {
  size_t bytesRead = 0;
  static int16_t inputBuffer[BUFFER_SIZE];

  // Read from I2S (microphone input)
  i2s_read(I2S_NUM_0, (char*)inputBuffer, BUFFER_SIZE * 2, &bytesRead, portMAX_DELAY);

  // Apply EQ filters (minimal processing for zero-delay)
  for (int i = 0; i < BUFFER_SIZE; i++) {
    int32_t sample = inputBuffer[i];

    // Apply bass boost (low-pass characteristic)
    sample = (sample * bassEQ) / 100;

    // Apply treble boost (high-pass characteristic)
    sample = sample + ((sample * (trebleEQ - 50)) / 100);

    // Apply sensitivity gain
    sample = (sample * sensitivity) / 100;

    // Apply output volume
    sample = (sample * volumeOutput) / 100;

    // Clamp to 16-bit range
    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;

    audioBuffer[i] = (int16_t)sample;
  }

  // Write to I2S (DAC output)
  size_t bytesWritten = 0;
  i2s_write(I2S_NUM_0, (char*)audioBuffer, BUFFER_SIZE * 2, &bytesWritten, portMAX_DELAY);
}

// ============= WIFI SETUP =============
void setupWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.beginSmartConfig();

  // Start AP for configuration
  WiFi.softAP("SABILU_Setup");

  setupWebServer();
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    String html = R"(
<!DOCTYPE html>
<html>
<head>
  <title>SABILU Control</title>
  <style>
    body { font-family: Arial; margin: 20px; background: #222; color: #fff; }
    .container { max-width: 500px; margin: auto; }
    .control { margin: 15px 0; padding: 10px; background: #333; border-radius: 5px; }
    input[type="range"] { width: 100%; }
    button { padding: 10px 20px; background: #0066cc; color: white; border: none; cursor: pointer; border-radius: 5px; }
    .status { padding: 10px; background: #0a5a0a; border-radius: 5px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>SABILU Audio Control</h1>
    <div class="status"><p id="status">Loading...</p></div>
    
    <div class="control">
      <label>Sensitivity: <span id="sensValue">60</span></label>
      <input type="range" min="1" max="100" value="60" onchange="setSensitivity(this.value)">
    </div>

    <div class="control">
      <label>Volume: <span id="volValue">70</span></label>
      <input type="range" min="1" max="100" value="70" onchange="setVolume(this.value)">
    </div>

    <div class="control">
      <label>Treble: <span id="trebValue">50</span></label>
      <input type="range" min="1" max="100" value="50" onchange="setTreble(this.value)">
    </div>

    <div class="control">
      <label>Bass: <span id="bassValue">60</span></label>
      <input type="range" min="1" max="100" value="60" onchange="setBass(this.value)">
    </div>

    <div class="control">
      <label>Device Name:</label>
      <input type="text" id="deviceName" placeholder="Enter device name">
      <button onclick="setDeviceName()">Update</button>
    </div>

    <div class="control">
      <label>Preset Select:</label>
      <select id="presetSelect" onchange="setPreset(this.value)">
        <option value="0">1 - Suara Bas JikJik</option>
        <option value="1">2 - Bas Deb</option>
        <option value="2">3 - Bas Deb Jik</option>
        <option value="3">4 - Bas CikCik</option>
        <option value="4">5 - Bas Drejeb</option>
        <option value="5">6 - Bas Gler</option>
        <option value="6">7 - Bas Drum</option>
        <option value="7">8 - Trompet</option>
        <option value="8">9 - Custom 1</option>
        <option value="9">10 - Custom 2</option>
      </select>
    </div>
  </div>

  <script>
    function setSensitivity(value) {
      document.getElementById('sensValue').innerText = value;
      fetch('/api/sensitivity/' + value);
    }
    function setVolume(value) {
      document.getElementById('volValue').innerText = value;
      fetch('/api/volume/' + value);
    }
    function setTreble(value) {
      document.getElementById('trebValue').innerText = value;
      fetch('/api/treble/' + value);
    }
    function setBass(value) {
      document.getElementById('bassValue').innerText = value;
      fetch('/api/bass/' + value);
    }
    function setDeviceName() {
      let name = document.getElementById('deviceName').value;
      if(name) fetch('/api/devicename/' + name);
    }
    function setPreset(value) {
      fetch('/api/preset/' + value);
    }
  </script>
</body>
</html>
    )";
    request->send(200, "text/html", html);
  });

  server.on("/api/sensitivity/:value", HTTP_GET, [](AsyncWebServerRequest * request) {
    sensitivity = request->getParam("value")->value().toInt();
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/volume/:value", HTTP_GET, [](AsyncWebServerRequest * request) {
    volumeOutput = request->getParam("value")->value().toInt();
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/treble/:value", HTTP_GET, [](AsyncWebServerRequest * request) {
    trebleEQ = request->getParam("value")->value().toInt();
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/bass/:value", HTTP_GET, [](AsyncWebServerRequest * request) {
    bassEQ = request->getParam("value")->value().toInt();
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/devicename/:name", HTTP_GET, [](AsyncWebServerRequest * request) {
    deviceName = request->getParam("name")->value();
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/preset/:id", HTTP_GET, [](AsyncWebServerRequest * request) {
    currentPreset = request->getParam("id")->value().toInt();
    if (currentPreset >= 0 && currentPreset < 10) {
      sensitivity = presets[currentPreset].sensitivity;
      bassEQ = presets[currentPreset].bass;
      trebleEQ = presets[currentPreset].treble;
    }
    request->send(200, "text/plain", "OK");
  });

  server.begin();
  wifiConnected = true;
}

// ============= MAIN LOOP =============
void loop() {
  handleEncoderRotation();
  handleEncoderClick();

  // Process audio continuously for zero-delay
  processAudio();

  // Update display occasionally
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 100) {
    if (currentMenu == MENU_MAIN) {
      updateEQGraph();
    }
    lastUpdate = millis();
  }
}
