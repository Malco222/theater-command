// Checks made sounds the way a small speaker and a loop player will meet them.
//
//   node sound/tools/loops.mjs <folder of made sounds> [name ...]
//
// For every loop: the jump between its last and first sample against the largest jump within 5 ms either side of the
// join (over about 1.5 a click may be heard), and any hole: a stretch of 0.3 s more than 20 dB under the loop's middle
// level. For every sound: how much quieter it gets through a laptop speaker (a 150 Hz high-pass; over 6 dB it fades
// away on one), and its peak.

import fs from 'node:fs';
import path from 'node:path';
import { SR, isStereo, toDb, hp4, loudness, peak, readWav } from '../kit/dsp.mjs';

const [dir, ...want] = process.argv.slice(2);
const list = JSON.parse(fs.readFileSync(path.join(dir, 'sound-list.json'), 'utf8')).filter((s) => !want.length || want.includes(s.name));
const chans = (x) => (isStereo(x) ? [x.L, x.R] : [x]);
const rows = [];
for (const s of list) {
  const x = readWav(path.join(dir, s.name + '.wav'));
  const lap = loudness(x)[s.loop ? 'integrated' : 'moment'] - loudness(isStereo(x) ? { L: hp4(x.L, 150), R: hp4(x.R, 150) } : hp4(x, 150))[s.loop ? 'integrated' : 'moment'];
  const row = { name: s.name, loop: s.loop, peakDb: +toDb(peak(x)).toFixed(1), laptopLossDb: +lap.toFixed(1) };
  if (s.loop) {
    let worst = 0;
    for (const c of chans(x)) {
      const n = c.length, w = Math.round(SR * 0.005);
      const join = Math.abs(c[0] - c[n - 1]);
      let near = 1e-9; for (let i = 1; i < w; i++) near = Math.max(near, Math.abs(c[i] - c[i - 1]), Math.abs(c[n - i] - c[n - i - 1]));
      worst = Math.max(worst, join / near);
    }
    const m = chans(x)[0], f = Math.round(SR * 0.05), lv = [];
    for (let i = 0; i + f <= m.length; i += f) { let e = 0; for (let k = 0; k < f; k++) e += m[i + k] ** 2; lv.push(toDb(Math.sqrt(e / f) + 1e-9)); }
    const mid = [...lv].sort((a, b) => a - b)[lv.length >> 1], holes = [];
    for (let i = 0; i + 6 <= lv.length; i++) if (lv.slice(i, i + 6).every((v) => v < mid - 20)) { holes.push(+(i * 0.05).toFixed(2)); i += 6; }
    Object.assign(row, { joinStep: +worst.toFixed(2), holes });
  }
  rows.push(row);
  console.log(`${s.name.padEnd(26)} peak ${String(row.peakDb).padStart(5)} dB  laptop -${String(row.laptopLossDb).padStart(4)} dB${s.loop ? `  join ${row.joinStep}x${row.joinStep > 1.5 ? ' CLICK' : ''}${row.holes.length ? '  holes at ' + row.holes.join(', ') + ' s' : ''}` : ''}${row.laptopLossDb > 6 ? '  FADES ON A LAPTOP' : ''}`);
}
