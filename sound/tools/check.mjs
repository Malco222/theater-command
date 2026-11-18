// Looks every made sound over for the faults a meter can find, since a meter is all this workshop has for ears.
//
//   node sound/tools/check.mjs <folder with the WAVs and sound-list.json>
//
// It checks: peaks too near the top; a click at the start or a cut-off at the end of a one-shot; a jump at the join
// of a loop; a loop that swells and dies so much that the ear would hear it come round; rumble below what a
// speaker can use; and a lean to one side. It prints one line a fault and ends with a count.

import fs from 'node:fs';
import path from 'node:path';
import { SR, sec, toDb, isStereo, toMono, lengthOf, peak, loudness, lp4, readWav } from '../kit/dsp.mjs';

const dir = process.argv[2];
const list = JSON.parse(fs.readFileSync(path.join(dir, 'sound-list.json'), 'utf8'));
const rms = (x, a = 0, b = x.length) => { let s = 0; for (let i = a; i < b; i++) s += x[i] * x[i]; return Math.sqrt(s / Math.max(1, b - a)); };
let faults = 0;
const say = (name, what) => { faults++; console.log(`${name.padEnd(24)} ${what}`); };

for (const s of list) {
  const x = readWav(path.join(dir, s.name + '.wav'));
  const m = toMono(isStereo(x) ? { L: Float32Array.from(x.L), R: Float32Array.from(x.R) } : Float32Array.from(x));
  const n = m.length, pk = toDb(peak(x)), all = rms(m);
  if (pk > -0.9) say(s.name, `peak ${pk.toFixed(1)} dB is too near the top`);
  if (s.loop && pk > -3.3) say(s.name, `the limiter is working on it (peak ${pk.toFixed(1)} dB): a loop should not need one`);
  let mean = 0; for (let i = 0; i < n; i++) mean += m[i]; mean /= n;
  if (Math.abs(mean) > 0.004) say(s.name, `sits off centre by ${mean.toFixed(4)}`);
  // Rumble: how much of the sound is below 60 Hz.
  const sub = rms(lp4(m, 60)) / (all || 1);
  if (s.loop && sub > 0.6) say(s.name, `${Math.round(sub * 100)}% of it is below 60 Hz`);
  if (isStereo(x)) {
    const l = rms(x.L), r = rms(x.R), lean = toDb(l / (r || 1e-9));
    if (Math.abs(lean) > 3) say(s.name, `leans ${lean > 0 ? 'left' : 'right'} by ${Math.abs(lean).toFixed(1)} dB`);
  }
  // The biggest step between neighbouring samples that is normal for this sound.
  const steps = []; for (let i = 1; i < n; i += 7) steps.push(Math.abs(m[i] - m[i - 1]));
  steps.sort((a, b) => a - b);
  const usual = steps[Math.floor(steps.length * 0.999)] || 1e-6;
  if (s.loop) {
    const join = Math.abs(m[0] - m[n - 1]);
    if (join > usual * 3 && join > 0.004) say(s.name, `jumps at the join (${join.toFixed(4)} against a usual ${usual.toFixed(4)})`);
    // Does it swell and die? Loudness of each 2 s stretch against the whole.
    const win = sec(2); let lo = 1e9, hi = 0;
    for (let a = 0; a + win <= n; a += sec(0.5)) { const v = rms(m, a, a + win); lo = Math.min(lo, v); hi = Math.max(hi, v); }
    const swing = toDb(hi / (lo || 1e-9));
    if (swing > 9) say(s.name, `swings ${swing.toFixed(1)} dB between its quietest and loudest two seconds`);
    // The two ends should be as loud as each other.
    const ends = toDb(rms(m, 0, sec(0.5)) / (rms(m, n - sec(0.5), n) || 1e-9));
    if (Math.abs(ends) > 12) say(s.name, `its first and last half second differ by ${ends.toFixed(1)} dB`);
  } else {
    if (Math.abs(m[0]) > 0.02) say(s.name, `starts with a step of ${m[0].toFixed(3)}`);
    const tail = toDb(rms(m, n - sec(0.005), n));
    if (tail > -50) say(s.name, `is cut off at the end (the last 5 ms are at ${tail.toFixed(0)} dB)`);
    // Dead air before the sound starts.
    let first = 0; while (first < n && Math.abs(m[first]) < 0.003) first++;
    if (first > sec(0.06)) say(s.name, `has ${(first / SR).toFixed(2)} s of nothing before it starts`);
    // A click inside: one sample step far beyond the usual.
    let worst = 0, at = 0; for (let i = 1; i < n; i++) { const d = Math.abs(m[i] - m[i - 1]); if (d > worst) { worst = d; at = i; } }
    if (worst > 0.5 && !['guns', 'flak'].includes(s.family)) say(s.name, `has a step of ${worst.toFixed(2)} at ${(at / SR).toFixed(3)} s`);
  }
  if (s.smallSpeakerLoss > 6) say(s.name, `loses ${s.smallSpeakerLoss} dB on a small speaker`);
  const want = s.loop ? loudness(x).integrated : loudness(x).moment;
  if (Math.abs(want - s.lufs) > 0.6) say(s.name, `measures ${want.toFixed(1)} LUFS, the list says ${s.lufs}`);
}
console.log(`${list.length} sounds checked, ${faults} thing${faults === 1 ? '' : 's'} to look at`);
