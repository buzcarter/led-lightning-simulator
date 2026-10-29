#!/usr/bin/env node
/* A pretend board: prints what src/storm.h would, at about the rate it would.

     node server.js --demo    runs it inside the service
     node fake-bench.js       sends it over UDP, the way the monitor filter does,
                              so the whole path can be checked without hardware */

import dgram from 'node:dgram';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { readFirmware } from './firmware.js';

const pad = (s, n) => String(s).padEnd(n);
const num = (v, n) => String(v).padStart(n);
const pick = arr => arr[Math.floor(Math.random() * arr.length)];

const STEPS = 10;
function knob(idx) {
  const pct = Math.floor((idx + 1) * 100 / STEPS);
  return `${pct}%${idx === 0 ? ' (min)' : idx === STEPS - 1 ? ' (max)' : ''}`;
}

export function fakeBench(fw, emit) {
  const segs = fw.segments || [{ name: 'Cloud_Body', start: 0, len: 132, role: 'cloud' }, { name: 'Bolt_Left', start: 132, len: 38, role: 'bolt' }];
  const pals = (fw.palettes || [{ name: 'storm' }]).map(p => p.name);
  const roots = segs.filter(s => s.role === 'bolt').map(s => s.name);

  const k = { palette: 1, speed: 5, level: 3 };
  let strikes = [];           // { name, until }
  let chaosUntil = 0;
  let last = '', lastAt = 0;
  let peak = 0;

  emit('');
  emit(`[storm] StormCloud ready — ${fw.numLeds || 300} px, ${segs.length} segments, pin 18, master 170`);
  emit(`[storm] ${pad('segment', 16)} ${num('start', 5)} ${num('len', 5)} ${num('role', 6)}  attachment`);
  for (const s of segs) {
    const tail = s.role === 'fork' ? `off ${s.parent} at ${s.forkAt}%` : s.role === 'bolt' ? 'root channel' : '-';
    emit(`[storm] ${pad(s.name, 16)} ${num(s.start, 5)} ${num(s.len, 5)} ${num(s.role, 6)}  ${tail}`);
  }
  emit('[storm] palette hue=80/64 sat=165/30 -> cool #3D5A0F hot #E6F0C8');
  emit('[storm] frame buffer 900 B, strike pool 96 B, segment table 196 B');
  emit('[keys]  p s l pick palette, speed or level; + - step it; h hold, then 0-6');
  emit('[storm] chaos button GPIO27 to GND');
  emit('');

  setInterval(() => {
    const now = Date.now();
    const chaos = now < chaosUntil;
    strikes = strikes.filter(s => s.until > now);

    // storms come in bursts; speed and chaos shorten the gaps
    const rate = (chaos ? 0.5 : 0.04) * (0.4 + k.speed / 6);
    if (roots.length && Math.random() < rate) {
      if (strikes.length >= 4 && Math.random() < 0.05) {
        emit(`[storm] DROPPED ${pick(roots)} — all 4 strike slots busy`);
      } else {
        strikes.push({ name: pick(roots), until: now + 250 + Math.random() * 600 });
      }
    }
    // someone at the keyboard, now and then
    if (Math.random() < 0.004) {
      const which = pick(['palette', 'speed', 'level']);
      const n = which === 'palette' ? pals.length : STEPS;
      k[which] = (k[which] + (Math.random() < 0.5 ? n - 1 : 1)) % n;
      emit(`[keys]  ${which} ${k[which] + 1}/${n}`);
    }
    if (Math.random() < 0.0015) chaosUntil = now + 4500;

    const mA = strikes.length ? 1400 + strikes.length * 1100 + Math.random() * 600 : 180 + Math.random() * 90;
    peak = Math.max(peak, mA);

    const newest = strikes[strikes.length - 1];
    const chan = newest ? newest.name + (strikes.length > 1 ? ` +${strikes.length - 1}` : '') : '_';
    const line = `palette: ${pad(pals[k.palette], 10)}  speed: ${pad(knob(k.speed), 10)}  ` +
                 `level: ${pad(chaos ? 'CHAOS' : knob(k.level), 10)} | strike: ${pad(chan, 16)}`;

    if ((line !== last && now - lastAt >= 120) || now - lastAt >= 750) {
      const fps = Math.round((strikes.length ? 92 : 99) - Math.random() * 4);
      emit(`${line} | fps: ${num(fps, 3)}  mA: ${num(Math.round(peak), 4)}`);
      last = line; lastAt = now; peak = 0;
    }
  }, 30);
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const port = +(process.env.STORM_DASH_UDP || 7777);
  const sock = dgram.createSocket('udp4');
  const root = join(dirname(fileURLToPath(import.meta.url)), '..');
  fakeBench(readFirmware(root), line => {
    console.log(line);
    sock.send(Buffer.from(line), port, '127.0.0.1');
  });
}
