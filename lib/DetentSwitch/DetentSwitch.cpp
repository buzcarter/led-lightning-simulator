#include "DetentSwitch.h"

DetentSwitch::DetentSwitch(uint8_t pin, const DetentProfile &profile)
    : reader_(pin), profile_(profile) {}

void DetentSwitch::begin() {
  reader_.begin();
}

bool DetentSwitch::update() {
  raw_ = reader_.read();

  if (position_ == 0) {
    position_ = positionFromRaw(raw_);
    return true;
  }

  // Only move once the reading is clearly past a cutoff, so it doesn't flicker on a boundary.
  // cutoffs[i] sits between position i + 1 and i + 2.
  const uint8_t previous = position_;
  const int32_t raw = raw_;
  const int32_t h = profile_.hysteresis;
  while (position_ < profile_.positions && raw > (int32_t)profile_.cutoffs[position_ - 1] + h) {
    position_++;
  }
  while (position_ > 1 && raw < (int32_t)profile_.cutoffs[position_ - 2] - h) {
    position_--;
  }
  return position_ != previous;
}

uint8_t DetentSwitch::positionFromRaw(uint16_t raw) const {
  uint8_t pos = 1;
  for (uint8_t i = 0; i + 1 < profile_.positions; i++) {
    if (raw > profile_.cutoffs[i]) {
      pos++;
    }
  }
  return pos;
}
