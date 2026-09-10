/* ─────────────────────────────────────────────────────────────
   controls.h — YOURS. Bolt Bench never overwrites this file.

   Three B10K linear pots on ADC1. POTS_ENABLE 0 compiles the lot out
   and every value falls back to tuning.h.

   Wiring, per pot: outer leg -> 3V3, other outer leg -> GND, wiper ->
   the ADC pin, plus 100 nF from wiper to GND at the board end. 3.3 V,
   NOT 5 V — these pins are not 5 V tolerant. Swap the outer legs to
   reverse a knob's direction.

   All three knobs are detented. A continuous knob on a noisy 12-bit ADC
   is the worst of both worlds: never quite still, never repeatable. With
   detents the hysteresis band is far wider than the noise, so a position
   holds exactly where you left it.
   ───────────────────────────────────────────────────────────── */
#pragma once
#include <Arduino.h>
#include "storm.h"

#ifndef POTS_ENABLE
  #define POTS_ENABLE 1
#endif

/* ADC1 only. ADC2 (GPIO 0/2/4/12-15/25-27) stops working the moment WiFi
   is enabled, and GPIO2 is the strip anyway. GPIO34 is input-only, which
   is a feature for an analog input — it cannot be driven by accident. */
#define POT_PALETTE_PIN  GPIO_NUM_32
#define POT_SPEED_PIN    GPIO_NUM_33
#define POT_LEVEL_PIN    GPIO_NUM_34

/* The ESP32 ADC flattens near both rails, so trim a dead zone off each end
   to make the first and last detent reachable. */
#define POT_MV_MIN       80
#define POT_MV_MAX       3140

/* Sampling. One pot per interval, round-robin, so each knob is read every
   3 x POT_INTERVAL_MS — about 750 ms. Knob positions do not need to be
   fresh at frame rate, and sampling rarely but thoroughly beats sampling
   constantly and then filtering: averaging N conversions cuts the noise by
   sqrt(N), where a filter buys the same quiet only by adding lag.

   32 conversions costs ~3 ms, landing in one frame out of every twenty-six.
   There is deliberately no smoothing filter now: with this much averaging
   plus detent hysteresis, an EMA would only add latency. */
#define POT_INTERVAL_MS  250
#define POT_OVERSAMPLE   32

/* Speed detents, 8.8 fixed point. Geometric, because equal twists should
   give equal ratio changes; index 5 is exactly 1.00x. */
#define SPEED_STEPS   11
static const uint16_t SPEED_Q8[SPEED_STEPS] = {
   64,   84,  111,  147,  194,  256,  338,  446,  588,  776, 1024
/* 0.25x                        1.00x                        4.00x */
};

/* Brightness at level detent 0. Not zero — a knob position that makes the
   whole costume look broken is rarely what you want at the end of the
   travel. Set to 0 to turn it into an off switch. */
#define POT_LEVEL_FLOOR  20

#define POT_LOG_MS       400

#if POTS_ENABLE

struct Pot { gpio_num_t pin; uint8_t out; };

static Pot potPalette = { POT_PALETTE_PIN, 0 };
static Pot potSpeed   = { POT_SPEED_PIN,   0 };
static Pot potLevel   = { POT_LEVEL_PIN,   0 };

static uint8_t heldPalette = 0, heldSpeed = 0, heldLevel = 0;

static inline void potRead(Pot &p) {
  uint32_t acc = 0;
  for (uint8_t i = 0; i < POT_OVERSAMPLE; i++) acc += analogReadMilliVolts(p.pin);
  int32_t mv = (int32_t)(acc / POT_OVERSAMPLE);
  if (mv <= POT_MV_MIN)      p.out = 0;
  else if (mv >= POT_MV_MAX) p.out = 255;
  else p.out = (uint8_t)(((mv - POT_MV_MIN) * 255) / (POT_MV_MAX - POT_MV_MIN));
}

/* Snap 0..255 to one of `steps` detents, holding the current one until the
   knob has moved three quarters of a step. Quantising without that is worse
   than not quantising: park on a boundary and noise flips it forever.
   At 11 steps the band is ~18 counts against maybe 10 of ADC noise. */
static inline uint8_t potDetent(uint8_t v, uint8_t steps, uint8_t &held) {
  if (steps < 2) return 0;
  uint16_t width  = 255 / (steps - 1);
  int16_t  centre = (int16_t)((uint16_t)held * 255 / (steps - 1));
  int16_t  d = (int16_t)v - centre;
  if (d < 0) d = -d;
  if (d > (int16_t)(width * 3 / 4))
    held = (uint8_t)(((uint32_t)v * (steps - 1) + 127) / 255);
  return held;
}

void controlsUpdate(bool force) {
  static uint32_t nextAt = 0;
  static uint8_t  turn = 0;
  static uint32_t loggedAt = 0;

  if (force) {
    potRead(potPalette); potRead(potSpeed); potRead(potLevel);
  } else {
    if ((int32_t)(millis() - nextAt) < 0) return;
    nextAt = millis() + POT_INTERVAL_MS;
    switch (turn) {
      case 0:  potRead(potPalette); break;
      case 1:  potRead(potSpeed);   break;
      default: potRead(potLevel);   break;
    }
    turn = (turn + 1) % 3;
  }

  /* Palette, not raw hue. Rotating hue alone leaves SAT wherever tuning.h
     left it — and at a low saturation every hue is the same near-white, so
     the knob appears dead. Each detent carries a whole look instead. */
  uint8_t pal = potDetent(potPalette.out, NUM_PALETTES, heldPalette);
  bool trimChanged = false;
  if (force || pal != g_palette) {
    g_palette = pal;
    g_hue    = PALETTES[pal].hue;
    g_sat    = PALETTES[pal].sat;
    g_hotSat = PALETTES[pal].hotSat;
    g_trim   = PALETTES[pal].trim;
    updatePalette();
    trimChanged = true;
  }

  uint8_t sp = potDetent(potSpeed.out, SPEED_STEPS, heldSpeed);
  g_speedQ8 = SPEED_Q8[sp];

  g_level = potDetent(potLevel.out, LEVEL_STEPS, heldLevel);
  uint8_t ceil8 = scale8(MASTER, g_trim);
  uint8_t span  = ceil8 > POT_LEVEL_FLOOR ? ceil8 - POT_LEVEL_FLOOR : 0;
  uint8_t b = (uint8_t)(POT_LEVEL_FLOOR + ((uint32_t)g_level * span / (LEVEL_STEPS - 1)));
  if (force || trimChanged || b != g_bright) { g_bright = b; FastLED.setBrightness(b); }

#if STORM_LOG
  static uint8_t lp = 255, ll = 255, ls = 255;
  if (force || g_palette != lp || g_level != ll || sp != ls) {
    if (millis() - loggedAt >= POT_LOG_MS || force) {
      lp = g_palette; ll = g_level; ls = sp; loggedAt = millis();
      LOG("[pots]  %-11s hue=%-3u sat=%-3u | speed %u.%02ux | level %2u/%u (%u%%) bright=%u\n",
          PALETTES[g_palette].name, (unsigned)g_hue, (unsigned)g_sat,
          (unsigned)(g_speedQ8 >> 8), (unsigned)((g_speedQ8 & 0xFF) * 100 / 256),
          (unsigned)g_level, (unsigned)(LEVEL_STEPS - 1),
          (unsigned)LEVEL_PERCENT(g_level), (unsigned)g_bright);
    }
  }
#endif
}

void controlsBegin() {
  analogReadResolution(12);
  controlsUpdate(true);
  LOG("[pots]  palette GPIO%u (%u), speed GPIO%u (%u), level GPIO%u (%u detents)\n",
      (unsigned)POT_PALETTE_PIN, (unsigned)NUM_PALETTES,
      (unsigned)POT_SPEED_PIN,   (unsigned)SPEED_STEPS,
      (unsigned)POT_LEVEL_PIN,   (unsigned)LEVEL_STEPS);
  LOG("[pots]  sampled %u at a time, one knob every %u ms\n",
      (unsigned)POT_OVERSAMPLE, (unsigned)POT_INTERVAL_MS);
}

#else   /* POTS_ENABLE == 0 — knobs compiled out, tuning.h rules */

inline void controlsUpdate(bool) {}
inline void controlsBegin() { LOG("[pots]  disabled\n"); }

#endif
