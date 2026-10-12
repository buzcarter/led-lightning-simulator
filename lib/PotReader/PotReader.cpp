#include "PotReader.h"

namespace {
#if defined(ESP32)
constexpr uint8_t ADC_BITS = 12;
constexpr float VREF = 3.3f;
#else
constexpr uint8_t ADC_BITS = 10;  // Uno R3 and Mega 2560
constexpr float VREF = 5.0f;
#endif
}  // namespace

PotReader::PotReader(uint8_t pin, uint8_t samples)
    : pin_(pin), samples_(samples > 0 ? samples : 1) {}

void PotReader::begin() {
#if defined(ESP32)
  analogReadResolution(ADC_BITS);
  analogSetPinAttenuation(pin_, ADC_11db);  // full range up to about 3.1 V
#endif
}

uint16_t PotReader::read() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < samples_; i++) {
    sum += analogRead(pin_);
  }
  return (uint16_t)((sum + samples_ / 2) / samples_);  // rounded average
}

uint16_t PotReader::maxRaw() {
  return (1U << ADC_BITS) - 1;
}

float PotReader::vref() {
  return VREF;
}
