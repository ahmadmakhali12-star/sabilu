#ifndef ENCODER_INPUT_H
#define ENCODER_INPUT_H

#include <Arduino.h>

class EncoderInput {
private:
  uint8_t clkPin;
  uint8_t dtPin;
  uint8_t swPin;
  
  volatile int encoderPos;
  volatile int lastEncoderPos;
  volatile unsigned long lastEncoderTime;
  volatile bool buttonPressed;
  
  static EncoderInput* instance;

public:
  EncoderInput(uint8_t clk, uint8_t dt, uint8_t sw);
  void begin();
  
  int getRotationDelta();
  bool isButtonPressed();
  void clearButtonPress();
  
  static void isrClk();
};

#endif