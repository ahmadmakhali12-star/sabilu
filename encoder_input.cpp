#include "encoder_input.h"

EncoderInput* EncoderInput::instance = nullptr;

EncoderInput::EncoderInput(uint8_t clk, uint8_t dt, uint8_t sw)
  : clkPin(clk), dtPin(dt), swPin(sw), encoderPos(0), 
    lastEncoderPos(0), lastEncoderTime(0), buttonPressed(false) {
  instance = this;
}

void EncoderInput::begin() {
  pinMode(clkPin, INPUT_PULLUP);
  pinMode(dtPin, INPUT_PULLUP);
  pinMode(swPin, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(clkPin), EncoderInput::isrClk, CHANGE);
}

void EncoderInput::isrClk() {
  if (!instance) return;

  unsigned long currentTime = micros();
  if (currentTime - instance->lastEncoderTime < 500) return;

  int clkState = digitalRead(instance->clkPin);
  int dtState = digitalRead(instance->dtPin);

  if (clkState != dtState) {
    instance->encoderPos++;
  } else {
    instance->encoderPos--;
  }

  instance->lastEncoderTime = currentTime;
}

int EncoderInput::getRotationDelta() {
  if (encoderPos != lastEncoderPos) {
    int delta = encoderPos - lastEncoderPos;
    lastEncoderPos = encoderPos;
    return delta;
  }
  return 0;
}

bool EncoderInput::isButtonPressed() {
  if (digitalRead(swPin) == LOW) {
    if (!buttonPressed) {
      buttonPressed = true;
      return true;
    }
  } else {
    buttonPressed = false;
  }
  return false;
}

void EncoderInput::clearButtonPress() {
  buttonPressed = false;
}