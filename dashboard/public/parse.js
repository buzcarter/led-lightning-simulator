/* Turns one line of the board's serial output into a typed record.
   Shared by the service (server.js) and the page, so both read the board the
   same way. The formats are the LOG() calls in src/storm.h and src/controls.h;
   change one there and change it here. Anything unrecognised is { type: 'text' }. */

// palette: tornado     speed: 60%         level: 40%        | strike: Bolt_Left +1     | fps:  98  mA: 3180 | raw:  123/2045/4095
const STATUS = new RegExp(
  'palette:\\s+(.+?)\\s+speed:\\s+(\\d+)%(?:\\s+\\((min|max)\\))?' +
  '\\s+level:\\s+(?:(CHAOS)|(\\d+)%(?:\\s+\\((min|max)\\))?)' +
  '\\s*\\|\\s*strike:\\s+(\\S+)(?:\\s+\\+(\\d+))?' +
  '\\s*\\|\\s*fps:\\s*(\\d+)\\s+mA:\\s*(\\d+)' +
  '(?:\\s*\\|\\s*raw:\\s*(\\d+)\\/\\s*(\\d+)\\/\\s*(\\d+))?'
);
const BOOT    = /StormCloud ready\D+(\d+) px, (\d+) segments, pin (\d+), master (\d+)/;
const SEGMENT = /^\[storm\]\s+(\S+)\s+(\d+)\s+(\d+)\s+(cloud|bolt|fork)\s+(.*)$/;
const FORK    = /off (\S+) at (\d+)%/;
const PALETTE = /\[storm\] palette .*cool #([0-9A-F]{6}) hot #([0-9A-F]{6})/i;
const DROPPED = /\[storm\] DROPPED (\S+) .*?(\d+) strike slots/;
const POTS    = /\[pots\]\s+(\d+) palettes, (\d+) speeds, (\d+) levels/;

const knob = (pct, end) => ({ pct: +pct, min: end === 'min', max: end === 'max' });

export function parseLine(raw) {
  // tolerate a `time` monitor filter or stray control codes ahead of the text
  const line = raw.replace(/\x1b\[[0-9;]*[A-Za-z]/g, '').trimEnd();
  let m;

  if ((m = STATUS.exec(line))) {
    return {
      type: 'status',
      palette: m[1].trim(),
      speed: knob(m[2], m[3]),
      level: m[4] ? { chaos: true, pct: 100, min: false, max: false } : { chaos: false, ...knob(m[5], m[6]) },
      strike: m[7] === '_' ? null : { name: m[7], overlap: m[8] ? +m[8] : 0 },
      fps: +m[9],
      mA: +m[10],
      raw: m[11] != null ? [+m[11], +m[12], +m[13]] : null,
    };
  }
  if ((m = BOOT.exec(line)))    return { type: 'boot', px: +m[1], segments: +m[2], pin: +m[3], master: +m[4] };
  if ((m = DROPPED.exec(line))) return { type: 'dropped', name: m[1], slots: +m[2] };
  if ((m = PALETTE.exec(line))) return { type: 'bootPalette', cool: '#' + m[1], hot: '#' + m[2] };
  if ((m = POTS.exec(line)))    return { type: 'pots', palettes: +m[1], speeds: +m[2], levels: +m[3] };
  if ((m = SEGMENT.exec(line))) {
    const f = m[4] === 'fork' ? FORK.exec(m[5]) : null;
    return {
      type: 'segment', name: m[1], start: +m[2], len: +m[3], role: m[4],
      parent: f ? f[1] : null, forkAt: f ? +f[2] : null,
    };
  }
  return { type: 'text' };
}
