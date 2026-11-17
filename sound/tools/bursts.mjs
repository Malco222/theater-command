// Finds the separate bursts of sound in a stretch of a recording: shots, bursts of fire, explosions. For each it
// gives where it starts and ends, how loud it is, whether its peak sits hard against the top (an overloaded
// microphone), and how much of the voice band is voiced just before, during and after it (orders shouted on the
// range, which must not be cut into a sound).
//
//   node sound/tools/bursts.mjs <file> [--from s] [--to s] [--rise 12] [--quiet 18]
//
// A burst starts where the level leaps by --rise dB above the last fifth of a second, and ends when it has stayed
// --quiet dB under its own peak for a quarter of a second.

import { SR, toDb, hp, lp } from '../kit/dsp.mjs';
import { decode, probe } from '../kit/source.mjs';

const a = process.argv.slice(2);
const opt = (k, d) => (a.includes(k) ? +a[a.indexOf(k) + 1] : d);
const file = a[0];
if (!file) { console.error('usage: node sound/tools/bursts.mjs <file> [--from s] [--to s] [--rise 12] [--quiet 18]'); process.exit(1); }
const info = probe(file), from = opt('--from', 0), to = opt('--to', info.seconds), RISE = opt('--rise', 12), QUIET = opt('--quiet', 18);
const x = decode(file, { from, to, channels: 1 });
const hop = Math.round(SR * 0.005), lvl = [];
for (let i = 0; i + hop <= x.length; i += hop) { let s = 0; for (let k = 0; k < hop; k++) s += x[i + k] * x[i + k]; lvl.push(toDb(Math.sqrt(s / hop) + 1e-9)); }
const voice = hp(lp(Float32Array.from(x), 3000), 300);
/**
 * The share of a stretch that is voiced: a voice (speaking, shouting, singing) has a pitch, its waveform repeating
 * every 3 to 12 ms, where a shot, an engine's roar or wind does not. Each 40 ms frame loud enough to matter is tested
 * for that repetition in the voice band; the answer is the share of such frames that have it (0.3 and over: a voice
 * is very likely there). An engine with a steady note can also score, so read it with the picture.
 */
const voicedFrame = (i0, n) => {
  let e = 0; for (let k = 0; k < n; k++) e += voice[i0 + k] * voice[i0 + k];
  if (e / n < 1e-6) return null;
  let best = 0;
  for (let lag = Math.round(SR / 330); lag <= Math.round(SR / 85); lag += 2) {
    let c = 0, e1 = 0, e2 = 0;
    for (let k = 0; k + lag < n; k += 2) { c += voice[i0 + k] * voice[i0 + k + lag]; e1 += voice[i0 + k] ** 2; e2 += voice[i0 + k + lag] ** 2; }
    const r = c / Math.sqrt(e1 * e2 + 1e-12); if (r > best) best = r;
  }
  return best;
};
function speech(t0, t1) {
  const n = Math.round(SR * 0.04), i0 = Math.max(0, Math.round(t0 * SR)), i1 = Math.min(voice.length - n, Math.round(t1 * SR));
  let frames = 0, voiced = 0;
  for (let i = i0; i < i1; i += n) { const r = voicedFrame(i, n); if (r == null) continue; frames++; if (r > 0.75) voiced++; }
  return frames < 3 ? 0 : voiced / frames;
}
const out = [];
const back = Math.round(0.2 / 0.005), hold = Math.round(0.25 / 0.005);
for (let i = back; i < lvl.length; i++) {
  let before = -200; for (let k = i - back; k < i - 2; k++) before = Math.max(before, lvl[k]);
  if (lvl[i] - before < RISE) continue;
  let top = lvl[i], j = i, under = 0;
  while (j < lvl.length - 1 && under < hold) { j++; if (lvl[j] > top) { top = lvl[j]; under = 0; } else if (lvl[j] < top - QUIET) under++; else under = 0; }
  const s = i * 0.005, e = (j - hold) * 0.005;
  let pk = 0, hot = 0; for (let k = Math.round(s * SR); k < Math.round(e * SR); k++) { const v = Math.abs(x[k]); if (v > pk) pk = v; }
  for (let k = Math.round(s * SR); k < Math.round(e * SR); k++) if (Math.abs(x[k]) > pk * 0.97) hot++;
  out.push({ at: +(from + s).toFixed(3), end: +(from + e).toFixed(3), len: +(e - s).toFixed(2), top: +top.toFixed(1), peakDb: +toDb(pk).toFixed(1), flat: hot, talkBefore: +speech(s - 1.2, s - 0.05).toFixed(2), talkIn: +speech(s + 0.05, e).toFixed(2), talkAfter: +speech(e, e + 1.2).toFixed(2) });
  i = j;
}
console.log(`${file}: ${out.length} bursts between ${from} and ${+to.toFixed(1)} s`);
console.log('      at     end    len   level   peak  flat  voiced share before/in/after (0.3+ a voice is likely)');
for (const b of out) console.log(`${String(b.at).padStart(8)} ${String(b.end).padStart(7)} ${String(b.len).padStart(6)} ${String(b.top).padStart(7)} ${String(b.peakDb).padStart(6)} ${String(b.flat).padStart(5)}   ${b.talkBefore} / ${b.talkIn} / ${b.talkAfter}`);
