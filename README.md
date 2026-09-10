# Bolt Bench

A browser bench for designing lightning animations on a 300-pixel addressable LED strip, and for exporting the result as a FastLED sketch that runs the same math.

Built for a wearable cloud costume: an LED rope stuffed into polyfill batting, with bolts hanging below.

## The problem

Writing lightning on a microcontroller is a bad feedback loop. Every tweak to a timing constant costs a compile, an upload, and a look — call it thirty seconds — and the thing you are trying to judge lasts twenty-two milliseconds. You cannot see a stepped leader at full speed. You certainly cannot see it while wearing the costume, in a dark room, holding a laptop.

The second problem is spatial. A cloud lights up in sympathy with the bolt that leaves it, brightest near the channel root and falling off with distance. That effect depends on where each pixel physically sits inside the batting — information that exists in your head and in the rope you twisted, but nowhere in the firmware.

This tool closes both gaps. You draw the routing, tune the storm at a tenth speed with a scope running, and export a sketch that carries the pixel coordinates with it.

## What this is

One self-contained HTML file. No build step, no dependencies, no server required. The only network request is a Google Fonts stylesheet; offline it falls back to system faces and everything still runs.

## Quick start

```bash
open /Users/buzcarter/Code/lightning-simulator/index.html
```

Double-clicking the file works too.

If you want your slider tuning to persist between sessions, serve it instead — browsers treat `file://` as an opaque origin and may discard `localStorage`. Every storage call is wrapped in `try`/`catch`, so it degrades quietly rather than breaking:

```bash
npx serve .
```

## How it's wired

### 1. Routing: SVG paths become LED indices

Routing is a list of **runs** in wiring order. Each run is one length of rope: a path plus a pixel count. The page samples that path at even arc-length intervals using `getTotalLength()` / `getPointAtLength()`, so pixels land evenly spaced along the curve no matter how it bends.

Indices are allocated cumulatively down the list. The default layout is:

| Run | Role | Count | Indices |
|---|---|---|---|
| Cloud body | cloud | 132 | `leds[0…131]` |
| Bolt · left | root channel | 38 | `leds[132…169]` |
| Fork · left | forks off left at 55% | 18 | `leds[170…187]` |
| Bolt · centre | root channel | 44 | `leds[188…231]` |
| Fork · centre | forks off centre at 45% | 22 | `leds[232…253]` |
| Bolt · right | root channel | 34 | `leds[254…287]` |
| Fork · right | forks off right at 40% | 12 | `leds[288…299]` |

Change any count and everything downstream shifts, in the UI and in the export together. Reorder runs with the arrows to match how you actually wired the rope.

The drawing surface is a 1000 × 760 viewBox. You can build paths three ways: click points directly on the stage (press `E` for edit mode), drag the handles to adjust, or paste an SVG `d` attribute straight out of Illustrator or Inkscape. Cloud runs smooth their points into Catmull-Rom curves; bolt runs stay as hard polyline zigzags, because that is what lightning looks like.

Parent links are stored by run id rather than by position, so reordering or deleting runs never silently reassigns a fork to the wrong channel. Cycles are detected on every rebuild and broken by detaching the offending run.

### 2. The math contract

This is the part that makes the preview worth trusting. The animation is not JavaScript that resembles Arduino code — it is the same arithmetic, transcribed:

- **`sin8`, `cos8`, `scale8`, `qadd8`, `qsub8`** are FastLED's `lib8tion` functions, including the `b_m16_interleave` lookup table `{0, 49, 49, 41, 90, 27, 117, 10}` that gives `sin8` its particular piecewise shape. Everything per-pixel is `uint8`.
- **`random16()`** is FastLED's LCG, `seed = seed * 2053 + 13849`, and `random8()` returns the low byte plus the high byte the way FastLED does. Both sides seed `0x1337`, so the hardware replays the storm you tuned rather than an unrelated one.
- **`hash8(index, frame)`** is stateless per-pixel noise. Flicker has to be reproducible frame to frame, which a sequential PRNG cannot give you when pixels are visited in a different order.
- **`dist8`** is an octagonal distance approximation, `max + min * 85/256`, roughly `hypot` within a few percent using no square root and no floats.

Every slider in the panel maps to a `#define` in the exported sketch.

### 3. The strike model

A strike is an **event**, precomputed at trigger time. When one fires, the tool rolls its leader speed, stroke count, per-stroke duration and brightness, and afterglow length, and freezes them into a struct. Rendering is then a pure function of elapsed milliseconds.

That is deliberate. It makes the preview scrubbable, it makes the same strike reproducible, and it maps cleanly onto a `millis()`-driven state machine on hardware.

The phases, with default tuning:

| Phase | Timing | What the pixels do |
|---|---|---|
| Stepped leader | 22–88 ms | A head races down the channel from the cloud end, leaving a decaying trail behind it |
| Return stroke | 12–46 ms | The whole channel goes white-hot, brightest and fastest of the sequence |
| Restrikes | 1–4 strokes, 30–140 ms apart | Repeat flashes at varying brightness; some only light *part* of the channel, as real restrikes do |
| Afterglow | ~330 ms | The whole tree decays and cools — heat drops from 255 toward 0, and colour slides from white to blue |
| Cloud decay | 540 ms | The cloud holds its glow longer than the bolt, which is what sells the depth |

Bolt runs also taper toward the tip, so the channel thins out as it descends.

### 4. Forks

A fork is a real second length of rope, attached partway down another channel — so it is modelled as a run with a **parent** and an **attachment point** (`forkAt`, 0–255 along the parent), not as a rendering trick.

Two things make it read as lightning rather than as two bolts that happen to fire together:

**The fork's leader launches late and travels at the same speed.** It starts when the parent's leader head reaches the attachment point, and its duration is scaled by length, `parentLeaderMs × forkLen / parentLen`. Propagation velocity is therefore constant across the whole tree, which means a branch and its trunk reach the ground at about the same moment. That is what real forked lightning does, and it is the detail that sells it.

**Forks sit out some restrikes.** Real return strokes often re-illuminate only the main channel. Each strike rolls a `forkMask` at creation time — one bit per stroke, the return stroke always set — and branches light only on the strokes whose bit is set. Two sliders control this: **Fork brightness** (how much dimmer a branch is than its trunk, compounding at each level) and **Fork restrike** (the odds a given restrike travels into the branches).

Forks nest. A fork can itself have forks, to a depth of 3.

Only root channels are ever struck directly — the storm scheduler and `randomBolt()` both filter to `parent < 0`. Clicking a branch on the stage fires the channel it belongs to.

In the routing list, every bolt run gets a **fork of** selector and an attachment percentage. The fork's first point is pinned to its parent's path and redraws when you move the parent or change the percentage, because the rope really is attached; it shows as an amber handle and is the one point you cannot drag.

### 5. Cloud sympathy, and why coordinates ship with the sketch

When a bolt fires, every cloud pixel picks up light based on its **2-D distance from that bolt's root** — not its distance in strip-index space. A rope serpentining through batting has neighbours in space that are nowhere near each other in index order, and index-space falloff looks obviously wrong.

So the export includes the geometry:

```c
const uint8_t LED_X[NUM_LEDS] PROGMEM = { ... };
const uint8_t LED_Y[NUM_LEDS] PROGMEM = { ... };
```

Positions are normalised to 0–255 across the long axis, which keeps distances isotropic. Each segment also records its own root point (`ox`, `oy`). This is the payload that makes the SVG matter to the microcontroller instead of being just a preview toy.

### 6. Colour: the hue lives in the afterglow

What makes this read as lightning is not the colour, it is the **heat axis** — the channel is white-hot at the stroke and cools to a tint as it fades. So the colour model is a ramp between two endpoints, and `heat` walks between them:

```c
leds[i].r = qadd8(leds[i].r, scale8(mix8(coolC.r, hotC.r, heat), bright));
```

`coolC` is the fully saturated `HUE` at `SAT`. `hotC` is the same hue at `HOT_SAT`, which stays low so the return stroke keeps a near-white core. That constraint is the whole trick: push `HOT_SAT` up and the effect stops looking like lightning and starts looking like a coloured tube light. Hue belongs in the decay, not the strike.

Both endpoints are resolved **once per frame**, not per pixel, so the per-pixel cost is three `mix8` and three `scale8` — the same as when the palette was hardcoded blue.

`TRIM` compensates for the eye rather than the LED. Green sits at the peak of human luminous sensitivity, so a green flash at `MASTER` 170 reads about twice as bright as blue at 170. Starting points live in `PALETTES[]` in `config.h`:

| Palette | Hue | Sat | Hot sat | Trim | Reads as |
|---|---|---|---|---|---|
| storm | 160 | 200 | 40 | 255 | cold blue-white |
| tornado | 80 | 165 | 30 | 200 | sickly yellow-green — the dread is in the *low* saturation |
| voldemort | 96 | 220 | 45 | 190 | vivid unnatural green — menace rather than dread |
| halloween | 192 | 205 | 55 | 235 | purple |
| red planet | 8 | 235 | 60 | 245 | mars dust |

The bench runs FastLED's own `hsv2rgb_rainbow`, transcribed, so the swatches and the preview land on the same RGB the strip will.

## Using the bench

### Controls

| Key / action | Effect |
|---|---|
| `Space` | Play / pause |
| `S` | Fire a random bolt |
| `E` | Toggle routing edit mode |
| `Backspace` | Remove the last point (edit mode) |
| Click a bolt on the stage | Fire that specific bolt |
| Rate slider | 0.03× to 2.00× |

Drop the rate to about **0.10×** when tuning. A 22 ms leader is invisible at full speed and obvious at a tenth.

### The two instruments

**The filmstrip** under the stage is literally `leds[0…299]` — the buffer the controller ships, drawn one pixel per LED and scaled up. It carries an index ruler every 25 pixels and labelled segment brackets. This is where you catch an off-by-one in your wiring order.

**The scope** below it plots channel peak against cloud mean over roughly a ten-second window. Use it to balance afterglow against cloud decay: if the white line drops to zero long before the blue fill does, the cloud is holding too long, and vice versa.

Four presets — Distant storm, Rolling, Direct hit, Heat lightning — are starting points, not destinations.

## Export

The **Export** panel emits five source files plus the layout. They are split along the line of **ownership**, not by size — which is what makes re-exporting safe:

| File | Owner | Overwritten on export |
|---|---|---|
| `config.h` | **you** — pins, logging, palettes | never; the bench emits it as a starter only |
| `layout.h` | generator — segments, `LED_X`/`LED_Y` | wholesale |
| `tuning.h` | generator — animation parameters | wholesale |
| `storm.h` | generator — the engine | wholesale |
| `main.cpp` | generator — `setup()` / `loop()` | wholesale |

Drop the four generated files into `src/` after any export; leave your `config.h` alone.

`layout.json` round-trips back into the tool via **Import layout JSON**, so you can version your routing or move it between machines.

Both use Copy rather than a download — this page is often viewed in a sandboxed frame where browser-initiated downloads are blocked, and a Copy button that always works beats a download link that sometimes does not.

### Logging

The sketch logs to serial at 115200 (matching `monitor_speed` in `platformio.ini`). Every log line is behind one flag:

```c
#define STORM_LOG  1     // 0 removes all of it
```

Setting it to `0` — or passing `-DSTORM_LOG=0` in `build_flags`, since the define is `#ifndef`-guarded — drops Serial, the format strings, and the per-frame power measurement entirely. On an ESP32 that is **28 KB of flash and 368 bytes of RAM**, so the off switch is real rather than cosmetic.

Five events are reported:

| Event | When | Why you want it |
|---|---|---|
| Boot table | Once at startup | Dumps every segment with its name, indices, role and attachment — confirms the firmware's idea of your routing matches the bench's |
| `strike` | A channel fires | Names the channel and prints the rolled leader, stroke count, `forkMask` and total duration |
| `done` | A strike finishes | Pairs with the above so you can see overlap |
| `DROPPED` | No free strike slot | The `MAX_STRIKES` diagnostic. If you see these, the storm is scheduling faster than it can render |
| Heartbeat | Every 5 s | Frame rate, active strike count, measured peak draw in mA, and time to the next strike |

Roughly what it looks like:

```
[storm] StormCloud ready - 300 px, 7 segments, pin 2, master 170
[storm] segment          start   len   role  attachment
[storm] Cloud body           0   132  cloud  -
[storm] Bolt - left        132    38   bolt  root channel
[storm] Fork - left        170    18   fork  off Bolt - left at 54%
[storm] strike  Bolt - centre   slot=0 leader=180ms strokes=1 forkMask=0x01 total=1481ms
[storm]   done  Bolt - centre   after 1482ms
[storm] 98 fps  active=0  peak draw=3180 mA  next strike in 2204 ms
```

That peak draw figure comes from FastLED's own `calculate_unscaled_power_mW`, so it is the number to watch against whatever supply you end up using — see below.

### Hardware notes

**300 pixels will not fit on an AVR Nano.** The frame buffer alone is 900 bytes of a 2 KB budget, before strike state. Use an ESP32, Teensy, or RP2040.

**Power is the real constraint.** WS2812B pixels draw roughly 60 mA each at full white. All 300 at once is about 18 A at 5 V, or 14.8 A at the default master brightness of 210. Sheet-lightning flashes genuinely do drive most of the cloud near white simultaneously, so that peak is real, not theoretical — even though the flashes are brief enough that average draw stays low.

What that means in practice:

- Cap the draw with `FastLED.setMaxPowerInVoltsAndMilliamps(5, N)` rather than trusting the animation to behave. This is the single most useful line you can add.
- Inject power at both ends of the strip, and at the midpoint if you can reach it.
- Put a 1000 µF electrolytic across 5 V and ground at the strip, and a 330–470 Ω resistor in series with the data line.
- Level-shift 3.3 V data up to 5 V on ESP32 and RP2040.

A sagging supply does not fail cleanly — it produces glitching and colour corruption that looks exactly like a software bug, and will cost you an evening.

Default pin is `LED_PIN 6`, type `WS2812B`, order `GRB`. Change them at the top of the sketch.

## Known limits

- **The sketch compiles but has never run on hardware.** `pio run` succeeds clean for `esp32dev` with FastLED 3.9 — no warnings — so the syntax and API use are verified. Timing, colour and the power figures are still unproven on real pixels.
- Fork nesting is capped at depth 3, in both the preview and the sketch.
- `MAX_STRIKES` is 6 and `MAX_STROKES` is 6. Push the storm parameters hard enough and strikes will be dropped rather than queued.
- Preview-only controls — dot size, bloom — model light diffusing through batting. They are not exported, because the controller has no equivalent.

## Building the firmware

```bash
pio run                 # compile
pio run -t upload       # flash
pio device monitor      # watch the log
```

`platformio.ini` targets `esp32dev`. Re-exporting overwrites four of the five source files, but never `config.h` — so your pin choice, logging settings and palettes survive.

## Files

```
lightning-simulator/
├── index.html        the bench: routing, tuning, export
├── README.md         this file
├── platformio.ini    ESP32 build config
└── src/
    ├── config.h      YOURS: pins, logging, palettes — never regenerated
    ├── layout.h      generated: segment table, pixel coordinates
    ├── tuning.h      generated: animation parameters and palette
    ├── storm.h       generated: the engine
    ├── main.cpp      generated: setup() and loop()
    └── layout.json   exported routing, re-importable into the bench
```
