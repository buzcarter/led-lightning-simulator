/* ─────────────────────────────────────────────────────────────
   sound.h — YOURS. Bolt Bench never overwrites this file.

   Thunder from a DFPlayer Mini. It plays one clip at a time, so the
   sounds take turns rather than layering: the ambient rumble plays until
   a strike's thunder interrupts it, and comes back when the thunder ends.
   Chaos plays one of its own clips at once and keeps it to the end.

   Pins, file counts, clip length and volume are in config.h, with the
   card's layout. Each name on the card starts with three digits, from 001
   with no gaps; anything after them is ignored, so "001 close crack.mp3"
   is fine. Each play picks one at random from its folder. audio/ in the
   project holds the card's files, laid out the same way.

   Wiring: DFPlayer VCC to 5 V and GND to the ESP32's ground. SOUND_TX_PIN
   to the DFPlayer's RX through a 1 kΩ resistor, which keeps the hiss
   down; the DFPlayer's TX to SOUND_RX_PIN, which only feeds the log. A
   speaker of 3 W or less straight across SPK1 and SPK2, and a 100-470 µF
   capacitor across VCC and GND at the module: the strip's current spikes
   otherwise come through the speaker as clicks.
   ───────────────────────────────────────────────────────────── */
#pragma once
#include <Arduino.h>
#include "storm.h"

/* 0 leaves the DFPlayer out entirely, library and all. */
#ifndef SOUND_ENABLE
  #define SOUND_ENABLE 1
#endif
#if SOUND_ENABLE
  #include <DFRobotDFPlayerMini.h>
#endif

/* Light arrives at once, sound does not: each strike's thunder waits a
   random THUNDER_DELAY, which reads as a storm at varying distances and
   hides the module's own start-up lag. A clip is longer than the gap
   between strikes, so a new one only cuts in once the last has played
   THUNDER_HOLD_MS; strikes before that are too far off to hear. */
#define THUNDER_DELAY_MIN   300
#define THUNDER_DELAY_MAX   2000
#define THUNDER_HOLD_MS     4000

#define SOUND_BOOT_MS     1500          // the module reading its card after power on
#define SOUND_CMD_GAP_MS  60            // the module drops commands sent closer than this

enum SoundFolder : uint8_t { SND_AMBIENT = 1, SND_STRIKE = 2, SND_CHAOS = 3 };

#if SOUND_ENABLE

static DFRobotDFPlayerMini dfp;
static SoundFolder soundNow = SND_AMBIENT; // what is playing
static bool     soundChaosOn = false;      // the clip playing is chaos's: strikes wait
static bool     soundStarted = false;      // the first rumble has gone out
static uint32_t soundAt = 0;               // millis the clip playing started
static uint32_t soundCmdAt = 0;            // millis of the last command of any kind
static uint32_t thunderAt = 0;             // a strike's thunder, due then; 0 = none
static uint8_t  soundVol = 255;            // what the module was last told
static uint8_t  soundLast[4] = {0};        // the last file per folder, to avoid repeats

/* Arduino's random(), not random16(): the storm's draws must stay the
   bench's, so the same seed replays the same storm. */
static uint8_t pickFile(SoundFolder f, uint8_t n) {
  if (n <= 1) return 1;
  uint8_t k = (uint8_t)random(1, n + 1);
  if (k == soundLast[f]) k = k % n + 1;    // never the same file twice running
  return k;
}

static void soundPlay(SoundFolder f, uint32_t realNow, const char *why) {
  uint8_t n = f == SND_AMBIENT ? SOUND_AMBIENT_FILES
            : f == SND_STRIKE  ? SOUND_STRIKE_FILES : SOUND_CHAOS_FILES;
  if (!n) return;
  uint8_t k = pickFile(f, n);
  dfp.playFolder(f, k);
  soundLast[f] = k;
  soundNow = f; soundAt = soundCmdAt = realNow;
  LOG("[sound] %s %02u/%03u.mp3\n", why, (unsigned)f, (unsigned)k);
}

/* Called by the scheduler for each strike it fires. */
static void soundStrike() {
  if (soundChaosOn || thunderAt) return;
  uint32_t now = millis();
  if (soundNow == SND_STRIKE && now - soundAt < THUNDER_HOLD_MS) return;
  thunderAt = now + random(THUNDER_DELAY_MIN, THUNDER_DELAY_MAX + 1);
  if (!thunderAt) thunderAt = 1;
}

/* Chaos cuts straight in, with no distance delay: it is right overhead. */
void soundChaos(uint32_t realNow) {
  thunderAt = 0;
  soundChaosOn = true;
  soundPlay(SND_CHAOS, realNow, "chaos");
}

/* Press t in the monitor: a strike clip now, no delay, for testing the
   wiring and the card without waiting on the storm. */
void soundTest() {
  thunderAt = 0;
  soundPlay(SND_STRIKE, millis(), "test");
}

/* Everything the module says, in words. Its TX back to SOUND_RX_PIN is the
   only way to see it, so silence here with the module powered means that
   wire, or the module is not running at all. Playback does not depend on
   it: clips are timed from SOUND_CLIP_MS. */
static void soundReport(uint8_t type, uint16_t value) {
  switch (type) {
    case DFPlayerCardOnline:   LOG("[sound] card ready\n"); break;
    case DFPlayerCardInserted: LOG("[sound] card inserted\n"); break;
    case DFPlayerCardRemoved:  LOG("[sound] card REMOVED\n"); break;
    case DFPlayerPlayFinished: LOG("[sound] finished file %u\n", (unsigned)value); break;
    case DFPlayerError: {
      const char *why = value == Busy             ? "busy: no card, or it cannot be read"
                      : value == Sleeping         ? "sleeping"
                      : value == SerialWrongStack ? "garbled command: check the 1 kΩ and the RX wire"
                      : value == CheckSumNotMatch ? "checksum: noise on the RX wire"
                      : value == FileIndexOut     ? "no such file: a count in config.h is too high"
                      : value == FileMismatch     ? "file not found: check the folder and file names"
                      : "unknown";
      LOG("[sound] ERROR %u, %s\n", (unsigned)value, why);
      break;
    }
    default: LOG("[sound] message %u, %u\n", (unsigned)type, (unsigned)value); break;
  }
}

void soundBegin() {
  Serial2.begin(9600, SERIAL_8N1, SOUND_RX_PIN, SOUND_TX_PIN);
  dfp.begin(Serial2, false, false);    // no ACK and no reset, so nothing here blocks a frame
  soundCmdAt = millis();
  g_onStrike = soundStrike;
  LOG("[sound] DFPlayer on GPIO%u (TX) / GPIO%u (RX), %u ambient + %u strike + %u chaos clips of %u ms\n",
      (unsigned)SOUND_TX_PIN, (unsigned)SOUND_RX_PIN, (unsigned)SOUND_AMBIENT_FILES,
      (unsigned)SOUND_STRIKE_FILES, (unsigned)SOUND_CHAOS_FILES, (unsigned)SOUND_CLIP_MS);
}

/* Once a frame. At most one command goes out per call, spaced by
   SOUND_CMD_GAP_MS, in order of what would be noticed most. */
void soundUpdate(uint32_t realNow) {
  while (dfp.available()) {
    uint8_t type = dfp.readType();
    soundReport(type, dfp.read());
    // a module slower to read its card than SOUND_BOOT_MS dropped the first
    // rumble; it says when the card is ready, so start again from there
    if (type == DFPlayerCardOnline || type == DFPlayerCardInserted) soundStarted = false;
  }

  bool ended = realNow - soundAt >= SOUND_CLIP_MS;
  if (ended) soundChaosOn = false;

  if (realNow < SOUND_BOOT_MS || realNow - soundCmdAt < SOUND_CMD_GAP_MS) return;

  uint8_t vol = (uint8_t)((uint16_t)SOUND_VOLUME * (g_level + 1) / LEVEL_STEPS);
  if (vol != soundVol) { dfp.volume(vol); soundVol = vol; soundCmdAt = realNow; return; }

  if (thunderAt && (int32_t)(realNow - thunderAt) >= 0) {
    thunderAt = 0;
    soundPlay(SND_STRIKE, realNow, "strike");
  } else if (!soundStarted || ended) {
    soundStarted = true;
    soundPlay(SND_AMBIENT, realNow, "ambient");   // a fresh pick each time
  }
}

#else
inline void soundTest() {}
inline void soundBegin() {}
inline void soundUpdate(uint32_t) {}
inline void soundChaos(uint32_t) {}
#endif
