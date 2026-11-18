// A recording read second by second, for a mixer who cannot hear it. One line a window: how loud, how sharp its
// peak, where its weight sits, and whether the voice band is beating like speech. Used to choose which seconds of a
// long field recording to cut from.
//
//   node sound/tools/timeline.mjs <file> [--from s] [--to s] [--step 1] [--png out.png]
//
// --png also draws the stretch read (see draw.mjs for how to read the picture).

import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import { SR, toDb, lp, hp, fft } from '../kit/dsp.mjs';
import { decode, probe } from '../kit/source.mjs';

const a = process.argv.slice(2);
const opt = (k, d = null) => (a.includes(k) ? a[a.indexOf(k) + 1] : d);
const file = a[0];
if (!file) { console.error('usage: node sound/tools/timeline.mjs <file> [--from s] [--to s] [--step 1] [--png out.png]'); process.exit(1); }
const from = +opt('--from', 0), info = probe(file), to = +opt('--to', info.seconds), step = +opt('--step', Math.max(0.25, Math.round((to - from) / 60 * 4) / 4));
const x = decode(file, { from, to, channels: 1 });

const rms = (v, i0, i1) => { let s = 0; for (let i = i0; i < i1; i++) s += v[i] * v[i]; return Math.sqrt(s / Math.max(1, i1 - i0)); };
const low = lp(lp(Float32Array.from(x), 160, 0.6), 160, 0.7), voice = hp(lp(Float32Array.from(x), 3000), 300), top = hp(hp(Float32Array.from(x), 4000), 4000);
const N = 4096;
/** The frequency below which half the window's energy lies. */
function middle(i0) {
  const re = new Float32Array(N), im = new Float32Array(N);
  for (let i = 0; i < N; i++) re[i] = (x[i0 + i] || 0) * (0.5 - 0.5 * Math.cos(2 * Math.PI * i / N));
  fft(re, im);
  const p = new Float32Array(N / 2); let total = 0;
  for (let k = 1; k < N / 2; k++) { p[k] = re[k] * re[k] + im[k] * im[k]; total += p[k]; }
  let s = 0; for (let k = 1; k < N / 2; k++) { s += p[k]; if (s >= total / 2) return k * SR / N; }
  return 0;
}
console.log(`${file}\n  ${info.codec} ${info.rate} Hz ${info.channels} ch, ${info.seconds.toFixed(1)} s; read ${from} to ${to} s in steps of ${step} s`);
console.log('   from     level   peak  crest   low  voice   top  middle  beat');
const w = Math.round(step * SR);
for (let i0 = 0; i0 + w / 2 < x.length; i0 += w) {
  const i1 = Math.min(x.length, i0 + w), all = rms(x, i0, i1);
  let pk = 0; for (let i = i0; i < i1; i++) { const v = Math.abs(x[i]); if (v > pk) pk = v; }
  // How much the voice band's level swings at the pace of syllables (3 to 8 a second): speech swings a lot.
  const hop = Math.round(SR / 50), env = []; for (let i = i0; i + hop <= i1; i += hop) env.push(toDb(rms(voice, i, i + hop) + 1e-7));
  let swing = 0; for (let k = 3; k < env.length; k++) swing += Math.abs(env[k] - env[k - 3]); swing /= Math.max(1, env.length - 3);
  const rel = (v) => (toDb(rms(v, i0, i1) + 1e-9) - toDb(all + 1e-9)).toFixed(0).padStart(5);
  console.log(`${(from + i0 / SR).toFixed(2).padStart(8)}  ${toDb(all).toFixed(1).padStart(7)} ${toDb(pk).toFixed(1).padStart(6)} ${(toDb(pk) - toDb(all)).toFixed(0).padStart(6)} ${rel(low)} ${rel(voice)} ${rel(top)} ${String(Math.round(middle(Math.min(Math.max(0, x.length - N), i0 + Math.max(0, (w - N) >> 1))))).padStart(7)} ${swing.toFixed(1).padStart(5)}`);
}
console.log('  level, peak: dB. crest: peak over level (a shot is 15 or more, an engine 6 to 10). low, voice, top: dB against the whole (under 160 Hz, 300 to 3000 Hz, over 4 kHz). middle: Hz with half the energy below. beat: dB the voice band swings in 60 ms.');

const png = opt('--png');
if (png) {
  const here = path.dirname(fileURLToPath(import.meta.url));
  const r = spawnSync(process.execPath, [path.join(here, 'draw.mjs'), png, `${file}@${from}-${to}`], { encoding: 'utf8' });
  console.log(r.status === 0 ? `  ${r.stdout.trim()}` : `  could not draw: ${(r.stderr || '').trim()}`);
}
