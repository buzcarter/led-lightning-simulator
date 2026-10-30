import { test } from 'node:test';
import assert from 'node:assert/strict';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { parseLine } from '../public/parse.js';
import { createState, applyLine } from '../public/state.js';
import { readFirmware } from '../firmware.js';

test('status line, quiet sky', () => {
  const p = parseLine('palette: tornado     speed: 60%         level: 40%        | strike: _                | fps:  99  mA:  240');
  assert.equal(p.type, 'status');
  assert.equal(p.palette, 'tornado');
  assert.deepEqual(p.speed, { pct: 60, min: false, max: false });
  assert.equal(p.level.chaos, false);
  assert.equal(p.strike, null);
  assert.equal(p.fps, 99);
  assert.equal(p.mA, 240);
  assert.equal(p.cloud, null);             // older firmware, no cloud field
});

test('status line with a space in the palette, overlap and ends', () => {
  const p = parseLine('palette: red planet  speed: 10% (min)   level: 100% (max) | strike: Bolt_Centre +1   | fps:  96  mA: 5110');
  assert.equal(p.palette, 'red planet');
  assert.deepEqual(p.speed, { pct: 10, min: true, max: false });
  assert.deepEqual(p.level, { chaos: false, pct: 100, min: false, max: true });
  assert.deepEqual(p.strike, { name: 'Bolt_Centre', overlap: 1 });
});

test('status line with the cloud speed', () => {
  const p = parseLine('palette: storm       speed: 60%         cloud: 10% (min)   level: 40%        | strike: _                | fps:  99  mA:  240');
  assert.deepEqual(p.speed, { pct: 60, min: false, max: false });
  assert.deepEqual(p.cloud, { pct: 10, min: true, max: false });
  assert.deepEqual(p.level, { chaos: false, pct: 40, min: false, max: false });
});

test('chaos, behind a time filter prefix', () => {
  const p = parseLine('12:01:02.345 > palette: storm       speed: 60%         level: CHAOS      | strike: Bolt_Left        | fps:  98  mA: 3180');
  assert.equal(p.level.chaos, true);
  assert.equal(p.strike.name, 'Bolt_Left');
});

test('boot banner, segment rows and faults', () => {
  assert.deepEqual(parseLine('[storm] StormCloud ready — 300 px, 7 segments, pin 5, master 170'),
    { type: 'boot', px: 300, segments: 7, pin: 5, master: 170 });
  assert.equal(parseLine('[storm] segment          start   len   role  attachment').type, 'text');
  assert.deepEqual(parseLine('[storm] Bolt_Left_Fork     170    18   fork  off Bolt_Left at 54%'),
    { type: 'segment', name: 'Bolt_Left_Fork', start: 170, len: 18, role: 'fork', parent: 'Bolt_Left', forkAt: 54 });
  assert.equal(parseLine('[storm] Cloud_Body           0   132  cloud  -').role, 'cloud');
  assert.deepEqual(parseLine('[storm] DROPPED Bolt_Right — all 4 strike slots busy'),
    { type: 'dropped', name: 'Bolt_Right', slots: 4 });
  assert.equal(parseLine('[storm] palette hue=80/64 sat=165/30 -> cool #3D5A0F hot #E6F0C8').hot, '#E6F0C8');
});

test('state counts strikes on a change of channel, not on every refresh', () => {
  const s = createState();
  const line = (strike) => `palette: storm       speed: 60%         level: 40%        | strike: ${strike.padEnd(16)} | fps:  98  mA: 3180`;
  applyLine(s, line('Bolt_Left'), 1);
  applyLine(s, line('Bolt_Left'), 2);      // slow-timer refresh, same strike
  applyLine(s, line('Bolt_Left +1'), 3);   // a second one joins
  applyLine(s, line('_'), 4);
  applyLine(s, line('Bolt_Left'), 5);
  assert.equal(s.strikes.length, 3);
  assert.equal(s.strikeCounts.Bolt_Left, 3);
  assert.equal(s.history.length, 5);
});

test('boot banner replaces the segment table once all rows arrive', () => {
  const s = createState({ segments: [{ name: 'old', start: 0, len: 1, role: 'cloud' }] });
  applyLine(s, '[storm] StormCloud ready — 50 px, 2 segments, pin 5, master 170', 1);
  applyLine(s, '[storm] Cloud_Body           0    30  cloud  -', 2);
  assert.equal(s.segments[0].name, 'old');
  applyLine(s, '[storm] Bolt_Left           30    20   bolt  root channel', 3);
  assert.equal(s.segmentsFrom, 'board');
  assert.deepEqual(s.segments.map(x => x.name), ['Cloud_Body', 'Bolt_Left']);
});

test('firmware source reads back the real tables', () => {
  const fw = readFirmware(join(dirname(fileURLToPath(import.meta.url)), '../..'));
  assert.ok(fw.numLeds > 0);
  assert.ok(fw.segments.length > 0);
  assert.ok(fw.segments.some(s => s.role === 'fork' && s.parent));
  assert.ok(fw.palettes.length > 0);
  assert.equal(fw.steps.level, 10);
  assert.equal(fw.steps.speed.length, 10);
  assert.equal(fw.steps.speed[5], 1);          // step 6 is exactly 1.00x
});
