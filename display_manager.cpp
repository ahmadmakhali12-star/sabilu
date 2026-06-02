#include "display_manager.h"

DisplayManager::DisplayManager(TFT_eSPI* display) {
  tft = display;
}

void DisplayManager::clearScreen() {
  tft->fillScreen(TFT_BLACK);
}

void DisplayManager::drawHeader(const char* title, uint16_t bgColor) {
  tft->fillRect(0, 0, 160, 25, bgColor);
  tft->setTextColor(TFT_WHITE, bgColor);
  tft->setTextSize(2);
  tft->drawCentreString(title, 80, 5);
}

void DisplayManager::drawFooter(const char* info) {
  tft->drawLine(0, 120, 160, 120, TFT_GREY);
  tft->setTextSize(1);
  tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  tft->drawString(info, 5, 122);
}

void DisplayManager::drawMainScreen(
  const char* deviceName,
  const char* presetName,
  uint8_t sensitivity,
  uint8_t volume,
  uint8_t bass,
  uint8_t treble,
  uint8_t signalL,
  uint8_t signalR,
  bool wifiConnected) {

  clearScreen();
  
  // Header
  drawHeader("SABILU DZIKRI", TFT_BLUE);

  // Preset info
  tft->setTextColor(TFT_CYAN, TFT_BLACK);
  tft->setTextSize(1);
  tft->drawString("Preset: ", 5, 28);
  tft->setTextColor(TFT_GREEN, TFT_BLACK);
  tft->drawString(presetName, 50, 28);

  // Device name
  tft->setTextColor(TFT_CYAN, TFT_BLACK);
  tft->drawString("Device: ", 5, 38);
  tft->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft->drawString(deviceName, 50, 38);

  // Parameters display
  tft->setTextSize(1);
  tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  
  String sensStr = "Sens:" + String(sensitivity);
  String volStr = "Vol:" + String(volume);
  String bassStr = "Bass:" + String(bass);
  String trebStr = "Tre:" + String(treble);
  
  tft->drawString(sensStr, 5, 50);
  tft->drawString(volStr, 50, 50);
  tft->drawString(bassStr, 5, 60);
  tft->drawString(trebStr, 50, 60);

  // EQ Graph
  drawEQGraph(10, 75, signalL, signalR, TFT_GREEN, TFT_CYAN);

  // Status
  String wifiStatus = wifiConnected ? "WiFi OK" : "No WiFi";
  drawFooter(wifiStatus.c_str());
}

void DisplayManager::drawPresetSelector(const char** presetNames, uint8_t currentPreset) {
  clearScreen();
  drawHeader("SELECT PRESET", TFT_BLUE);

  for (int i = 0; i < 10; i++) {
    int yPos = 30 + (i * 9);
    
    if (i == currentPreset) {
      tft->fillRect(0, yPos, 160, 9, TFT_DARKGREY);
      tft->setTextColor(TFT_YELLOW, TFT_DARKGREY);
    } else {
      tft->setTextColor(TFT_GREEN, TFT_BLACK);
    }
    
    tft->setTextSize(1);
    String preset = String(i + 1) + "-" + presetNames[i];
    tft->drawString(preset, 5, yPos);
  }
}

void DisplayManager::drawAdjustmentScreen(const char* label, uint8_t value, uint8_t minVal, uint8_t maxVal) {
  clearScreen();
  
  String title = String(label);
  drawHeader(title.c_str(), TFT_BLUE);

  // Value display
  tft->setTextColor(TFT_CYAN, TFT_BLACK);
  tft->setTextSize(3);
  tft->drawCentreString(String(value), 80, 40);

  // Progress bar
  int barWidth = (value * 130) / 100;
  tft->fillRect(15, 75, barWidth, 15, TFT_GREEN);
  tft->drawRect(15, 75, 130, 15, TFT_WHITE);

  // Min/Max
  tft->setTextSize(1);
  tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  tft->drawString("Min:" + String(minVal), 15, 95);
  tft->drawString("Max:" + String(maxVal), 105, 95);

  drawFooter("Rotate to adjust");
}

void DisplayManager::drawEQGraph(int x, int y, uint8_t levelL, uint8_t levelR, uint16_t colorL, uint16_t colorR) {
  // Draw box
  tft->drawRect(x, y, 70, 30, TFT_GREY);
  tft->drawLine(x + 35, y, x + 35, y + 30, TFT_GREY);

  // Left channel
  int heightL = (levelL * 28) / 100;
  tft->fillRect(x + 2, y + 30 - heightL, 15, heightL, colorL);

  // Right channel
  int heightR = (levelR * 28) / 100;
  tft->fillRect(x + 37, y + 30 - heightR, 15, heightR, colorR);

  // Labels
  tft->setTextSize(1);
  tft->setTextColor(colorL, TFT_BLACK);
  tft->drawString("L", x + 8, y + 32);
  tft->setTextColor(colorR, TFT_BLACK);
  tft->drawString("R", x + 45, y + 32);
}

void DisplayManager::drawWiFiStatus(const char* ssid, const char* ip, bool connected) {
  clearScreen();
  drawHeader("WIFI SETUP", TFT_BLUE);

  tft->setTextSize(1);
  tft->setTextColor(TFT_YELLOW, TFT_BLACK);

  if (connected) {
    tft->setTextColor(TFT_GREEN, TFT_BLACK);
    tft->drawString("Connected", 10, 35);
    tft->setTextColor(TFT_CYAN, TFT_BLACK);
    tft->drawString("SSID: " + String(ssid), 10, 50);
    tft->drawString("IP: " + String(ip), 10, 65);
  } else {
    tft->setTextColor(TFT_RED, TFT_BLACK);
    tft->drawString("Not Connected", 10, 35);
    tft->setTextColor(TFT_YELLOW, TFT_BLACK);
    tft->drawString("AP: SABILU_Setup", 10, 50);
  }

  drawFooter("Press back");
}

void DisplayManager::drawPresetNameEditor(const char* currentName, uint8_t presetNumber) {
  clearScreen();
  
  String title = "Edit Preset " + String(presetNumber + 1);
  drawHeader(title.c_str(), TFT_BLUE);

  tft->setTextColor(TFT_CYAN, TFT_BLACK);
  tft->setTextSize(1);
  tft->drawString("Current Name:", 10, 35);
  
  tft->setTextColor(TFT_GREEN, TFT_BLACK);
  tft->setTextSize(2);
  tft->drawString(currentName, 10, 50);

  tft->setTextSize(1);
  tft->setTextColor(TFT_YELLOW, TFT_BLACK);
  tft->drawString("Edit options:", 10, 80);
  tft->drawString("> Save", 10, 95);
  tft->drawString("> Cancel", 10, 110);
}