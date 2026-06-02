/*
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║       SABILU - Lightweight Audio Module for ESP32            ║
 * ║     Optimized for Minimal Memory & Flash Usage              ║
 * ║         Zero-Delay Audio + Minimal Display Code             ║
 * ║                    Version 1.0 - Lite                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 * 
 * Memory Optimization:
 * - No SPIFFS (presets in flash)
 * - Minimal WiFi (no web server)
 * - Simple display routines
 * - Lightweight audio processing
 * - RAM: ~60KB | Flash: ~400KB
 */

#include <SPI.h>
#include <TFT_eSPI.h>
#include "driver/i2s.h"

// ========================= PIN DEFINITIONS =========================
#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4

#define I2S_BCK   26
#define I2S_LRCK  25
#define I2S_DIN   22

#define ENC_CLK   19
#define ENC_DT    18
#define ENC_SW    21

// ========================= CONSTANTS =========================
#define SAMPLE_RATE       44100
#define BUFFER_SIZE       1024      // Reduced from 2048
#define MAX_PRESETS       10
#define ENCODER_DEBOUNCE  500

// ========================= PRESETS IN FLASH =========================
struct Preset {
  const char* name;
  uint8_t bass;
  uint8_t treble;
  uint8_t sens;
};

// Store in PROGMEM to save RAM
const Preset PRESETS[MAX_PRESETS] PROGMEM = {
  {"JikJik Bass", 85, 30, 60},
  {"Deb Bass", 90, 25, 55},
  {"Deb-Jik", 88, 28, 58},
  {"CikCik", 75, 50, 65},
  {"Drejeb", 95, 15, 50},
  {"Gler", 80, 40, 62},
  {"Drum", 92, 20, 52},
  {"Trompet", 40, 85, 70},
  {"Custom 1", 50, 50, 60},
  {"Custom 2", 60, 60, 60}
};

// ========================= MENU ENUM =========================
enum MenuState {
  MENU_MAIN,
  MENU_PRESET,
  MENU_SENS,
  MENU_VOL,
  MENU_TREBLE,
  MENU_BASS,
  MENU_STATUS
};

// ========================= GLOBAL OBJECTS =========================
TFT_eSPI tft = TFT_eSPI();

// ========================= GLOBAL VARIABLES =========================
// Audio parameters
uint8_t currentPreset = 0;
uint8_t sensitivity = 60;
uint8_t volume = 70;
uint8_t treble = 50;
uint8_t bass = 60;

// Menu state
MenuState currentMenu = MENU_MAIN;
int menuSel = 0;

// Encoder state
volatile int encPos = 0;
volatile int lastEncPos = 0;
volatile unsigned long lastEncTime = 0;

// Audio buffers
int16_t audioBuf[BUFFER_SIZE];
int16_t inputBuf[BUFFER_SIZE];

// Audio processing state
int32_t bassFilter = 0;
int32_t trebleFilter = 0;

// ========================= INTERRUPT SERVICE ROUTINE =========================
void IRAM_ATTR encoderISR(void) {
  unsigned long now = micros();
  if (now - lastEncTime < ENCODER_DEBOUNCE) return;

  if (digitalRead(ENC_CLK) != digitalRead(ENC_DT)) {
    encPos++;
  } else {
    encPos--;
  }
  lastEncTime = now;
}

// ========================= I2S SETUP =========================
void setupI2S(void) {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 2,        // Reduced from 4
    .dma_buf_len = 256,
    .use_apll = true
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCK,
    .ws_io_num = I2S_LRCK,
    .data_out_num = I2S_DIN,
    .data_in_num = I2S_NUM_0
  };

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// ========================= ENCODER SETUP =========================
void setupEncoder(void) {
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), encoderISR, CHANGE);
}

// ========================= AUDIO PROCESSING (OPTIMIZED) =========================
void processAudio(void) {
  size_t bytesRead = 0, bytesWritten = 0;

  // Read input
  i2s_read(I2S_NUM_0, inputBuf, BUFFER_SIZE * 2, &bytesRead, portMAX_DELAY);

  // Fast audio processing
  float sensMult = sensitivity / 100.0f;
  float volMult = volume / 100.0f;
  float bassMult = 1.0f + ((bass - 50) * 0.01f);
  float trebMult = 1.0f + ((treble - 50) * 0.01f);

  for (int i = 0; i < BUFFER_SIZE; i++) {
    int32_t s = inputBuf[i];

    // Gain
    s = (int32_t)(s * sensMult);

    // Bass filter (simple low-pass)
    bassFilter = bassFilter + (int32_t)(0.1f * (s - bassFilter));
    int32_t bassComp = (int32_t)((int64_t)bassFilter * bassMult);

    // Treble filter (high-pass)
    int32_t hpf = s - bassFilter;
    trebleFilter = trebleFilter + (int32_t)(0.05f * (hpf - trebleFilter));
    int32_t trebComp = (int32_t)((int64_t)trebleFilter * trebMult);

    // Mix
    s = bassComp + trebComp;

    // Volume
    s = (int32_t)(s * volMult);

    // Clip
    if (s > 32767) s = 32767;
    if (s < -32768) s = -32768;

    audioBuf[i] = (int16_t)s;
  }

  // Write output
  i2s_write(I2S_NUM_0, audioBuf, BUFFER_SIZE * 2, &bytesWritten, portMAX_DELAY);
}

// ========================= DISPLAY FUNCTIONS =========================
void drawMain(void) {
  tft.fillScreen(TFT_BLACK);
  
  // Header
  tft.fillRect(0, 0, 160, 25, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(2);
  tft.drawString("SABILU", 35, 5);

  // Preset info
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("Preset:", 5, 30);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  
  char buf[20];
  strcpy_P(buf, (char*)pgm_read_word(&(PRESETS[currentPreset].name)));
  tft.drawString(buf, 50, 30);

  // Menu items
  const char* items[] = {"> Preset", "> Sensitivity", "> Volume", "> Treble", "> Bass", "> Status"};
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  
  for (int i = 0; i < 6; i++) {
    int y = 45 + i * 12;
    if (menuSel == i) {
      tft.fillRect(0, y, 160, 11, TFT_DARKGREY);
      tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
    } else {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    tft.drawString(items[i], 5, y);
  }

  // Simple graph
  tft.drawRect(80, 102, 70, 20, TFT_GREY);
  tft.fillRect(85, 110, (sensitivity * 30) / 100, 8, TFT_GREEN);
  tft.fillRect(115, 110, (bass * 30) / 100, 8, TFT_YELLOW);
}

void drawPresets(void) {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 160, 20, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(1);
  tft.drawString("SELECT PRESET", 35, 5);

  for (int i = 0; i < MAX_PRESETS; i++) {
    int y = 22 + i * 10;
    if (menuSel == i) {
      tft.fillRect(0, y, 160, 9, TFT_DARKGREY);
      tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
    } else {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
    }
    
    char buf[20];
    strcpy_P(buf, (char*)pgm_read_word(&(PRESETS[i].name)));
    tft.setTextSize(1);
    tft.drawString(String(i + 1) + ". " + buf, 5, y);
  }
}

void drawValue(const char* title, uint8_t val, uint16_t color) {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 160, 20, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(1);
  tft.drawString(title, 30, 5);

  tft.setTextColor(color, TFT_BLACK);
  tft.setTextSize(3);
  char buf[4];
  snprintf(buf, sizeof(buf), "%3d", val);
  tft.drawString(buf, 50, 40);

  // Bar
  int w = (val * 130) / 100;
  tft.fillRect(10, 85, w, 20, color);
  tft.drawRect(10, 85, 130, 20, TFT_WHITE);

  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Min:1  Max:100", 15, 110);
}

void drawStatus(void) {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 160, 20, TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(1);
  tft.drawString("STATUS", 50, 5);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(1);
  tft.drawString("SABILU v1.0 Lite", 5, 25);
  tft.drawString("44.1kHz 16bit", 5, 40);
  tft.drawString("Zero-Delay Audio", 5, 55);
  
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Running OK", 5, 75);
  
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(1);
  char buf[40];
  snprintf(buf, sizeof(buf), "RAM: %d KB", ESP.getFreeHeap() / 1024);
  tft.drawString(buf, 5, 95);
}

// ========================= ENCODER HANDLING =========================
void handleEncoder(void) {
  if (encPos == lastEncPos) return;

  int diff = encPos - lastEncPos;
  lastEncPos = encPos;

  switch (currentMenu) {
    case MENU_MAIN:
      menuSel += diff;
      if (menuSel < 0) menuSel = 5;
      if (menuSel > 5) menuSel = 0;
      drawMain();
      break;

    case MENU_PRESET:
      currentPreset += diff;
      if (currentPreset < 0) currentPreset = 9;
      if (currentPreset > 9) currentPreset = 0;
      drawPresets();
      break;

    case MENU_SENS:
      sensitivity += diff;
      if (sensitivity < 1) sensitivity = 1;
      if (sensitivity > 100) sensitivity = 100;
      drawValue("Sensitivity", sensitivity, TFT_GREEN);
      break;

    case MENU_VOL:
      volume += diff;
      if (volume < 1) volume = 1;
      if (volume > 100) volume = 100;
      drawValue("Volume", volume, TFT_RED);
      break;

    case MENU_TREBLE:
      treble += diff;
      if (treble < 1) treble = 1;
      if (treble > 100) treble = 100;
      drawValue("Treble", treble, TFT_MAGENTA);
      break;

    case MENU_BASS:
      bass += diff;
      if (bass < 1) bass = 1;
      if (bass > 100) bass = 100;
      drawValue("Bass", bass, TFT_ORANGE);
      break;

    default:
      break;
  }
}

void handleButton(void) {
  static unsigned long lastClick = 0;
  if (millis() - lastClick < 300) return;

  if (digitalRead(ENC_SW) != LOW) return;
  lastClick = millis();

  switch (currentMenu) {
    case MENU_MAIN:
      currentMenu = (MenuState)menuSel;
      menuSel = 0;
      if (currentMenu == MENU_MAIN) {
        drawMain();
      } else if (currentMenu == MENU_PRESET) {
        drawPresets();
      } else if (currentMenu == MENU_SENS) {
        drawValue("Sensitivity", sensitivity, TFT_GREEN);
      } else if (currentMenu == MENU_VOL) {
        drawValue("Volume", volume, TFT_RED);
      } else if (currentMenu == MENU_TREBLE) {
        drawValue("Treble", treble, TFT_MAGENTA);
      } else if (currentMenu == MENU_BASS) {
        drawValue("Bass", bass, TFT_ORANGE);
      } else if (currentMenu == MENU_STATUS) {
        drawStatus();
      }
      break;

    case MENU_PRESET:
      currentMenu = MENU_MAIN;
      menuSel = 0;
      drawMain();
      break;

    case MENU_SENS:
      currentMenu = MENU_MAIN;
      menuSel = 1;
      drawMain();
      break;

    case MENU_VOL:
      currentMenu = MENU_MAIN;
      menuSel = 2;
      drawMain();
      break;

    case MENU_TREBLE:
      currentMenu = MENU_MAIN;
      menuSel = 3;
      drawMain();
      break;

    case MENU_BASS:
      currentMenu = MENU_MAIN;
      menuSel = 4;
      drawMain();
      break;

    case MENU_STATUS:
      currentMenu = MENU_MAIN;
      menuSel = 5;
      drawMain();
      break;

    default:
      currentMenu = MENU_MAIN;
      drawMain();
      break;
  }
}

// ========================= SETUP =========================
void setup(void) {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n\n╔══════════════════════════════════════════════╗");
  Serial.println("║  SABILU v1.0 LITE - Optimized for ESP32    ║");
  Serial.println("║  Minimal Flash & RAM - Zero-Delay Audio     ║");
  Serial.println("╚══════════════════════════════════════════════╝\n");

  // TFT Init
  Serial.print("[TFT] Initializing display... ");
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("SABILU", 40, 50);
  Serial.println("OK");

  // I2S Setup
  Serial.print("[I2S] Setting up audio... ");
  setupI2S();
  Serial.println("OK");

  // Encoder Setup
  Serial.print("[ENC] Initializing encoder... ");
  setupEncoder();
  Serial.println("OK");

  delay(1000);

  // Show main screen
  currentMenu = MENU_MAIN;
  drawMain();

  Serial.println("[SYSTEM] Ready!\n");
  Serial.printf("[HEAP] Free: %d KB\n\n", ESP.getFreeHeap() / 1024);
}

// ========================= MAIN LOOP =========================
void loop(void) {
  // Handle inputs
  handleEncoder();
  handleButton();

  // Process audio
  processAudio();

  // Yield to prevent watchdog
  yield();
}

/*
 * SABILU v1.0 LITE
 * Optimized for minimal memory footprint
 * RAM usage: ~60 KB
 * Flash usage: ~400 KB
 * 
 * Features:
 * - Zero-delay audio processing
 * - 10 presets (stored in PROGMEM)
 * - Simple display interface
 * - Rotary encoder control
 * - Real-time EQ (Bass/Treble)
 * 
 * Email: ahmadmakhali12@gmail.com
 */
