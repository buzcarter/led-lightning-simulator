/* ─────────────────────────────────────────────────────────────
   controls.h — YOURS. Bolt Bench never overwrites this file.

   Three B10K linear pots on ADC1. Set POTS_ENABLE to 0 and every
   value falls back to tuning.h, with no ADC code compiled in.

   Wiring, per pot: outer leg -> 3V3, other outer leg -> GND, wiper ->
   the ADC pin, plus 100 nF from wiper to GND at the board end. 3.3 V,
   NOT 5 V — these pins are not 5 V tolerant. Swap the outer legs to
   reverse a knob's direction.
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
#define POT_HUE_PIN    GPIO_NUM_32
#define POT_SPEED_PIN  GPIO_NUM_33
#define POT_LEVEL_PIN  GPIO_NUM_34

/* The ESP32 ADC flattens near both rails, so trim a dead zone off each end
   to make 0 and 255 actually reachable. Widen if a knob never quite bottoms
   out; narrow if it hits the stop early. */
#define POT_MV_MIN     80
#define POT_MV_MAX     3140

/* EMA smoothing, as a right-shift: ema += (raw - ema) >> POT_SMOOTH.
   Note this is tuned for OUR loop rate, which is not fast. 300 WS2812Bs
   take ~9 ms to clock out, so loop() runs near 105 Hz and each pot is
   visited every third pass — about 35 Hz. At shift 2 that settles in
   ~110 ms, which feels immediate on a knob. Code written for a loop that
   free-runs at thousands of hertz uses a far gentler alpha; copying that
   value here would make the knobs feel like treacle. */
#define POT_SMOOTH     2
#define POT_OVERSAMPLE 4      // averaged per visit; halves the white noise

/* Speed knob range, as powers of two either side of centre. 2 -> 0.25x at
   one stop, 1.00x at midpoint, ~4x at the other. Geometric, because a
   linear speed knob spends most of its travel somewhere useless. */
#define POT_SPEED_OCTAVES  2.0f
#define POT_SPEED_SNAP     6      // counts either side of centre that snap to 1.00x

#define POT_LOG_MS     250        // rate limit; turning a knob must not flood the port

#if POTS_ENABLE

struct Pot {
  gpio_num_t pin;
  int32_t    ema;      // smoothed millivolts, 8.8 fixed point
  uint8_t    out;      // 0..255 after dead zone and hysteresis
};

static Pot potHue   = { POT_HUE_PIN,   0, 0 };
static Pot potSpeed = { POT_SPEED_PIN, 0, 0 };
static Pot potLevel = { POT_LEVEL_PIN, 0, 0 };

static inline int32_t potSample(const Pot &p) {
  uint32_t acc = 0;
  for (uint8_t i = 0; i < POT_OVERSAMPLE; i++) acc += analogReadMilliVolts(p.pin);
  return (int32_t)((acc / POT_OVERSAMPLE) << 8);
}

static inline uint8_t potScale(const Pot &p) {
  int32_t mv = p.ema >> 8;
  if (mv <= POT_MV_MIN) return 0;
  if (mv >= POT_MV_MAX) return 255;
  return (uint8_t)(((mv - POT_MV_MIN) * 255) / (POT_MV_MAX - POT_MV_MIN));
}

static inline uint8_t potRead(Pot &p) {
  p.ema += (potSample(p) - p.ema) >> POT_SMOOTH;
  uint8_t v = potScale(p);
  // one-count hysteresis: without it the last bit dithers and the hue crawls
  // while nobody is touching anything
  if (v > (uint8_t)(p.out + 1) || (uint8_t)(v + 1) < p.out) p.out = v;
  return p.out;
}

static inline void potPrime(Pot &p) {
  p.ema = potSample(p);
  p.out = potScale(p);
}

void controlsUpdate(bool force) {
  static uint8_t turn = 0;
  static uint32_t loggedAt = 0;
  static uint16_t lastSpeed = 0;

  // One pot per pass. Reading all three every frame costs ~1 ms of a ~9.5 ms
  // frame; round-robin keeps that under 4% and each knob still updates ~35x
  // a second, which is far quicker than a hand can turn one.
  if (force) { potPrime(potHue); potPrime(potSpeed); potPrime(potLevel); }
  else {
    switch (turn) {
      case 0: potRead(potHue);   break;
      case 1: potRead(potSpeed); break;
      default: potRead(potLevel); break;
    }
    turn = (turn + 1) % 3;
  }

  if (force || potHue.out != g_hue) { g_hue = potHue.out; updatePalette(); }

  uint8_t sp = potSpeed.out;
  if (sp > 128 - POT_SPEED_SNAP && sp < 128 + POT_SPEED_SNAP) sp = 128;   // find 1.00x by feel
  uint16_t q8 = (uint16_t)(256.0f * powf(2.0f, ((int)sp - 128) / 128.0f * POT_SPEED_OCTAVES) + 0.5f);
  if (force || q8 != lastSpeed) { lastSpeed = q8; g_speedQ8 = q8; }

  // Placeholder: a plain dimmer until the severity crossfade lands, at which
  // point this knob moves gap, stroke count, leader speed and afterglow
  // together instead.
  uint8_t ceil8 = scale8(MASTER, TRIM);
  uint8_t b = (uint8_t)(20 + ((uint32_t)potLevel.out * (ceil8 > 20 ? ceil8 - 20 : 0) / 255));
  if (force || b != g_bright) { g_bright = b; FastLED.setBrightness(b); }

#if STORM_LOG
  if (millis() - loggedAt >= POT_LOG_MS) {
    static uint8_t ph = 255, pl = 255; static uint16_t pq = 0;
    if (force || potHue.out != ph || potLevel.out != pl || g_speedQ8 != pq) {
      ph = potHue.out; pl = potLevel.out; pq = g_speedQ8;
      LOG("[pots]  hue=%-3u speed=%u.%02ux bright=%-3u\n", (unsigned)ph,
          (unsigned)(pq >> 8), (unsigned)((pq & 0xFF) * 100 / 256), (unsigned)g_bright);
      loggedAt = millis();
    }
  }
#endif
}

void controlsBegin() {
  analogReadResolution(12);
  controlsUpdate(true);
  LOG("[pots]  hue GPIO%u, speed GPIO%u, level GPIO%u\n",
      (unsigned)POT_HUE_PIN, (unsigned)POT_SPEED_PIN, (unsigned)POT_LEVEL_PIN);
}

#else   /* POTS_ENABLE == 0 — knobs compiled out, tuning.h rules */

inline void controlsUpdate(bool) {}
inline void controlsBegin() { LOG("[pots]  disabled\n"); }

#endif
