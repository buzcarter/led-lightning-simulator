/* ─────────────────────────────────────────────────────────────
   controls.h — YOURS. Bolt Bench never overwrites this file.

   Three B10K linear pots on ADC1. POTS_ENABLE 0 compiles the lot out
   and every value falls back to tuning.h.

   Wiring, per pot: outer leg -> 3V3, other outer leg -> GND, wiper ->
   the ADC pin, plus 100 nF from wiper to GND at the board end. 3.3 V,
   NOT 5 V — these pins are not 5 V tolerant. Swap the outer legs to
   reverse a knob's direction.

   All three knobs are 10-position detented rotary switches: a 3D-printed
   detent on each pot, read by DetentSwitch (lib/, from Project 16). A
   continuous knob on a noisy 12-bit ADC is the worst of both worlds: never
   quite still, never repeatable. With detents the hysteresis band is far
   wider than the noise, so a position holds exactly where you left it.
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

/* Detent calibration, from lib/DetentSwitch/DetentProfiles.h. DETENT_10 was
   measured on GPIO34 with one knob; the same print on the other two pins
   should land close. If one skips or sticks at a position, re-run the
   Project 16 tuner on that knob and give it its own profile. */
#define POT_PROFILE      DETENT_10

/* Sampling. One pot per interval, round-robin, so each knob is read every
   3 x POT_INTERVAL_MS — about 750 ms. Knob positions do not need to be
   fresh at frame rate, and sampling rarely but thoroughly beats sampling
   constantly and then filtering: averaging N conversions cuts the noise by
   sqrt(N), where a filter buys the same quiet only by adding lag.
   PotReader averages 16 conversions per read. */
#define POT_INTERVAL_MS  250

/* Speed detents, 8.8 fixed point, one per knob position. Geometric, because
   equal twists should give equal ratio changes; position 6 is exactly 1.00x.
   SPEED_STEPS lives in storm.h, beside LEVEL_STEPS, so the dashboard can see it. */
static const uint16_t SPEED_Q8[SPEED_STEPS] = {
   64,   84,  111,  147,  194,  256,  338,  446,  588,  776
/* 0.25x                        1.00x                   3.03x */
};

/* Brightness at level detent 0. Not zero — a knob position that makes the
   whole costume look broken is rarely what you want at the end of the
   travel. Set to 0 to turn it into an off switch. */
#define POT_LEVEL_FLOOR  20

#define POT_LOG_MS       400

/* ── mood LED ───────────────────────────────────────────────── */
/* A discrete 4-leg RGB LED on three PWM channels, showing which palette is
   live. It pulses on every strike and rides up during chaos, so one LED
   reports mood, activity and fury at a glance.

   Pinout of the usual 5 mm part (the one in the Elegoo starter kit): from the
   flat side, RED - CATHODE - GREEN - BLUE. The cathode is second and is the
   longest of the four leads. That is a common CATHODE, so MOOD_COMMON_ANODE
   stays 0. If yours is an anode part the long leg goes to 3V3 instead and the
   logic inverts; a multimeter in diode mode settles it -- black probe on the
   long leg, and if the other three light in turn it is a cathode.

   Resistors: one per colour, between each short leg and its GPIO.

   DO NOT use the 220R-on-all-three that starter-kit lessons specify. That is
   a 5 V figure. Red drops about 2.0 V, green and blue nearer 2.9 V, so at
   3.3 V a uniform 220R gives red 5.9 mA, green 1.8 mA and blue 1.4 mA -- red
   four times the others, and blue looking dead. Size them per colour:

       red      220R      ->  5.9 mA
       green  39R-68R    ->  9.8 mA at 41R, 5.9 mA at 68R
       blue   39R-68R    ->  7.3 mA at 41R, 4.4 mA at 68R

   Anything in that range is fine for green and blue -- lower is brighter,
   and even 39R stays inside both the LED's 20 mA rating and the ESP32's
   comfortable 12 mA per pin. Red is the exception: it has a whole volt more
   headroom, so the same small resistor would put it at over 30 mA, past both
   limits. Red keeps the 220R.

   Get the resistors right first. Only then trim the gains below. */
#ifndef MOOD_LED_ENABLE
  #define MOOD_LED_ENABLE 1
#endif
#define MOOD_PIN_R        GPIO_NUM_25
#define MOOD_PIN_G        GPIO_NUM_26
#define MOOD_PIN_B        GPIO_NUM_13
#ifndef MOOD_COMMON_ANODE
  #define MOOD_COMMON_ANODE 0      // 1 if the long leg goes to 3V3
#endif

#define MOOD_PWM_HZ    4000
#define MOOD_PWM_BITS  8
#define MOOD_CH_R      0           // LEDC channels; nothing else here uses them
#define MOOD_CH_G      1
#define MOOD_CH_B      2

/* Per-channel trim, set by eye, and only after the resistors are sized. With
   the values above the three channels draw roughly the same current, so there
   is no a-priori winner to correct for -- but the eye is far more sensitive to
   green than to blue, so expect to pull green down rather than push blue up.
   Show it white and trim whichever channel shouts. */
#ifndef MOOD_GAIN_R
  #define MOOD_GAIN_R  255
#endif
#ifndef MOOD_GAIN_G
  #define MOOD_GAIN_G  255
#endif
#ifndef MOOD_GAIN_B
  #define MOOD_GAIN_B  255
#endif

#define MOOD_IDLE      110         // resting level of the palette colour
#define MOOD_PULSE_MS  180         // strike flash decay

#if MOOD_LED_ENABLE

/* The LEDC API changed in Arduino-ESP32 3.x: channels went away and the pin
   itself became the handle. This works either side of that. */
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  #define MOOD_ATTACH(pin, ch)   ledcAttach((pin), MOOD_PWM_HZ, MOOD_PWM_BITS)
  #define MOOD_SET(pin, ch, d)   ledcWrite((pin), (d))
#else
  #define MOOD_ATTACH(pin, ch)   do { ledcSetup((ch), MOOD_PWM_HZ, MOOD_PWM_BITS); \
                                      ledcAttachPin((pin), (ch)); } while (0)
  #define MOOD_SET(pin, ch, d)   ledcWrite((ch), (d))
#endif

static inline void moodPut(gpio_num_t pin, uint8_t ch, uint8_t v) {
  MOOD_SET(pin, ch, MOOD_COMMON_ANODE ? (uint8_t)(255 - v) : v);
}

void moodBegin() {
  MOOD_ATTACH(MOOD_PIN_R, MOOD_CH_R);
  MOOD_ATTACH(MOOD_PIN_G, MOOD_CH_G);
  MOOD_ATTACH(MOOD_PIN_B, MOOD_CH_B);
  moodPut(MOOD_PIN_R, MOOD_CH_R, 0);
  moodPut(MOOD_PIN_G, MOOD_CH_G, 0);
  moodPut(MOOD_PIN_B, MOOD_CH_B, 0);
  LOG("[mood]  RGB on GPIO%u/%u/%u, common %s\n",
      (unsigned)MOOD_PIN_R, (unsigned)MOOD_PIN_G, (unsigned)MOOD_PIN_B,
      MOOD_COMMON_ANODE ? "anode" : "cathode");
}

void moodUpdate(uint32_t realNow) {
  uint8_t lvl = MOOD_IDLE;

  uint32_t since = realNow - g_strikeAt;
  if (g_strikeAt && since < MOOD_PULSE_MS) {          // brief flash on each strike
    uint8_t p = (uint8_t)(255 - since * 255 / MOOD_PULSE_MS);
    lvl = qadd8(lvl, scale8((uint8_t)(255 - MOOD_IDLE), p));
  }
  if (g_chaos) lvl = qadd8(lvl, scale8((uint8_t)(255 - lvl), g_chaos));

  // coolC is the saturated palette colour -- the mood itself, not the
  // near-white peak the strike renders at
  moodPut(MOOD_PIN_R, MOOD_CH_R, scale8(scale8(coolC.r, lvl), MOOD_GAIN_R));
  moodPut(MOOD_PIN_G, MOOD_CH_G, scale8(scale8(coolC.g, lvl), MOOD_GAIN_G));
  moodPut(MOOD_PIN_B, MOOD_CH_B, scale8(scale8(coolC.b, lvl), MOOD_GAIN_B));
}

#else
inline void moodBegin() {}
inline void moodUpdate(uint32_t) {}
#endif

/* ── chaos button ───────────────────────────────────────────── */
/* A momentary button to GND on CHAOS_PIN, using the internal pull-up, so it
   is two wires and no resistor. Do NOT put it on GPIO34-39: those pins have
   no internal pull-ups and would need an external one.

   A tap has a shape rather than being a flat flash. Every root channel fires
   at once, staggered so it reads as a cascade instead of one white frame;
   the scheduler's gaps collapse and every strike doubles for CHAOS_HOLD_MS;
   then it all eases back to the knobs over CHAOS_DECAY_MS, like the storm
   moving off. Tapping again restarts it.

   The envelope runs in real milliseconds while the strikes it schedules run
   on the storm clock — so a tap always lasts about four and a half seconds
   no matter where the speed knob is. */
#ifndef CHAOS_ENABLE
  #define CHAOS_ENABLE 1
#endif
#define CHAOS_PIN          GPIO_NUM_27
#define CHAOS_DEBOUNCE_MS  25
#define CHAOS_HOLD_MS      2500     // everything at maximum
#define CHAOS_DECAY_MS     2000     // easing back to the knob positions
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
    chaosAt = 0; g_chaos = 0;
    g_gapMin = GAP_MIN; g_gapMax = GAP_MAX; g_dblChance = DBL_CHANCE;
    FastLED.setBrightness(g_bright);
    return;
  }
  uint8_t amt = 255;
  if (e > CHAOS_HOLD_MS)
    amt = (uint8_t)(255 - (uint32_t)(e - CHAOS_HOLD_MS) * 255 / CHAOS_DECAY_MS);
  g_chaos = amt;

  g_gapMin    = chaosLerp16(GAP_MIN, CHAOS_GAP_MIN, amt);
  g_gapMax    = chaosLerp16(GAP_MAX, CHAOS_GAP_MAX, amt);
  g_dblChance = (uint8_t)chaosLerp16(DBL_CHANCE, 255, amt);

  uint8_t ceil8 = scale8(MASTER, g_trim);            // ignore the level knob while it rages
  uint8_t head  = ceil8 > g_bright ? ceil8 - g_bright : 0;
  FastLED.setBrightness((uint8_t)(g_bright + scale8(head, amt)));
}

#else
inline void chaosBegin() {}
inline void chaosPoll(uint32_t, uint32_t) {}
#endif

#if POTS_ENABLE

#include <DetentSwitch.h>
#include <DetentProfiles.h>

static DetentSwitch potPalette(POT_PALETTE_PIN, POT_PROFILE);
static DetentSwitch potSpeed  (POT_SPEED_PIN,   POT_PROFILE);
static DetentSwitch potLevel  (POT_LEVEL_PIN,   POT_PROFILE);

/* Knob position (1..N) as an index into a table of `steps` entries. A knob
   with more detents than the table has stays on the last entry. */
static inline uint8_t potIndex(const DetentSwitch &k, uint8_t steps) {
  uint8_t i = k.position() ? k.position() - 1 : 0;
  return i < steps ? i : steps - 1;
}

void controlsUpdate(bool force) {
  static uint32_t nextAt = 0;
  static uint8_t  turn = 0;

  if (force) {
    potPalette.update(); potSpeed.update(); potLevel.update();
  } else {
    if ((int32_t)(millis() - nextAt) < 0) return;
    nextAt = millis() + POT_INTERVAL_MS;
    switch (turn) {
      case 0:  potPalette.update(); break;
      case 1:  potSpeed.update();   break;
      default: potLevel.update();   break;
    }
    turn = (turn + 1) % 3;
  }

  /* Palette, not raw hue. Rotating hue alone leaves SAT wherever tuning.h
     left it — and at a low saturation every hue is the same near-white, so
     the knob appears dead. Each detent carries a whole look instead. With
     fewer palettes than detents they repeat round the dial, so every click
     still changes something; add palettes in config.h to fill it. */
  uint8_t pal = potIndex(potPalette, 255) % NUM_PALETTES;
  bool trimChanged = false;
  if (force || pal != g_palette) {
    g_palette = pal;
    g_hue    = PALETTES[pal].hue;
    g_hotHue = PALETTES[pal].hotHue;
    g_sat    = PALETTES[pal].sat;
    g_hotSat = PALETTES[pal].hotSat;
    g_trim   = PALETTES[pal].trim;
    updatePalette();
    trimChanged = true;
  }

  g_speed   = potIndex(potSpeed, SPEED_STEPS);
  g_speedQ8 = SPEED_Q8[g_speed];

  g_level = potIndex(potLevel, LEVEL_STEPS);
  uint8_t ceil8 = scale8(MASTER, g_trim);
  uint8_t span  = ceil8 > POT_LEVEL_FLOOR ? ceil8 - POT_LEVEL_FLOOR : 0;
  uint8_t b = (uint8_t)(POT_LEVEL_FLOOR + ((uint32_t)g_level * span / (LEVEL_STEPS - 1)));
  if (force || trimChanged || b != g_bright) { g_bright = b; FastLED.setBrightness(b); }

  g_potRaw[0] = potPalette.raw();
  g_potRaw[1] = potSpeed.raw();
  g_potRaw[2] = potLevel.raw();
}

void controlsBegin() {
  potPalette.begin(); potSpeed.begin(); potLevel.begin();
  controlsUpdate(true);
  LOG("[pots]  palette GPIO%u, speed GPIO%u, level GPIO%u, %u detents each\n",
      (unsigned)POT_PALETTE_PIN, (unsigned)POT_SPEED_PIN, (unsigned)POT_LEVEL_PIN,
      (unsigned)potLevel.positions());
  LOG("[pots]  %u palettes, %u speeds, %u levels; one knob every %u ms\n",
      (unsigned)NUM_PALETTES, (unsigned)SPEED_STEPS, (unsigned)LEVEL_STEPS,
      (unsigned)POT_INTERVAL_MS);
}

#else   /* POTS_ENABLE == 0 — knobs compiled out, tuning.h rules */

inline void controlsUpdate(bool) {}
inline void controlsBegin() { LOG("[pots]  disabled\n"); }

#endif
