/* Turns one line of the board's serial output into a typed record.
   Shared by the service (server.js) and the page, so both read the board the
   same way. The formats are the LOG() calls in src/storm.h and src/controls.h;
   change one there and change it here. Anything unrecognised is { type: 'text' }. */

// palette: tornado     speed: 60%         cloud: 40%         level: 40%        | strike: Bolt_Left +1     | fps:  98  mA: 3180
// cloud is optional, so lines from firmware without the cloud speed still read
const STATUS = new RegExp(
  'palette:\\s+(?<palette>.+?)\\s+speed:\\s+(?<speed>\\d+)%(?:\\s+\\((?<speedEnd>min|max)\\))?' +
  '(?:\\s+cloud:\\s+(?<cloud>\\d+)%(?:\\s+\\((?<cloudEnd>min|max)\\))?)?' +
  '\\s+level:\\s+(?:(?<chaos>CHAOS)|(?<level>\\d+)%(?:\\s+\\((?<levelEnd>min|max)\\))?)' +
  '\\s*\\|\\s*strike:\\s+(?<strike>\\S+)(?:\\s+\\+(?<overlap>\\d+))?' +
  '\\s*\\|\\s*fps:\\s*(?<fps>\\d+)\\s+mA:\\s*(?<mA>\\d+)'
);
const BOOT    = /StormCloud ready\D+(\d+) px, (\d+) segments, pin (\d+), master (\d+)/;
const SEGMENT = /^\[storm\]\s+(\S+)\s+(\d+)\s+(\d+)\s+(cloud|bolt|fork)\s+(.*)$/;
const FORK    = /off (\S+) at (\d+)%/;
const PALETTE = /\[storm\] palette .*cool #([0-9A-F]{6}) hot #([0-9A-F]{6})/i;
const DROPPED = /\[storm\] DROPPED (\S+) .*?(\d+) strike slots/;

const knob = (pct, end) => ({ pct: +pct, min: end === 'min', max: end === 'max' });

export function parseLine(raw) {
  // tolerate a `time` monitor filter or stray control codes ahead of the text
  const line = raw.replace(/\x1b\[[0-9;]*[A-Za-z]/g, '').trimEnd();
  let m;

  if ((m = STATUS.exec(line))) {
    const g = m.groups;
    return {
      type: 'status',
      palette: g.palette.trim(),
      speed: knob(g.speed, g.speedEnd),
      cloud: g.cloud != null ? knob(g.cloud, g.cloudEnd) : null,
      level: g.chaos ? { chaos: true, pct: 100, min: false, max: false } : { chaos: false, ...knob(g.level, g.levelEnd) },
      strike: g.strike === '_' ? null : { name: g.strike, overlap: g.overlap ? +g.overlap : 0 },
      fps: +g.fps,
      mA: +g.mA,
    };
  }
  if ((m = BOOT.exec(line)))    return { type: 'boot', px: +m[1], segments: +m[2], pin: +m[3], master: +m[4] };
  if ((m = DROPPED.exec(line))) return { type: 'dropped', name: m[1], slots: +m[2] };
  if ((m = PALETTE.exec(line))) return { type: 'bootPalette', cool: '#' + m[1], hot: '#' + m[2] };
  if ((m = SEGMENT.exec(line))) {
    const f = m[4] === 'fork' ? FORK.exec(m[5]) : null;
    return {
      type: 'segment', name: m[1], start: +m[2], len: +m[3], role: m[4],
      parent: f ? f[1] : null, forkAt: f ? +f[2] : null,
    };
  }
  return { type: 'text' };
}
