// DetentProfiles: calibrated cutoffs for each detent knob design.
// Cutoffs are raw ADC counts, so a profile only applies to the board type it was measured on.
// To add one, run the tuner (README: "Calibration procedure") and paste the CUTOFFS line from `p`.

#pragma once

#include <DetentSwitch.h>

#if defined(ESP32)

// 10-position knob. Measured 2026-10-10 on an ESP32 DevKit, GPIO34, 11 dB attenuation.
static const uint16_t DETENT_10_CUTOFFS[] = {113, 467, 945, 1416, 1888, 2357, 2836, 3426, 3931};
static const DetentProfile DETENT_10 = {10, DETENT_10_CUTOFFS, 40};

#else

// 10-position knob. NOMINAL, not measured: evenly spaced 30° steps on a 10-bit, 5 V ADC.
// Tune and replace before relying on it.
static const uint16_t DETENT_10_CUTOFFS[] = {102, 205, 307, 409, 512, 614, 716, 818, 921};
static const DetentProfile DETENT_10 = {10, DETENT_10_CUTOFFS, 10};

#endif
