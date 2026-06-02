#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>

class DisplayManager {
private:
  TFT_eSPI* tft;

public:
  DisplayManager(TFT_eSPI* display);
  
  void drawMainScreen(
    const char* deviceName,
    const char* presetName,
    uint8_t sensitivity,
    uint8_t volume,
    uint8_t bass,
    uint8_t treble,
    uint8_t signalL,
    uint8_t signalR,
    bool wifiConnected
  );
  
  void drawPresetSelector(const char** presetNames, uint8_t currentPreset);
  void drawAdjustmentScreen(const char* label, uint8_t value, uint8_t minVal, uint8_t maxVal);
  void drawEQGraph(int x, int y, uint8_t levelL, uint8_t levelR, uint16_t colorL, uint16_t colorR);
  void drawWiFiStatus(const char* ssid, const char* ip, bool connected);
  void drawPresetNameEditor(const char* currentName, uint8_t presetNumber);
  
  void clearScreen();
  void drawHeader(const char* title, uint16_t bgColor);
  void drawFooter(const char* info);
};

#endif