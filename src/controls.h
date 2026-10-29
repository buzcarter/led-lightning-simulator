/* ─────────────────────────────────────────────────────────────
   controls.h — YOURS. Bolt Bench never overwrites this file.

   Two inputs: the chaos button on the board, and keys typed into the
   serial monitor on a tethered computer. Palette, speed, cloud speed and
   level are set from the keyboard; there are no knobs.
   ───────────────────────────────────────────────────────────── */
#pragma once
#include <Arduino.h>
#include "storm.h"

/* ── settings ───────────────────────────────────────────────── */
/* Speed steps, 8.8 fixed point, one per step. Geometric, because equal
   presses should give equal ratio changes; step 6 is exactly 1.00x.
   SPEED_STEPS lives in storm.h, beside LEVEL_STEPS, so the dashboard can
   see it. 0.25x at step 1, 3.03x at step 10. The cloud speed uses the same
   table. */
static const uint16_t SPEED_Q8[SPEED_STEPS] = {
   64,   84,  111,  147,  194,  256,  338,  446,  588,  776
};

/* Brightness at level step 1. Not zero — a setting that makes the whole
   costume look broken is rarely what you want at the end of the range.
   Set to 0 to turn it into an off switch. */
#define LEVEL_FLOOR  20

/* Recomputes everything that follows from g_palette, g_speed, g_cloud and g_level. */
void controlsApply() {
  /* Palette, not raw hue. Rotating hue alone leaves SAT wherever tuning.h
     left it — and at a low saturation every hue is the same near-white.
     Each palette carries a whole look instead. */
  const Palette &p = PALETTES[g_palette];
  g_hue = p.hue; g_hotHue = p.hotHue; g_sat = p.sat; g_hotSat = p.hotSat; g_trim = p.trim;
  updatePalette();

  g_speedQ8 = SPEED_Q8[g_speed];
  g_cloudQ8 = SPEED_Q8[g_cloud];

  /* Level spans LEVEL_FLOOR up to the palette's ceiling, MASTER x TRIM. */
  uint8_t ceil8 = scale8(MASTER, g_trim);
  uint8_t span  = ceil8 > LEVEL_FLOOR ? ceil8 - LEVEL_FLOOR : 0;
  g_bright = (uint8_t)(LEVEL_FLOOR + ((uint32_t)g_level * span / (LEVEL_STEPS - 1)));
}

/* ── chaos button ───────────────────────────────────────────── */
/* A momentary button to GND on CHAOS_PIN, using the internal pull-up, so it
   is two wires and no resistor. Do NOT put it on GPIO34-39: those pins have
   no internal pull-ups and would need an external one.

   A tap has a shape rather than being a flat flash. Every root channel fires
   at once, staggered so it reads as a cascade instead of one white frame;
   the scheduler's gaps collapse and every strike doubles for CHAOS_HOLD_MS;
   then it all eases back to the settings over CHAOS_DECAY_MS, like the storm
   moving off. Tapping again restarts it.

   The envelope runs in real milliseconds while the strikes it schedules run
   on the storm clock — so a tap always lasts about four and a half seconds
   no matter what the speed is. */
#ifndef CHAOS_ENABLE
  #define CHAOS_ENABLE 1
#endif
#define CHAOS_PIN          GPIO_NUM_27
#define CHAOS_DEBOUNCE_MS  25
#define CHAOS_HOLD_MS      2500     // everything at maximum
#define CHAOS_DECAY_MS     2000     // easing back to the settings
#define CHAOS_STAGGER_MS   90       // between channels in the opening volley
#define CHAOS_GAP_MIN      90       // scheduler gaps while held
#define CHAOS_GAP_MAX      260

#if CHAOS_ENABLE

static uint32_t chaosAt = 0;        // millis of the last tap; 0 = idle

static inline uint16_t chaosLerp16(uint16_t base, uint16_t target, uint8_t amt) {
  return (uint16_t)((int32_t)base + ((int32_t)target - (int32_t)base) * amt / 255);
}

void chaosFire(uint32_t realNow, uint32_t stormNow) {
  chaosAt = realNow ? realNow : 1;
  uint8_t k = 0;
  for (uint8_t i = 0; i < NUM_SEGMENTS; i++) {
    Segment sg; memcpy_P(&sg, &SEGMENTS[i], sizeof(Segment));
    if (!sg.isBolt || sg.parent >= 0) continue;      // forks come with their parent
    trigger(i, stormNow + (uint32_t)k * CHAOS_STAGGER_MS);
    k++;
  }
  LOG("[storm] CHAOS -- %u channels\n", (unsigned)k);
}

/* Back to the settings at once, wherever the envelope was. */
void chaosCancel() {
  chaosAt = 0; g_chaos = 0;
  g_gapMin = GAP_MIN; g_gapMax = GAP_MAX; g_dblChance = DBL_CHANCE;
}

void chaosBegin() {
  pinMode(CHAOS_PIN, INPUT_PULLUP);
  LOG("[storm] chaos button GPIO%u to GND\n", (unsigned)CHAOS_PIN);
}

void chaosPoll(uint32_t realNow, uint32_t stormNow) {
  static uint8_t  lastRaw = HIGH, stable = HIGH;
  static uint32_t changedAt = 0;

  uint8_t raw = digitalRead(CHAOS_PIN);
  if (raw != lastRaw) { lastRaw = raw; changedAt = realNow; }
  if (realNow - changedAt >= CHAOS_DEBOUNCE_MS && stable != lastRaw) {
    stable = lastRaw;
    if (stable == LOW) chaosFire(realNow, stormNow);   // active low
  }

  if (!chaosAt) return;
  uint32_t e = realNow - chaosAt;
  if (e >= (uint32_t)CHAOS_HOLD_MS + CHAOS_DECAY_MS) {
    chaosCancel();
    return;
  }
  uint8_t amt = 255;
  if (e > CHAOS_HOLD_MS)
    amt = (uint8_t)(255 - (uint32_t)(e - CHAOS_HOLD_MS) * 255 / CHAOS_DECAY_MS);
  g_chaos = amt;                                     // frameBrightness() reads this too

  g_gapMin    = chaosLerp16(GAP_MIN, CHAOS_GAP_MIN, amt);
  g_gapMax    = chaosLerp16(GAP_MAX, CHAOS_GAP_MAX, amt);
  g_dblChance = (uint8_t)chaosLerp16(DBL_CHANCE, 255, amt);
}

#else
inline void chaosCancel() {}
inline void chaosBegin() {}
inline void chaosPoll(uint32_t, uint32_t) {}
#endif

/* ── keyboard ───────────────────────────────────────────────── */
/* Keys typed into the serial monitor (pio device monitor passes them through).

     p s c l pick palette, speed (the bolts), cloud speed or level
     + -     step it up or down; past either end it wraps to the other
             (= and _ work too, so shift is optional)
     h       hold mode on/off: one segment lit steady, the storm paused
     0-9     while holding, the segment to light, in SEGMENTS[] order

   Each change prints the new value, and the dashboard line follows. Needs
   STORM_LOG, which is what opens the serial port; without it the settings
   stay at their boot values. */
#if STORM_LOG

enum Setting : uint8_t { SET_NONE, SET_PALETTE, SET_SPEED, SET_CLOUD, SET_LEVEL };
static Setting keySetting = SET_NONE;

static const char *settingName(Setting s) {
  return s == SET_PALETTE ? "palette" : s == SET_SPEED ? "speed"
       : s == SET_CLOUD ? "cloud" : "level";
}

/* One step up (dir +1) or down (-1) through n values, wrapping at both ends. */
static inline uint8_t stepWrap(uint8_t v, int8_t dir, uint8_t n) {
  return dir > 0 ? (v + 1 < n ? v + 1 : 0) : (v > 0 ? v - 1 : n - 1);
}

static void logSpeed(const char *name, uint8_t step, uint16_t q8) {
  LOG("[keys]  %s %u/%u %u.%02ux\n", name, (unsigned)(step + 1), (unsigned)SPEED_STEPS,
      (unsigned)(q8 >> 8), (unsigned)((q8 & 0xFF) * 100 / 256));
}

static void logSetting(Setting s) {
  switch (s) {
    case SET_PALETTE:
      LOG("[keys]  palette %u/%u %s\n", (unsigned)(g_palette + 1), (unsigned)NUM_PALETTES,
          PALETTES[g_palette].name);
      break;
    case SET_SPEED: logSpeed("speed", g_speed, g_speedQ8); break;
    case SET_CLOUD: logSpeed("cloud", g_cloud, g_cloudQ8); break;
    case SET_LEVEL:
      LOG("[keys]  level %u/%u\n", (unsigned)(g_level + 1), (unsigned)LEVEL_STEPS);
      break;
    default: break;
  }
}

static void keyStep(int8_t dir) {
  switch (keySetting) {
    case SET_PALETTE: g_palette = stepWrap(g_palette, dir, NUM_PALETTES); break;
    case SET_SPEED:   g_speed   = stepWrap(g_speed,   dir, SPEED_STEPS);  break;
    case SET_CLOUD:   g_cloud   = stepWrap(g_cloud,   dir, SPEED_STEPS);  break;
    case SET_LEVEL:   g_level   = stepWrap(g_level,   dir, LEVEL_STEPS);  break;
    default: LOG("[keys]  pick p, s, c or l first\n"); return;
  }
  controlsApply();
  logSetting(keySetting);
}

void controlsBegin() {
  LOG("[keys]  p s c l pick palette, speed, cloud or level; + - step it; h hold, then 0-%u\n",
      (unsigned)(NUM_SEGMENTS - 1));
}

void controlsPoll() {
  while (Serial.available()) {
    int c = tolower(Serial.read());
    switch (c) {
      case 'p': keySetting = SET_PALETTE; break;
      case 's': keySetting = SET_SPEED;   break;
      case 'c': keySetting = SET_CLOUD;   break;
      case 'l': keySetting = SET_LEVEL;   break;
      case '+': case '=': keyStep(+1); continue;
      case '-': case '_': keyStep(-1); continue;
      case 'h':
        if (!g_hold) chaosCancel();          // so hold shows the level setting's brightness
        holdSet(!g_hold);
        continue;
      default:
        if (g_hold && c >= '0' && c <= '9') holdSelect((uint8_t)(c - '0'));
        continue;
    }
    LOG("[keys]  %s selected, + - to step\n", settingName(keySetting));
    logSetting(keySetting);
  }
}

#else
inline void controlsBegin() {}
inline void controlsPoll() {}
#endif
