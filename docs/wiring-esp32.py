#!/usr/bin/env python3
# Generates docs/wiring-esp32.svg. Run: python3 wiring.py <out.svg>
import sys

W, H = 1560, 1130
out = []
def add(s): out.append(s)

def line(x1, y1, x2, y2, cls="w"): add(f'<line class="{cls}" x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}"/>')
def path(pts, cls="w"):
    d = "M" + " L".join(f"{x},{y}" for x, y in pts)
    add(f'<path class="{cls}" d="{d}"/>')
def text(x, y, s, cls="t", anchor="start"): add(f'<text class="{cls}" x="{x}" y="{y}" text-anchor="{anchor}">{s}</text>')
def dot(x, y): add(f'<circle class="jn" cx="{x}" cy="{y}" r="3.5"/>')

def gnd(x, y):  # wire must already reach (x, y)
    line(x - 11, y, x + 11, y, "w")
    line(x - 7, y + 5, x + 7, y + 5, "w")
    line(x - 3, y + 10, x + 3, y + 10, "w")

def flag(x, y, label, cls="v33"):  # supply flag; wire comes up to (x, y)
    line(x - 11, y, x + 11, y, cls)
    text(x, y - 7, label, f"flag {cls}", "middle")

def res_h(cx, cy, label, sub=None, w=52):
    add(f'<rect class="part" x="{cx - w/2}" y="{cy - 8}" width="{w}" height="16" rx="2"/>')
    text(cx, cy - 13, label, "val", "middle")
    if sub: text(cx, cy + 23, sub, "note", "middle")

def cap_v(x, y, label, anchor="start", dx=16):  # plates at y and y+8
    line(x - 12, y, x + 12, y, "plate")
    line(x - 12, y + 8, x + 12, y + 8, "plate")
    text(x + dx, y + 8, label, "val", anchor)

# ── layout constants ────────────────────────────────────────────
BX0, BX1, BY0, BY1 = 670, 930, 96, 890
PL, PR = 688, 912            # header pin x
TAGL0, TAGL1 = 572, 662      # left tag column
TAGR0, TAGR1 = 938, 1028     # right tag column
def py(i): return 190 + 42 * i

LEFT = [  # silkscreen, tag text, kind
    ("EN",  "RESET",  "ctl"),
    ("VP",  "GPIO36", "in"),
    ("VN",  "GPIO39", "in"),
    ("D34", "GPIO34", "io"),
    ("D35", "GPIO35", "in"),
    ("D32", "GPIO32", "io"),
    ("D33", "GPIO33", "io"),
    ("D25", "GPIO25", "io"),
    ("D26", "GPIO26", "io"),
    ("D27", "GPIO27", "chaos"),
    ("D14", "GPIO14", "io"),
    ("D12", "GPIO12", "io"),
    ("D13", "GPIO13", "io"),
    ("GND", "GND",    "gnd"),
    ("VIN", "5V IN",  "v5"),
]
RIGHT = [
    ("D23", "GPIO23", "io"),
    ("D22", "GPIO22", "io"),
    ("TX0", "GPIO1",  "io"),
    ("RX0", "GPIO3",  "io"),
    ("D21", "GPIO21", "io"),
    ("D19", "GPIO19", "io"),
    ("D18", "GPIO18", "strip"),
    ("D5",  "GPIO5",  "io"),
    ("TX2", "GPIO17", "io"),
    ("RX2", "GPIO16", "io"),
    ("D4",  "GPIO4",  "io"),
    ("D2",  "GPIO2",  "io"),
    ("D15", "GPIO15", "io"),
    ("GND", "GND",    "gnd"),
    ("3V3", "3V3 OUT", "io"),
]
# input-only pins (no output, no pull-ups) get a dashed tag
INPUT_ONLY = {"GPIO34", "GPIO35", "GPIO36", "GPIO39"}

# ── header ─────────────────────────────────────────────────────
add(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" font-size="13">')
add('''<title>StormCloud wiring: ESP32 DevKit V1</title>
<style>
  svg{--bg:#fbfaf7;--ink:#1d2024;--ink2:#5a6068;--ink3:#8d939b;--rule:#d9d6cf;
      --board:#1f2328;--board-ink:#e9ecef;--module:#c9ccd1;--module-ink:#2a2e33;
      --v33:#c8463a;--v5:#d07a12;--gnd:#30343a;
      --chaos:#b3811c;--strip:#3f8f5a;--io:#9aa0a8}
  @media (prefers-color-scheme: dark){
    svg{--bg:#15181c;--ink:#e8eaed;--ink2:#a2a9b2;--ink3:#6e757e;--rule:#30353b;
        --board:#0c0e10;--board-ink:#d8dce0;--module:#3a3f46;--module-ink:#e0e3e6;
        --v33:#ef7466;--v5:#f0a243;--gnd:#b7bec6;
        --chaos:#e0b14f;--strip:#68ba84;--io:#6e757e}}
  .bg{fill:var(--bg)}
  text{font-family:ui-monospace,"SF Mono",Menlo,Consolas,monospace;fill:var(--ink)}
  .h1{font-family:system-ui,-apple-system,"Helvetica Neue",Arial,sans-serif;font-size:26px;font-weight:700}
  .h2{font-family:system-ui,-apple-system,"Helvetica Neue",Arial,sans-serif;font-size:14px;fill:var(--ink2)}
  .ttl{font-family:system-ui,-apple-system,"Helvetica Neue",Arial,sans-serif;font-size:14px;font-weight:700;letter-spacing:.06em}
  .t{font-size:12px}
  .val{font-size:12px;font-weight:600}
  .note{font-size:11px;fill:var(--ink2)}
  .flag{font-size:11px;font-weight:700}
  .w{stroke:var(--ink);stroke-width:2;fill:none;stroke-linecap:round;stroke-linejoin:round}
  .plate{stroke:var(--ink);stroke-width:3;stroke-linecap:round}
  .part{fill:var(--bg);stroke:var(--ink);stroke-width:2}
  .jn{fill:var(--ink)}
  .v33{stroke:var(--v33);stroke-width:2.5;fill:var(--v33)}
  .v5{stroke:var(--v5);stroke-width:2.5;fill:var(--v5)}
  text.v33,text.v5{stroke:none}
  .board{fill:var(--board)}
  .silk{fill:var(--board-ink);font-size:12.5px;font-weight:700}
  .silk-s{fill:var(--board-ink);font-size:10px}
  .pin{fill:#c9a74a;stroke:#7d6420;stroke-width:1}
  .module{fill:var(--module)}
  .module-t{fill:var(--module-ink);font-size:10px;font-weight:700}
  .chip{fill:#2c3137;stroke:#4a5058;stroke-width:1}
  .btn{fill:#3a3f46;stroke:#6b727a;stroke-width:1.5}
  .btn-cap{fill:#15171a}
  .tag{stroke-width:1.5}
  .tag-t{font-size:11px;font-weight:600}
  .box{fill:none;stroke:var(--rule);stroke-width:1.5;stroke-dasharray:5 4}
  .card{fill:var(--bg);stroke:var(--rule);stroke-width:1.5}
</style>''')
add(f'<rect class="bg" width="{W}" height="{H}"/>')

text(30, 46, "StormCloud wiring · ESP32 DevKit V1 (30-pin)", "h1")
text(30, 70, "Top view, USB at the bottom. Pin order matches the board silkscreen. Pins in colour are used; grey are free.", "h2")

# ── keyboard: the settings come over USB, not from parts on the board ──
KX, KY, KW = 60, 180, 440
add(f'<rect class="card" x="{KX}" y="{KY}" width="{KW}" height="236" rx="8"/>')
text(KX + 20, KY + 32, "CONTROLS", "ttl")
text(KX + 124, KY + 32, "keyboard, over the USB cable", "note")
keys = [("p  s  l", "pick palette, speed or level"),
        ("+  -", "step it; wraps past either end"),
        ("h", "hold: one segment lit, storm paused"),
        ("0 – 6", "while holding, the segment to light")]
for k, (key, what) in enumerate(keys):
    y = KY + 70 + k * 30
    text(KX + 20, y, key, "val")
    text(KX + 110, y, what, "t")
text(KX + 20, KY + 196, "Type into pio device monitor. Each change prints", "note")
text(KX + 20, KY + 212, "a [keys] line, and the dashboard follows.", "note")

# ── board ──────────────────────────────────────────────────────
add(f'<rect class="board" x="{BX0}" y="{BY0}" width="{BX1-BX0}" height="{BY1-BY0}" rx="14"/>')
for cx, cy in [(BX0+16, BY0+16), (BX1-16, BY0+16), (BX0+16, BY1-16), (BX1-16, BY1-16)]:
    add(f'<circle cx="{cx}" cy="{cy}" r="8" fill="var(--bg)"/>')
# module + antenna
add('<rect class="module" x="745" y="104" width="110" height="300" rx="4"/>')
add('<path d="M757,118 h14 v16 h12 v-16 h12 v16 h12 v-16 h12 v16 h12 v-16 h12" stroke="var(--module-ink)" stroke-width="2" fill="none"/>')
add('<rect x="752" y="150" width="96" height="246" rx="3" fill="none" stroke="var(--module-ink)" stroke-width="1" opacity=".5"/>')
text(800, 262, "ESP-WROOM-32", "module-t", "middle")
text(800, 278, "WiFi · BT", "module-t", "middle")
# on-board parts
add('<rect class="chip" x="738" y="560" width="44" height="26" rx="2"/>')
text(760, 602, "3V3 REG", "silk-s", "middle")
add('<rect class="chip" x="784" y="650" width="44" height="44" rx="3"/>')
text(806, 710, "CP2102", "silk-s", "middle")
# LEDs
add('<rect x="770" y="452" width="14" height="9" rx="1.5" fill="#e0443a"/>')
text(777, 478, "PWR", "silk-s", "middle")
add('<rect x="818" y="452" width="14" height="9" rx="1.5" fill="#3d8ef0"/>')
text(825, 478, "GPIO2", "silk-s", "middle")
# buttons + USB
for bx, lab in [(712, "EN"), (888, "BOOT")]:
    add(f'<rect class="btn" x="{bx-17}" y="826" width="34" height="34" rx="3"/>')
    add(f'<circle class="btn-cap" cx="{bx}" cy="843" r="9"/>')
    text(bx, 818, lab, "silk", "middle")
add('<rect x="775" y="852" width="50" height="42" rx="4" fill="#b8bdc3" stroke="#7a8087"/>')
text(800, 845, "micro-USB", "silk-s", "middle")

TAGFILL = {"v33": "var(--v33)", "v5": "var(--v5)", "gnd": "var(--gnd)",
           "chaos": "var(--chaos)", "strip": "var(--strip)"}

def tag(x0, x1, y, label, kind):
    col = TAGFILL.get(kind)
    dash = ' stroke-dasharray="4 3"' if label in INPUT_ONLY else ""
    if col:
        add(f'<rect class="tag" x="{x0}" y="{y-12}" width="{x1-x0}" height="24" rx="12" fill="{col}" stroke="{col}"/>')
        add(f'<text class="tag-t" x="{(x0+x1)/2}" y="{y+4}" text-anchor="middle" style="fill:#fff">{label}</text>')
    else:
        add(f'<rect class="tag" x="{x0}" y="{y-12}" width="{x1-x0}" height="24" rx="12" fill="var(--bg)" stroke="var(--io)"{dash}/>')
        add(f'<text class="tag-t" x="{(x0+x1)/2}" y="{y+4}" text-anchor="middle" style="fill:var(--ink2)">{label}</text>')

for i, (silk, lab, kind) in enumerate(LEFT):
    y = py(i)
    line(TAGL1, y, PL, y, "w")
    add(f'<rect class="pin" x="{PL-6}" y="{y-6}" width="12" height="12" rx="2"/>')
    text(PL + 14, y + 4.5, silk, "silk")
    tag(TAGL0, TAGL1, y, lab, kind)
for i, (silk, lab, kind) in enumerate(RIGHT):
    y = py(i)
    line(PR, y, TAGR0, y, "w")
    add(f'<rect class="pin" x="{PR-6}" y="{y-6}" width="12" height="12" rx="2"/>')
    text(PR - 14, y + 4.5, silk, "silk", "end")
    tag(TAGR0, TAGR1, y, lab, kind)

# ── chaos button ───────────────────────────────────────────────
yc = py(9)
CX = 520
add(f'<path class="w" d="M{TAGL0},{yc} L{CX},{yc} L{CX},{yc+18}" style="stroke:var(--chaos)"/>')
add(f'<circle class="part" cx="{CX}" cy="{yc+20}" r="3"/>')
add(f'<circle class="part" cx="{CX}" cy="{yc+50}" r="3"/>')
line(CX - 13, yc + 14, CX - 13, yc + 56, "plate")
line(CX - 13, yc + 35, CX - 28, yc + 35, "w")
line(CX - 28, yc + 28, CX - 28, yc + 42, "w")
line(CX, yc + 53, CX, yc + 72, "w"); gnd(CX, yc + 72)
text(CX - 36, yc + 28, "CHAOS", "ttl", "end")
text(CX - 36, yc + 44, "momentary", "note", "end")
text(CX - 36, yc + 58, "to GND, no resistor", "note", "end")

# left GND and VIN
yg_ = py(13)
line(TAGL0, yg_, 548, yg_, "w"); line(548, yg_, 548, yg_ + 12, "w"); gnd(548, yg_ + 12)
text(538, yg_ + 4, "button", "note", "end")
yv = py(14)
add(f'<path class="v5" d="M{TAGL0},{yv} L{520},{yv}" style="fill:none"/>')
add(f'<path d="M520,{yv} l-12,-7 v14 z" fill="var(--v5)"/>')
text(500, yv + 4, "+5V: USB or strip supply", "note", "end")

# ── right side: strip, supply ──────────────────────────────────
ys = py(6)                     # D18
yd = 652                       # the strip's DIN pad
XD = 1230                      # data wire drops here, clear of the supply leads
SX0, SX1 = 1290, 1500
add(f'<path class="w" d="M{TAGR1},{ys} L{XD},{ys} L{XD},{yd} L{SX0},{yd}" style="stroke:var(--strip)"/>')
res_h(1110, ys, "330 Ω")
text(XD + 10, yd - 8, "DIN", "val")

# strip
add(f'<rect class="part" x="{SX0}" y="570" width="{SX1-SX0}" height="164" rx="6"/>')
for k, (y, lab) in enumerate([(600, "+5V"), (yd, "DIN"), (704, "GND")]):
    add(f'<rect x="{SX0-6}" y="{y-6}" width="12" height="12" rx="2" fill="#c9a74a" stroke="#7d6420"/>')
    text(SX0 + 14, y + 4, lab, "t")
for k in range(5):
    x = SX0 + 74 + k * 24
    add(f'<rect x="{x}" y="{640}" width="18" height="18" rx="2" fill="var(--bg)" stroke="var(--ink2)"/>')
    add(f'<circle cx="{x+9}" cy="649" r="4" fill="var(--strip)" opacity=".7"/>')
add(f'<path d="M{SX0+74},676 h112" stroke="var(--ink2)" stroke-width="1.5"/>')
add(f'<path d="M{SX0+186},676 l-8,-4 v8 z" fill="var(--ink2)"/>')
text(SX0 + 74, 696, "data direction", "note")
text(SX0 + 60, 596, "WS2812B STRIP", "ttl")
text(SX0 + 60, 612, "input end, 300 px", "note")

# supply, above the strip
add(f'<rect class="part" x="{SX0}" y="402" width="{SX1-SX0}" height="66" rx="6"/>')
text(SX0 + 16, 428, "5 V SUPPLY", "ttl")
text(SX0 + 16, 446, "~18 A at full white:", "note")
text(SX0 + 16, 460, "size it, or cap it in code", "note")
PP, PN = 1330, 1450
# + lead: down, left, down, into the +5V pad
add(f'<path class="v5" d="M{PP},468 L{PP},520 L1250,520 L1250,600 L{SX0},600" style="fill:none"/>')
text(PP + 6, 484, "+", "val")
dot(PP, 500)
# − lead: down, right, round the strip, back to the ground junction
add(f'<path class="w" d="M{PN},468 L{PN},520 L1530,520 L1530,830 L1200,830 L1200,{py(13)}"/>')
text(PN + 6, 484, "−", "val")
dot(PN, 500)
# reservoir cap across the supply leads
line(PP, 500, 1384, 500, "v5"); line(1396, 500, PN, 500, "w")
line(1384, 488, 1384, 512, "plate"); line(1396, 488, 1396, 512, "plate")
text(1376, 492, "+", "note", "end")
text(1390, 542, "1000 µF, 6.3 V+", "val", "middle")
text(1390, 556, "at the strip", "note", "middle")
# strip GND pad to the ESP32 GND pin, sharing ground
ygr = py(13)
add(f'<path class="w" d="M{SX0},704 L1200,704 L1200,{ygr} L{TAGR1},{ygr}"/>')
dot(1200, ygr)
text(1044, 690, "common ground:", "val")
text(1044, 704, "strip, supply and", "note")
text(1044, 718, "ESP32 must share GND", "note")

# why not D2
text(1044, 576, "Not D2: it also drives", "note")
text(1044, 590, "the blue on-board LED,", "note")
text(1044, 604, "which loads the data line.", "note")

# ── legend ─────────────────────────────────────────────────────
LY = 930
add(f'<line x1="30" y1="{LY-20}" x2="{W-30}" y2="{LY-20}" stroke="var(--rule)" stroke-width="1"/>')
items = [("var(--chaos)", "Chaos button: D27"),
         ("var(--strip)", "Strip data: D18"),
         ("var(--v5)", "5 V"), ("var(--gnd)", "Ground")]
x = 30
for col, lab in items:
    add(f'<rect x="{x}" y="{LY-10}" width="22" height="14" rx="7" fill="{col}"/>')
    text(x + 30, LY + 2, lab, "t")
    x += 40 + len(lab) * 7.3
add(f'<rect x="30" y="{LY+18}" width="22" height="14" rx="7" fill="var(--bg)" stroke="var(--io)" stroke-dasharray="4 3"/>')
text(60, LY + 30, "Input only (GPIO34–39): no output, no internal pull-up. Wrong for the button.", "t")
gnd(41, LY + 46)
text(60, LY + 58, "Every ground symbol joins a GND pin. Both GND pins on the board are the same net.", "t")
notes = [
    "No knobs: palette, speed and level are set from the computer's keyboard through the serial monitor, over the same USB cable.",
    "Strip data is 3.3 V into a 5 V strip. Short runs usually work. If the first pixel glitches, add a 74AHCT125 level shifter before the 330 Ω.",
    "EN resets the board. Hold BOOT while tapping EN to force the flasher, only if an upload will not start on its own.",
]
for k, n in enumerate(notes):
    text(30, LY + 88 + k * 18, n, "note")

add('</svg>')
open(sys.argv[1], "w").write("\n".join(out) + "\n")
