// PotReader: averaged analog readings from one potentiometer pin.
// Knows the board's ADC settings, nothing about detents or serial output.

#pragma once

#include <Arduino.h>

class PotReader {
 public:
  explicit PotReader(uint8_t pin, uint8_t samples = 16);

  void begin();      // call once from setup()
  uint16_t read();   // averaged raw reading, 0 to maxRaw()

  uint8_t pin() const { return pin_; }

  static uint16_t maxRaw();  // 4095 on ESP32 (12-bit), 1023 on AVR (10-bit)
  static float vref();       // nominal full-scale voltage, for display only

 private:
  uint8_t pin_;
  uint8_t samples_;
};
