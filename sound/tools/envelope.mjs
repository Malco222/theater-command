// The level of a recording read by eye: its peak, how many samples sit at the peak (a clipped microphone leaves
// many), the kurtosis of its loudest fifth (near 1.9 clipped throughout; 3 or more clean; a pure whistle or siren
// also reads low, so read it with the picture), the onsets (where the level leaps), and the loudest level in each
// step of time.
//
//   node sound/tools/envelope.mjs <file> [--from s] [--to s] [--step 0.1] [--rise 9]
import { SR, toDb } from '../kit/dsp.mjs';
import { decode, probe } from '../kit/source.mjs';
const a = process.argv.slice(2); const opt = (k, d) => (a.includes(k) ? +a[a.indexOf(k) + 1] : d);
const file = a[0], info = probe(file), from = opt('--from', 0), to = opt('--to', info.seconds), step = opt('--step', 0.1), rise = opt('--rise', 9);
const x = decode(file, { from, to, channels: 1 });
let pk = 0; for (const v of x) pk = Math.max(pk, Math.abs(v));
const hop = Math.round(SR * 0.005), lv = [];
for (let i = 0; i + hop <= x.length; i += hop) { let s = 0; for (let k = 0; k < hop; k++) s += x[i + k] ** 2; lv.push(toDb(Math.sqrt(s / hop) + 1e-9)); }
const on = [];
for (let i = 4; i < lv.length; i++) { let b = -200; for (let k = Math.max(0, i - 12); k < i - 1; k++) b = Math.max(b, lv[k]); if (lv[i] - b >= rise && lv[i] > toDb(pk) - 40) { on.push(+(from + i * 0.005).toFixed(3) + '(' + lv[i].toFixed(0) + ')'); i += 16; } }
// kurtosis over the loudest 20% of 50 ms frames
const fr = Math.round(SR * 0.05), frames = [];
for (let i = 0; i + fr <= x.length; i += fr) { let s = 0; for (let k = 0; k < fr; k++) s += x[i + k] ** 2; frames.push([s, i]); }
frames.sort((p, q) => q[0] - p[0]); let m2 = 0, m4 = 0, n = 0;
for (const [, i] of frames.slice(0, Math.max(1, Math.round(frames.length * 0.2)))) for (let k = 0; k < fr; k++) { const v = x[i + k]; m2 += v * v; m4 += v ** 4; n++; }
const kurt = (m4 / n) / ((m2 / n) ** 2);
let hot = 0; for (const v of x) if (Math.abs(v) > pk * 0.98) hot++;
const env = []; const st = Math.round(step / 0.005);
for (let i = 0; i < lv.length; i += st) { let m = -200; for (let k = i; k < Math.min(lv.length, i + st); k++) m = Math.max(m, lv[k]); env.push(m.toFixed(0)); }
console.log(`${file.split('/').pop()} ${from}-${(+to).toFixed(2)} s  peak ${toDb(pk).toFixed(1)} dB  near-peak samples ${hot}  kurtosis(loud) ${kurt.toFixed(2)}`);
console.log('  onsets: ' + on.slice(0, 40).join(' '));
console.log(`  env/${step}s: ` + env.join(' '));
