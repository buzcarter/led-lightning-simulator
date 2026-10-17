#!/usr/bin/env node
/* Live dashboard for the bench.

   `pio device monitor` keeps the serial port and your terminal; its
   `dashboard` filter (monitor/filter_dashboard.py) copies each line here as a
   UDP datagram. This folds the lines into one picture of the board and streams
   them to any open page over Server-Sent Events.

     node server.js            listen for the monitor
     node server.js --demo     invent a board instead, for working on the page

   STORM_DASH_UDP (7777)  STORM_DASH_HTTP (7778)  STORM_DASH_BIND (127.0.0.1) */

import http from 'node:http';
import dgram from 'node:dgram';
import { readFile } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { readFirmware } from './firmware.js';
import { createState, applyLine } from './public/state.js';

const HERE     = dirname(fileURLToPath(import.meta.url));
const ROOT     = join(HERE, '..');
const UDP_PORT  = +(process.env.STORM_DASH_UDP  || 7777);
const HTTP_PORT = +(process.env.STORM_DASH_HTTP || 7778);
const BIND      = process.env.STORM_DASH_BIND || '127.0.0.1';
const DEMO      = process.argv.includes('--demo');

const firmware = readFirmware(ROOT);
const state = createState(firmware);
const clients = new Set();

function send(res, event, data) {
  res.write(`event: ${event}\ndata: ${JSON.stringify(data)}\n\n`);
}

function ingest(line) {
  const t = Date.now();
  if (!applyLine(state, line, t)) return;
  for (const res of clients) send(res, 'line', { t, line });
}

/* ── serial lines in ─────────────────────────────────────────── */
const udp = dgram.createSocket('udp4');
udp.on('message', buf => ingest(buf.toString('utf8')));
udp.on('error', err => {
  console.error(`udp ${UDP_PORT}: ${err.message}`);
  process.exit(1);
});
udp.bind(UDP_PORT, '127.0.0.1');

/* ── pages out ───────────────────────────────────────────────── */
const FILES = {
  '/':          [join(HERE, 'public/index.html'), 'text/html; charset=utf-8'],
  '/parse.js':  [join(HERE, 'public/parse.js'),   'text/javascript; charset=utf-8'],
  '/state.js':  [join(HERE, 'public/state.js'),   'text/javascript; charset=utf-8'],
  '/simulator': [join(ROOT, 'index.html'),        'text/html; charset=utf-8'],
};

const server = http.createServer(async (req, res) => {
  const path = new URL(req.url, 'http://x').pathname;

  if (path === '/events') {
    res.writeHead(200, {
      'Content-Type': 'text/event-stream',
      'Cache-Control': 'no-cache',
      Connection: 'keep-alive',
    });
    res.write('retry: 2000\n\n');
    send(res, 'snapshot', { now: Date.now(), state });
    clients.add(res);
    req.on('close', () => clients.delete(res));
    return;
  }
  if (path === '/state') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify(state));
    return;
  }
  const file = FILES[path];
  if (!file) { res.writeHead(404).end('not found'); return; }
  try {
    // read on every request, so editing the page needs only a reload
    const body = await readFile(file[0]);
    res.writeHead(200, { 'Content-Type': file[1], 'Cache-Control': 'no-store' });
    res.end(body);
  } catch {
    res.writeHead(404).end('not found');
  }
});

// proxies and browsers drop an idle stream; a comment line keeps it open
setInterval(() => { for (const res of clients) res.write(': ping\n\n'); }, 15000).unref();

server.on('error', err => {
  console.error(`http ${HTTP_PORT}: ${err.message}`);
  process.exit(1);
});
server.listen(HTTP_PORT, BIND, () => {
  console.log(`dashboard  http://${BIND === '0.0.0.0' ? 'localhost' : BIND}:${HTTP_PORT}/`);
  console.log(DEMO ? 'demo board running' : `listening   udp://127.0.0.1:${UDP_PORT}  (pio device monitor)`);
  const fw = Object.keys(firmware);
  console.log(`firmware    ${fw.length ? fw.join(', ') : 'nothing'} read from src/`);
});

if (DEMO) {
  const { fakeBench } = await import('./fake-bench.js');
  fakeBench(firmware, ingest);
}
