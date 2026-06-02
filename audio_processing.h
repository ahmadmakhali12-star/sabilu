#ifndef AUDIO_PROCESSING_H
#define AUDIO_PROCESSING_H

#include <stdint.h>
#include "driver/i2s.h"

// ============= AUDIO PROCESSING ENGINE =============
class AudioProcessor {
private:
  static const int BUFFER_SIZE = 2048;
  int16_t processingBuffer[BUFFER_SIZE];
  
  // EQ coefficient calculation
  float calculateBassCoefficient(uint8_t bassLevel);
  float calculateTrebleCoefficient(uint8_t trebleLevel);
  
  // First-order IIR filter state
  int32_t bassState;
  int32_t trebleState;

public:
  AudioProcessor();
  
  /**
   * Process audio buffer with real-time EQ
   * Minimal latency for zero-delay operation
   */
  void processBuffer(
    int16_t* inputBuffer,
    int16_t* outputBuffer,
    uint16_t bufferSize,
    uint8_t sensitivity,
    uint8_t volumeLevel,
    uint8_t bassLevel,
    uint8_t trebleLevel
  );
  
  /**
   * Fast limiter to prevent clipping
   */
  void applyLimiter(int16_t* buffer, uint16_t size, int16_t threshold);
  
  /**
   * Get current signal level for visualization
   */
  uint8_t getSignalLevel(int16_t* buffer, uint16_t size);
};

#endif
