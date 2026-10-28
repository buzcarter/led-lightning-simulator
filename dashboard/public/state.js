/* The dashboard's picture of the board, folded together one line at a time.
   The service keeps one so a page opened late gets the history; the page keeps
   its own copy, seeded from that snapshot, and applies each line as it arrives.
   Plain data throughout, so the snapshot is just JSON. */

import { parseLine } from './parse.js';

const HISTORY = 900;   // fps/mA samples: 15 min at one line a second
const STRIKES = 60;
const LOG     = 300;
const DROPS   = 30;

const push = (arr, item, cap) => { arr.push(item); if (arr.length > cap) arr.splice(0, arr.length - cap); };

/* firmware: what server.js read out of src/ — the segment table, palettes and
   knob cutoffs as built, used until (unless) the board's boot banner says otherwise. */
export function createState(firmware = {}) {
  return {
    firmware,
    segments: firmware.segments || [],
    segmentsFrom: firmware.segments ? 'source' : null,
    board: null,          // from the boot banner, if we saw one
    bootPalette: null,
    pending: null,        // segment rows still arriving after a boot banner
    status: null,
    statusAt: 0,
    lastLineAt: 0,
    lines: 0,
    history: [],          // { t, fps, mA }
    strikes: [],          // { t, name, overlap }
    strikeCounts: {},
    dropped: [],
    droppedTotal: 0,
    log: [],              // { t, line, type }
  };
}

export function applyLine(s, line, t) {
  if (!line.trim()) return null;
  const p = parseLine(line);
  s.lines++;
  s.lastLineAt = t;
  push(s.log, { t, line, type: p.type }, LOG);

  switch (p.type) {
    case 'status': {
      const prev = s.status;
      // a strike shows as a change of root channel, or a new overlap count
      if (p.strike && (!prev || !prev.strike || prev.strike.name !== p.strike.name ||
                       p.strike.overlap > prev.strike.overlap)) {
        push(s.strikes, { t, name: p.strike.name, overlap: p.strike.overlap }, STRIKES);
        s.strikeCounts[p.strike.name] = (s.strikeCounts[p.strike.name] || 0) + 1;
      }
      s.status = p;
      s.statusAt = t;
      push(s.history, { t, fps: p.fps, mA: p.mA }, HISTORY);
      break;
    }
    case 'boot':
      s.board = { ...p, bootAt: t };
      s.pending = [];
      s.strikes = []; s.strikeCounts = {}; s.status = null;
      break;
    case 'segment':
      if (!s.pending) break;     // a stray row without its banner: don't trust it
      s.pending.push({ name: p.name, start: p.start, len: p.len, role: p.role, parent: p.parent, forkAt: p.forkAt });
      if (s.board && s.pending.length >= s.board.segments) {
        s.segments = s.pending; s.segmentsFrom = 'board'; s.pending = null;
      }
      break;
    case 'bootPalette':
      s.bootPalette = { cool: p.cool, hot: p.hot };
      break;
    case 'dropped':
      s.droppedTotal++;
      push(s.dropped, { t, name: p.name, slots: p.slots }, DROPS);
      break;
  }
  return p;
}
