/* Reads what the dashboard can't learn from the serial line out of the
   firmware source: the segment table (the board only prints it at boot, which
   you usually miss), the palette colours, and the knob cutoffs. Each piece is
   optional; a file that has moved or changed shape just leaves that piece out. */

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

function read(file) {
  try { return readFileSync(file, 'utf8'); } catch { return null; }
}

function segments(src) {
  const body = /SEGMENTS\[[^\]]*\][^=]*=\s*\{([\s\S]*?)\};/.exec(src);
  if (!body) return null;
  const rows = [...body[1].matchAll(/\{\s*"([^"]+)"\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*\d+\s*,\s*\d+\s*,\s*(-?\d+)\s*,\s*(\d+)\s*\}/g)];
  const raw = rows.map(m => ({ name: m[1], start: +m[2], len: +m[3], isBolt: +m[4], parent: +m[5], forkAt: +m[6] }));
  return raw.map(r => ({
    name: r.name, start: r.start, len: r.len,
    role: r.parent >= 0 ? 'fork' : r.isBolt ? 'bolt' : 'cloud',
    parent: r.parent >= 0 ? raw[r.parent]?.name ?? null : null,
    forkAt: r.parent >= 0 ? Math.floor(r.forkAt * 100 / 255) : null,
  }));
}

function palettes(src) {
  const body = /PALETTES\[\]\s*=\s*\{([\s\S]*?)\};/.exec(src);
  if (!body) return null;
  return [...body[1].matchAll(/\{\s*"([^"]+)"\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}/g)]
    .map(m => ({ name: m[1], hue: +m[2], hotHue: +m[3], sat: +m[4], hotSat: +m[5], trim: +m[6] }));
}

function bytes(src, name) {
  const body = new RegExp(`${name}\\[[^\\]]*\\][^=]*=\\s*\\{([^}]*)\\}`).exec(src);
  return body ? body[1].split(',').map(s => s.trim()).filter(Boolean).map(Number) : null;
}

// the ESP32 profile comes first in DetentProfiles.h, and that is the board in platformio.ini
function detents(src) {
  const cut = /DETENT_10_CUTOFFS\[\]\s*=\s*\{([^}]*)\}/.exec(src);
  const hys = /DETENT_10\s*=\s*\{\s*\d+\s*,\s*DETENT_10_CUTOFFS\s*,\s*(\d+)\s*\}/.exec(src);
  if (!cut) return null;
  return { cutoffs: cut[1].split(',').map(Number), hysteresis: hys ? +hys[1] : 0, adcMax: 4095 };
}

export function readFirmware(root) {
  const layout = read(join(root, 'src/layout.h')) || '';
  const config = read(join(root, 'src/config.h')) || '';
  const detent = read(join(root, 'lib/DetentSwitch/DetentProfiles.h')) || '';
  const leds = /#define\s+NUM_LEDS\s+(\d+)/.exec(layout);
  const x = bytes(layout, 'LED_X'), y = bytes(layout, 'LED_Y');
  const fw = {
    numLeds: leds ? +leds[1] : null,
    // pixel positions, 0..255 on the long axis, so the page can draw the sky
    pixels: x && y && x.length === y.length ? x.map((v, i) => [v, y[i]]) : null,
    segments: segments(layout),
    palettes: palettes(config),
    detents: detents(detent),
  };
  for (const k of Object.keys(fw)) if (fw[k] == null) delete fw[k];
  return fw;
}
