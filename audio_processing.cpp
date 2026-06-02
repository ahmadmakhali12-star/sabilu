#include "audio_processing.h"
#include <math.h>

AudioProcessor::AudioProcessor() : bassState(0), trebleState(0) {
}

float AudioProcessor::calculateBassCoefficient(uint8_t bassLevel) {
  // Map 0-100 to 0.0-1.5x boost
  return 1.0f + ((bassLevel - 50) * 0.01f);
}

float AudioProcessor::calculateTrebleCoefficient(uint8_t trebleLevel) {
  // Map 0-100 to 0.0-1.5x boost
  return 1.0f + ((trebleLevel - 50) * 0.01f);
}

void AudioProcessor::processBuffer(
  int16_t* inputBuffer,
  int16_t* outputBuffer,
  uint16_t bufferSize,
  uint8_t sensitivity,
  uint8_t volumeLevel,
  uint8_t bassLevel,
  uint8_t trebleLevel) {

  float bassMult = calculateBassCoefficient(bassLevel);
  float trebleMult = calculateTrebleCoefficient(trebleLevel);
  float sensitivityMult = (sensitivity / 100.0f);
  float volumeMult = (volumeLevel / 100.0f);

  // IIR filter coefficients for minimal phase shift
  const float BASS_ALPHA = 0.1f;  // Low-pass
  const float TREBLE_ALPHA = 0.05f; // High-pass

  for (uint16_t i = 0; i < bufferSize; i++) {
    int32_t sample = inputBuffer[i];

    // Apply sensitivity (gain from microphone)
    sample = (int32_t)(sample * sensitivityMult);

    // Low-pass for bass (IIR first-order)
    bassState = bassState + (int32_t)(BASS_ALPHA * (sample - bassState));
    int32_t bassComponent = (bassState * bassMult);

    // High-pass for treble (difference from low-pass)
    int32_t highPass = sample - bassState;
    trebleState = trebleState + (int32_t)(TREBLE_ALPHA * (highPass - trebleState));
    int32_t trebleComponent = (trebleState * trebleMult);

    // Combine bass and treble
    sample = bassComponent + trebleComponent;

    // Apply volume
    sample = (int32_t)(sample * volumeMult);

    // Clamp to 16-bit range
    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;

    outputBuffer[i] = (int16_t)sample;
  }
}

void AudioProcessor::applyLimiter(int16_t* buffer, uint16_t size, int16_t threshold) {
  for (uint16_t i = 0; i < size; i++) {
    if (buffer[i] > threshold) {
      buffer[i] = threshold;
    } else if (buffer[i] < -threshold) {
      buffer[i] = -threshold;
    }
  }
}

uint8_t AudioProcessor::getSignalLevel(int16_t* buffer, uint16_t size) {
  uint32_t sum = 0;
  
  for (uint16_t i = 0; i < size; i++) {
    int32_t sample = buffer[i];
    sum += (sample * sample) >> 16;
  }

  // Calculate RMS
  uint32_t rms = sum / size;
  uint8_t level = (uint8_t)((rms > 32767) ? 100 : (rms * 100) / 32767);
  
  return (level > 100) ? 100 : level;
}
