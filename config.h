#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H

// ============= I2S AUDIO CONFIGURATION =============
#define I2S_SAMPLE_RATE 44100
#define I2S_BUFFER_SIZE 2048
#define I2S_NUM I2S_NUM_0

// ============= TFT DISPLAY CONFIGURATION =============
#define TFT_DRIVER ST7735
#define TFT_WIDTH  160
#define TFT_HEIGHT 128
#define TFT_ROTATION 1

// SPI Speed
#define SPI_FREQUENCY  40000000

// ============= COLOR DEFINITIONS =============
#define TFT_BLACK      0x0000
#define TFT_BLUE       0x001F
#define TFT_RED        0xF800
#define TFT_GREEN      0x07E0
#define TFT_CYAN       0x07FF
#define TFT_MAGENTA    0xF81F
#define TFT_YELLOW     0xFFE0
#define TFT_WHITE      0xFFFF
#define TFT_ORANGE     0xFC00
#define TFT_GREY       0x8410
#define TFT_DARKGREY   0x39E7

// ============= AUDIO CALIBRATION =============
#define MIC_SENSITIVITY_DEFAULT 60
#define DAC_OUTPUT_DEFAULT 70
#define EQ_BASS_DEFAULT 60
#define EQ_TREBLE_DEFAULT 50

// ============= ZERO-DELAY OPTIMIZATION =============
// Minimize DMA buffer latency
#define DMA_BUFFER_COUNT 4
#define DMA_BUFFER_LENGTH 256

// ============= ENCODER DEBOUNCE =============
#define ENCODER_DEBOUNCE_US 500

// ============= WIFI CONFIGURATION =============
#define WIFI_MODE WIFI_AP_STA
#define WIFI_AP_SSID "SABILU_Setup"
#define WIFI_AP_CHANNEL 1

// ============= SPIFFS CONFIGURATION =============
#define PRESET_STORAGE_FILE "/presets.json"
#define CONFIG_STORAGE_FILE "/config.json"

#endif
