/* ─────────────────────────────────────────────────────────────
   config.h — YOURS. Bolt Bench never overwrites this file.

   Pins, board wiring, logging and palettes live here so that
   re-exporting from the bench (which replaces layout.h, tuning.h,
   storm.h and main.cpp wholesale) cannot clobber them.
   ───────────────────────────────────────────────────────────── */
#pragma once
#include <Arduino.h>

/* ── strip ──────────────────────────────────────────────────── */
/* GPIO2 is a strapping pin, but strapping pins are only sampled at reset —
   after boot it is an ordinary output, which is why this works. The one
   risk is something holding it high during reset and blocking boot.
   If you ever need to move it: GPIO13/18/19/21/22/23/25/26/27/32/33 are
   all clear. Avoid GPIO6-11 (SPI flash), GPIO34-39 (input only),
   GPIO1/3 (Serial), GPIO16/17 (PSRAM on WROVER modules). */
#define LED_PIN      GPIO_NUM_2
#define LED_TYPE     WS2812B
#define COLOR_ORDER  GRB

/* How many strikes may overlap, and how many strokes each may have.
   Exceeding MAX_STRIKES logs a DROPPED line rather than queueing. */
#define MAX_STRIKES  6
#define MAX_STROKES  6

/* ── logging ────────────────────────────────────────────────── */
/* 0 removes Serial, the format strings and the per-frame power
   measurement entirely — about 28 KB of flash on an ESP32.
   Override from platformio.ini with -DSTORM_LOG=0. */
#ifndef STORM_LOG
  #define STORM_LOG      1
#endif
#ifndef STORM_LOG_BAUD
  #define STORM_LOG_BAUD 115200
#endif
#ifndef STORM_LOG_STATS
  #define STORM_LOG_STATS 5000    // ms between dashboard refreshes when nothing changes
#endif
#ifndef STORM_LOG_MIN_MS
  #define STORM_LOG_MIN_MS 120    // floor between lines, so a burst cannot flood the port
#endif
#ifndef STORM_LOG_RAW
  #define STORM_LOG_RAW 0         // 1 appends the raw 0..255 pot readings
#endif

#if STORM_LOG
  #define LOG(...)  Serial.printf(__VA_ARGS__)
#else
  #define LOG(...)  do {} while (0)
#endif

/* ── palettes ───────────────────────────────────────────────── */
/* Starting points for HUE / SAT / HOT_SAT / TRIM in tuning.h.

   Two rules learned the hard way:

   Hue belongs in the afterglow. HOT_SAT stays low so the return stroke
   is near white — that white core is what makes the eye read "hot". A
   fully saturated stroke reads as a coloured tube light instead.

   The heat ramp shifts HUE as well as saturation, which matters most for
   warm palettes. Desaturating toward white adds green and blue equally:
   from blue that gives blue-white and reads as hot, but from red it gives
   PINK, because nothing in nature gets hot by going pink. Fire runs red to
   orange to yellow to white. So hotHue lets the peak sit at a different
   hue from the afterglow — red cooling from an amber core, rather than a
   salmon one. Leave hotHue equal to hue where no shift is wanted.

   TRIM compensates for the eye, not the LED. Green sits at the peak of
   human luminous sensitivity, so a green flash at MASTER 170 feels
   roughly twice as bright as blue at 170. Pull greens back. */
struct Palette { const char *name; uint8_t hue, hotHue, sat, hotSat, trim; };

__attribute__((unused))
const Palette PALETTES[] = {
  //  name           hue  hotHue  sat  hotSat  trim
  { "storm",         160,   160,  200,    40,   255 },  // cold blue-white, where this started
  { "tornado",        80,    64,  165,    30,   200 },  // sickly yellow-green; dread is in the LOW saturation
  { "voldemort",      96,    96,  220,    45,   190 },  // vivid unnatural green — menace rather than dread
  { "halloween",     192,   192,  205,    55,   235 },  // purple
  { "red planet",      4,    32,  235,    70,   245 },  // deep red cooling from an amber core, not a pink one
};
#define NUM_PALETTES (sizeof(PALETTES) / sizeof(PALETTES[0]))
