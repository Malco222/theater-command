// Looks a raw recording over before it is used. No Claude can hear, so this is the first sieve: it measures the
// things that make a recording unusable (clipping, a thin low-rate copy, wind rumble, mains hum, a high noise floor)
// and the things a recipe needs to know (where each shot falls, where the steadiest stretch for a loop lies, how
// much weight is in the low end). It does not say a recording sounds good. Only an ear can.
//
//   node sound/tools/vet.mjs <file> [<file> ...] [--json out.json] [--from s] [--to s]
//
// A film clip is read by its sound track.

import fs from 'node:fs';
import { SR, sec, toDb, loudness, fft, peak } from '../kit/dsp.mjs';
import { probe, decode } from '../kit/source.mjs';

const args = process.argv.slice(2);
const files = [];
let jsonOut = null, from = 0, to = null;
for (let i = 0; i < args.length; i++) {
  if (args[i] === '--json') jsonOut = args[++i];
  else if (args[i] === '--from') from = +args[++i];
  else if (args[i] === '--to') to = +args[++i];
  else files.push(args[i]);
}
if (!files.length) { console.error('usage: node sound/tools/vet.mjs <file> [...] [--json out.json] [--from s] [--to s]'); process.exit(1); }

const BANDS = [['sub', 0, 40], ['low', 40, 120], ['lowmid', 120, 400], ['mid', 400, 2000], ['presence', 2000, 6000], ['air', 6000, 24000]];

/** The average spectrum of a sound, as power in each of 4096 bins up to 24 kHz. */
function spectrum(x) {
  const N = 8192, hop = N / 2, half = N / 2, acc = new Float64Array(half);
  const win = new Float64Array(N); for (let i = 0; i < N; i++) win[i] = 0.5 - 0.5 * Math.cos(2 * Math.PI * i / N);
  let frames = 0;
  const re = new Float64Array(N), im = new Float64Array(N);
  for (let at = 0; at + N <= x.length; at += hop) {
    for (let i = 0; i < N; i++) { re[i] = x[at + i] * win[i]; im[i] = 0; }
    fft(re, im, false);
    for (let k = 0; k < half; k++) acc[k] += re[k] * re[k] + im[k] * im[k];
    frames++;
  }
  if (frames) for (let k = 0; k < half; k++) acc[k] /= frames;
  return { power: acc, hz: SR / N };
}

/** Short-term level, in dB, every "hop" seconds over a window of "win" seconds. */
function levels(x, win, hop) {
  const w = sec(win), h = sec(hop), out = [];
  for (let at = 0; at + w <= x.length; at += h) { let s = 0; for (let i = at; i < at + w; i++) s += x[i] * x[i]; out.push(10 * Math.log10(s / w + 1e-12)); }
  return out;
}
const pct = (sorted, p) => sorted[Math.min(sorted.length - 1, Math.max(0, Math.round(p * (sorted.length - 1))))];

function vet(file) {
  const info = probe(file);
  const span = { from, to: to ?? Math.min(info.seconds || 600, from + 600) };
  const st = decode(file, { ...span, channels: 2 });
  const n = st.L.length;
  const x = new Float32Array(n); for (let i = 0; i < n; i++) x[i] = 0.5 * (st.L[i] + st.R[i]);
  const flags = [];

  // Level and clipping. A clipped recording has flat tops: runs of samples stuck at the ceiling.
  const pk = peak(st);
  let runs = 0, run = 0, stuck = 0;
  for (const ch of [st.L, st.R]) for (let i = 0; i < n; i++) { if (Math.abs(ch[i]) >= pk * 0.985 && pk > 0.5) { run++; } else { if (run >= 3) { runs++; stuck += run; } run = 0; } }
  const l = loudness(st);
  if (runs > 4) flags.push(`CLIPPED: ${runs} flat tops (${stuck} samples at the ceiling)`);

  // Noise floor and how far the loud parts stand above it.
  const lv = levels(x, 0.05, 0.025), sorted = [...lv].sort((a, b) => a - b);
  const floor = pct(sorted, 0.05), top = pct(sorted, 0.99), median = pct(sorted, 0.5);

  // Where the sound's weight lies.
  const sp = spectrum(x);
  let total = 0; for (let k = 1; k < sp.power.length; k++) total += sp.power[k];
  const bands = {};
  for (const [name, lo, hi] of BANDS) { let s = 0; for (let k = Math.max(1, Math.ceil(lo / sp.hz)); k < Math.min(sp.power.length, Math.floor(hi / sp.hz)); k++) s += sp.power[k]; bands[name] = +(10 * Math.log10(s / total + 1e-12)).toFixed(1); }

  // A low-rate copy (a small MP3, a phone clip) has nothing above a shelf. Find where the top falls away for good.
  const third = []; // third-octave smoothed levels from 1 kHz up
  for (let f = 1000; f < 23500; f *= Math.pow(2, 1 / 6)) { let s = 0, c = 0; for (let k = Math.floor(f / sp.hz); k < Math.min(sp.power.length, Math.ceil(f * Math.pow(2, 1 / 6) / sp.hz)); k++) { s += sp.power[k]; c++; } third.push([f, 10 * Math.log10(s / Math.max(1, c) + 1e-20)]); }
  const ref = Math.max(...third.slice(0, 12).map((t) => t[1]));
  let cutoff = 24000;
  for (let i = third.length - 1; i >= 0; i--) { if (third[i][1] > ref - 70) { cutoff = third[Math.min(third.length - 1, i + 1)][0]; break; } }
  if (cutoff < 15000) flags.push(`THIN TOP: nothing above ${(cutoff / 1000).toFixed(1)} kHz (a low-rate copy, or a dull microphone)`);
  const thin = { mp3: 120, aac: 90, vorbis: 90, opus: 60 }[info.codec];
  if (info.kbps && thin && info.kbps / Math.max(1, info.channels) * (info.channels > 1 ? 1.4 : 1) < thin) flags.push(`LOW RATE: ${info.codec} at ${info.kbps} kbps`);

  // Wind and handling: heavy, restless energy below 40 Hz.
  if (bands.sub > -6) flags.push(`RUMBLE: ${bands.sub} dB of the energy is below 40 Hz (wind or handling noise is likely)`);

  // Mains hum: a narrow line at 50 or 60 Hz standing well above its neighbours.
  for (const hum of [50, 60]) {
    const k = Math.round(hum / sp.hz), near = (a, b) => { let s = 0, c = 0; for (let i = a; i <= b; i++) { s += sp.power[i]; c++; } return s / c; };
    const line = Math.max(sp.power[k - 1], sp.power[k], sp.power[k + 1]), around = 0.5 * (near(k - 6, k - 3) + near(k + 3, k + 6));
    const k2 = Math.round(2 * hum / sp.hz), line2 = Math.max(sp.power[k2 - 1], sp.power[k2], sp.power[k2 + 1]), around2 = 0.5 * (near(k2 - 6, k2 - 3) + near(k2 + 3, k2 + 6));
    if (10 * Math.log10(line / around) > 15 && 10 * Math.log10(line2 / around2) > 10) flags.push(`HUM: a ${hum} Hz line and its octave stand out`);
  }
  if (floor > -45) flags.push(`NOISY: the quietest moments sit at ${floor.toFixed(0)} dB (no clean silence between events)`);

  // Events: moments where the level leaps. For guns, each is a shot.
  const fast = levels(x, 0.01, 0.005), events = [];
  for (let i = 24; i < fast.length; i++) {
    const before = [...fast.slice(i - 24, i - 2)].sort((a, b) => a - b)[11];
    if (fast[i] - before >= 14 && fast[i] > top - 18) {
      const t = from + i * 0.005;
      if (!events.length || t - events[events.length - 1].at > 0.15) { let p = fast[i]; for (let j = i; j < Math.min(fast.length, i + 20); j++) p = Math.max(p, fast[j]); events.push({ at: +t.toFixed(3), db: +p.toFixed(1) }); }
    }
  }

  // The steadiest stretch, for a loop: the 8 seconds whose level moves least.
  const slow = levels(x, 0.4, 0.1);
  let steady = null;
  const W = 80;
  if (slow.length > W) {
    let best = Infinity, bestAt = 0;
    for (let i = 0; i + W <= slow.length; i += 5) { let m = 0; for (let j = i; j < i + W; j++) m += slow[j]; m /= W; let v = 0; for (let j = i; j < i + W; j++) v += (slow[j] - m) ** 2; v = Math.sqrt(v / W); if (v < best && m > median - 6) { best = v; bestAt = i; } }
    if (best < Infinity) steady = { from: +(from + bestAt * 0.1).toFixed(1), to: +(from + bestAt * 0.1 + 8).toFixed(1), wobbleDb: +best.toFixed(2) };
  }

  // Speech has a beat of its own: the voice band swells about four times a second. A rough guess only.
  let talk = null;
  {
    const N = 4096, hop = 1024, env = [];
    const re = new Float64Array(N), im = new Float64Array(N);
    for (let at = 0; at + N <= n; at += hop) {
      for (let i = 0; i < N; i++) { re[i] = x[at + i] * (0.5 - 0.5 * Math.cos(2 * Math.PI * i / N)); im[i] = 0; }
      fft(re, im, false);
      let s = 0; for (let k = Math.round(300 * N / SR); k < Math.round(3400 * N / SR); k++) s += re[k] * re[k] + im[k] * im[k];
      env.push(Math.sqrt(s));
    }
    if (env.length > 128) {
      const M = 1 << Math.floor(Math.log2(env.length)), er = new Float64Array(M), ei = new Float64Array(M);
      const mean = env.slice(0, M).reduce((a, b) => a + b, 0) / M;
      for (let i = 0; i < M; i++) er[i] = (env[i] - mean) / (mean + 1e-9);
      fft(er, ei, false);
      const fs2 = SR / hop, bin = fs2 / M;
      let syll = 0, all = 0;
      for (let k = 1; k < M / 2; k++) { const f = k * bin, p = er[k] * er[k] + ei[k] * ei[k]; if (f >= 0.5 && f <= 16) all += p; if (f >= 2.5 && f <= 6.5) syll += p; }
      talk = +(syll / (all + 1e-12)).toFixed(2);
    }
  }
  if (talk != null && talk > 0.5 && bands.mid > -6) flags.push(`TALKING? the voice band beats like speech (${talk}); read the description before using`);

  // How wide it is.
  let lr = 0, ll = 0, rr = 0; for (let i = 0; i < n; i++) { lr += st.L[i] * st.R[i]; ll += st.L[i] * st.L[i]; rr += st.R[i] * st.R[i]; }
  const corr = lr / Math.sqrt(ll * rr + 1e-20);

  return {
    file, format: `${info.codec} ${info.rate} Hz ${info.channels} ch${info.bits ? ' ' + info.bits + ' bit' : ''}${info.kbps ? ' ' + info.kbps + ' kbps' : ''}`,
    seconds: +(info.seconds || n / SR).toFixed(1), read: [span.from, +(span.from + n / SR).toFixed(1)], fromFilm: info.hasVideo,
    peakDb: +toDb(pk).toFixed(1), lufs: +l.integrated.toFixed(1), loudestMomentLufs: +l.moment.toFixed(1),
    floorDb: +floor.toFixed(1), rangeDb: +(top - floor).toFixed(1), topKhz: +(cutoff / 1000).toFixed(1), bands,
    stereo: info.channels > 1 ? +corr.toFixed(2) : null, talk, events: events.length, firstEvents: events.slice(0, 40),
    loudestEvents: [...events].sort((p, q) => q.db - p.db).slice(0, 40), steady, flags,
  };
}

const all = [];
for (const f of files) {
  let r;
  try { r = vet(f); } catch (e) { console.log(`${f}\n  COULD NOT READ: ${e.message}\n`); continue; }
  all.push(r);
  console.log(`${r.file}`);
  console.log(`  ${r.format}, ${r.seconds} s${r.fromFilm ? ' (a film clip, read by its sound track)' : ''}`);
  console.log(`  peak ${r.peakDb} dB, whole ${r.lufs} LUFS, loudest moment ${r.loudestMomentLufs} LUFS, floor ${r.floorDb} dB, range ${r.rangeDb} dB, top ${r.topKhz} kHz`);
  console.log(`  weight: sub ${r.bands.sub}  low ${r.bands.low}  low-mid ${r.bands.lowmid}  mid ${r.bands.mid}  presence ${r.bands.presence}  air ${r.bands.air}   (dB of the whole)`);
  console.log(`  ${r.events} events${r.events ? ': ' + r.firstEvents.slice(0, 12).map((e) => `${e.at}s`).join(' ') + (r.events > 12 ? ' ...' : '') : ''}`);
  if (r.steady) console.log(`  steadiest 8 s: ${r.steady.from} to ${r.steady.to} s (level moves ${r.steady.wobbleDb} dB)`);
  if (r.stereo != null) console.log(`  left and right agree ${r.stereo} (1 = the same on both sides, 0 = wide)`);
  console.log(r.flags.length ? r.flags.map((f2) => `  ! ${f2}`).join('\n') : '  no faults measured (that is not the same as sounding good)');
  console.log('');
}
if (jsonOut) fs.writeFileSync(jsonOut, JSON.stringify(all, null, 1));
