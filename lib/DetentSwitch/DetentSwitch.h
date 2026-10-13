// DetentSwitch: turns a pot reading into a detent position, 1 to N.
// Runtime code: this is what a real project uses. Tuning lives in DetentTuner.

#pragma once

#include <Arduino.h>
#include <PotReader.h>

// Calibration for one knob design. See DetentProfiles.h.
struct DetentProfile {
  uint8_t positions;        // number of detents
  const uint16_t *cutoffs;  // positions - 1 raw midpoints, ascending
  uint16_t hysteresis;      // raw counts past a cutoff before the position changes
};

class DetentSwitch {
 public:
  DetentSwitch(uint8_t pin, const DetentProfile &profile);

  void begin();   // call once from setup()
  bool update();  // read the pot; true if the position changed (and on the first call)

  uint8_t position() const { return position_; }  // 1 to positions(), 0 before the first update()
  uint16_t raw() const { return raw_; }           // last averaged reading
  uint8_t positions() const { return profile_.positions; }

  // Position for a raw reading, without hysteresis.
  uint8_t positionFromRaw(uint16_t raw) const;

 private:
  PotReader reader_;
  DetentProfile profile_;
  uint8_t position_ = 0;
  uint16_t raw_ = 0;
};
