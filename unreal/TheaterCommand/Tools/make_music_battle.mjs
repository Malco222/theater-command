// Writes the music for battles: an original piece, composed here as notes and played by a small
// synthesiser, so nothing is borrowed from any recording, film or game.
//
// "The Advance": a march in D minor at a quick step, a little over a minute, made to loop without a
// seam. A snare drum and the low strings drive it from the first bar; the horns state a short
// martial figure, repeat it higher with the strings, climb, and come back down onto the chord that
// leads into the beginning again.
//
//   node Tools/make_music_battle.mjs <out.wav>
//
// It uses the same synthesiser as "The Map Room" (make_music_map_room.mjs).

import fs from 'node:fs';

const SR = 44100;
const BPM = 116;
const BEAT = 60 / BPM;
const BAR = 4 * BEAT;
const BARS = 32;
const TAIL = 4; // seconds of echo after the last bar, folded back onto the beginning so the loop has no seam
const N = Math.round(BARS * BAR * SR);
const NT = N + TAIL * SR;

const dryL = new Float32Array(NT);
const dryR = new Float32Array(NT);

// ---- Notes --------------------------------------------------------------------------------

const STEP = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 };
function hz(name) {
  const m = /^([A-G])([#b]?)(-?\d)$/.exec(name);
  const midi = 12 * (Number(m[3]) + 1) + STEP[m[1]] + (m[2] === '#' ? 1 : m[2] === 'b' ? -1 : 0);
  return 440 * 2 ** ((midi - 69) / 12);
}
const up = (name, octaves = 1) => name.replace(/-?\d$/, (d) => String(Number(d) + octaves));

// A sawtooth with its corners rounded off (polyBLEP), so high notes do not hiss.
function saw(phase, dt) {
  let v = 2 * phase - 1;
  if (phase < dt) { const t = phase / dt; v -= t + t - t * t - 1; }
  else if (phase > 1 - dt) { const t = (phase - 1) / dt; v -= t * t + t + t + 1; }
  return v;
}

let seed = 1939;
function noise() { // the same hiss every time, so the piece is the same every time it is made
  seed = (seed * 1664525 + 1013904223) >>> 0;
  return seed / 2147483648 - 1;
}

function add(i, v, pan) {
  if (i < 0 || i >= NT) return;
  dryL[i] += v * Math.cos((pan + 1) * Math.PI / 4);
  dryR[i] += v * Math.sin((pan + 1) * Math.PI / 4);
}

const smooth = (t) => (t <= 0 ? 0 : t >= 1 ? 1 : t * t * (3 - 2 * t));

// ---- Instruments --------------------------------------------------------------------------

/** Sustained strings: several slightly mistuned saws, filtered soft, swelling in and dying away. */
function strings(start, dur, note, vol, pan = 0, o = {}) {
  const f = hz(note);
  const attack = o.attack ?? 0.9, release = o.release ?? 1.4, bright = o.bright ?? 1;
  const tunes = [-0.006, 0.0, 0.0055, -0.0025];
  const phases = tunes.map((_, k) => (k * 0.37 + f * 0.013) % 1);
  const i0 = Math.round(start * SR), len = Math.round((dur + release) * SR);
  let a = 0, b = 0;
  for (let i = 0; i < len; i++) {
    const t = i / SR;
    const env = smooth(t / attack) * (t < dur ? 1 : 1 - smooth((t - dur) / release));
    const wob = 1 + 0.0025 * Math.sin(2 * Math.PI * 5.1 * t + f); // a little bow vibrato
    let s = 0;
    for (let k = 0; k < tunes.length; k++) {
      const dt = f * (1 + tunes[k]) * wob / SR;
      phases[k] += dt; if (phases[k] >= 1) phases[k] -= 1;
      s += saw(phases[k], dt);
    }
    const cut = Math.min(0.6, (900 + 2800 * bright * env + f * 2.6) / SR * 2 * Math.PI);
    a += cut * (s * 0.25 - a); b += cut * (a - b);
    add(i0 + i, b * env * vol, pan + 0.25 * Math.sin(k0(i)));
  }
}
const k0 = (i) => i / SR * 0.21; // a slow drift across the room

/** A horn: one saw and its fundamental, opening as it speaks, with vibrato once the note has settled. */
function horn(start, dur, note, vol, pan = -0.15) {
  const f = hz(note);
  const attack = 0.1, release = 0.35;
  const i0 = Math.round(start * SR), len = Math.round((dur + release) * SR);
  let phase = 0, a = 0, b = 0;
  for (let i = 0; i < len; i++) {
    const t = i / SR;
    const env = smooth(t / attack) * (t < dur ? 1 - 0.18 * smooth(t / Math.max(dur, 0.01)) : 0.82 * (1 - smooth((t - dur) / release)));
    const vib = 1 + 0.004 * smooth((t - 0.35) / 0.5) * Math.sin(2 * Math.PI * 4.8 * t);
    const dt = f * vib / SR;
    phase += dt; if (phase >= 1) phase -= 1;
    const s = 0.55 * saw(phase, dt) + 0.6 * Math.sin(2 * Math.PI * phase);
    const cut = Math.min(0.55, (650 + 2400 * env * env + f * 1.6) / SR * 2 * Math.PI);
    a += cut * (s - a); b += cut * (a - b);
    add(i0 + i, b * env * vol, pan);
  }
}

/** Low strings played short: the marching figure under the theme. */
function pluck(start, note, vol, pan = 0.3, len = 0.42) {
  const f = hz(note);
  const i0 = Math.round(start * SR), n = Math.round(len * SR);
  let p1 = 0, p2 = 0.4, a = 0, b = 0;
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    const env = smooth(t / 0.012) * Math.exp(-t / 0.16);
    const d1 = f / SR, d2 = f * 1.004 / SR;
    p1 += d1; if (p1 >= 1) p1 -= 1;
    p2 += d2; if (p2 >= 1) p2 -= 1;
    const s = saw(p1, d1) + saw(p2, d2);
    const cut = Math.min(0.5, (520 + 2300 * env) / SR * 2 * Math.PI);
    a += cut * (s * 0.5 - a); b += cut * (a - b);
    add(i0 + i, b * env * vol, pan);
  }
}

/** A kettledrum: a note that falls as it is struck, and the thump of the stick. */
function timpani(start, note, vol, pan = 0.05) {
  const f = hz(note);
  const i0 = Math.round(start * SR), n = Math.round(2.6 * SR);
  let phase = 0, lp = 0;
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    phase += f * (1 + 0.5 * Math.exp(-t / 0.018)) / SR;
    const tone = Math.sin(2 * Math.PI * phase) + 0.32 * Math.sin(4 * Math.PI * phase + 0.4) * Math.exp(-t / 0.3);
    lp += 0.05 * (noise() - lp);
    add(i0 + i, (tone * Math.exp(-t / 0.75) * smooth(t / 0.004) + lp * 3.2 * Math.exp(-t / 0.045)) * vol * 0.7, pan);
  }
}

/** A snare drum, far off. */
function snare(start, vol, pan = 0.35) {
  const i0 = Math.round(start * SR), n = Math.round(0.3 * SR);
  let lo = 0, hi = 0;
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    const w = noise();
    lo += 0.55 * (w - lo); hi += 0.1 * (w - hi); // keep the band between the two
    const body = Math.sin(2 * Math.PI * 185 * t) * Math.exp(-t / 0.04);
    add(i0 + i, ((lo - hi) * Math.exp(-t / 0.075) + 0.35 * body) * vol, pan);
  }
}

/** The floor under everything: a soft, very low held note. */
function drone(start, dur, note, vol) {
  const f = hz(note);
  const i0 = Math.round(start * SR), len = Math.round((dur + 3) * SR);
  let phase = 0, p2 = 0.3, a = 0;
  for (let i = 0; i < len; i++) {
    const t = i / SR;
    const env = smooth(t / 3) * (t < dur ? 1 : 1 - smooth((t - dur) / 3));
    phase += f / SR; if (phase >= 1) phase -= 1;
    const d2 = f * 2.003 / SR; p2 += d2; if (p2 >= 1) p2 -= 1;
    a += 0.03 * (saw(p2, d2) - a);
    const swell = 0.85 + 0.15 * Math.sin(2 * Math.PI * t / 9.5);
    add(i0 + i, (Math.sin(2 * Math.PI * phase) + 0.5 * a) * env * swell * vol, 0);
  }
}

// ---- The piece ----------------------------------------------------------------------------

const at = (bar, beat = 0) => bar * BAR + beat * BEAT;

const CHORDS = {
  Dm: { pad: ['D3', 'A3', 'D4', 'F4'], bass: 'D2', figure: ['D2', 'D2', 'A2', 'D2', 'F3', 'D2', 'A2', 'D2'] },
  Bb: { pad: ['Bb2', 'F3', 'Bb3', 'D4'], bass: 'Bb1', figure: ['Bb1', 'Bb1', 'F2', 'Bb1', 'D3', 'Bb1', 'F2', 'Bb1'] },
  Gm: { pad: ['G2', 'D3', 'G3', 'Bb3'], bass: 'G1', figure: ['G1', 'G1', 'D2', 'G1', 'Bb2', 'G1', 'D2', 'G1'] },
  A: { pad: ['A2', 'E3', 'A3', 'C#4'], bass: 'A1', figure: ['A1', 'A1', 'E2', 'A1', 'C#3', 'A1', 'E2', 'A1'] },
  F: { pad: ['F2', 'C3', 'F3', 'A3'], bass: 'F1', figure: ['F1', 'F1', 'C2', 'F1', 'A2', 'F1', 'C2', 'F1'] },
  C: { pad: ['C3', 'G3', 'C4', 'E4'], bass: 'C2', figure: ['C2', 'C2', 'G2', 'C2', 'E3', 'C2', 'G2', 'C2'] },
};

// One chord to a bar, in four phrases of eight.
const HARMONY = [
  'Dm', 'Dm', 'Bb', 'Bb', 'Gm', 'Gm', 'A', 'A', // the figure
  'Dm', 'Dm', 'F', 'C', 'Dm', 'Bb', 'A', 'A', // again, higher, with the strings
  'Bb', 'Bb', 'C', 'C', 'Dm', 'Dm', 'A', 'A', // the climb
  'Dm', 'Gm', 'Bb', 'A', 'Dm', 'Gm', 'A', 'A', // and down, onto the chord that leads back in
];

// How hard each phrase is played.
const LEVEL = (bar) => (bar < 2 ? 0.6 : bar < 8 ? 0.72 : bar < 16 ? 0.84 : bar < 24 ? 0.94 + 0.008 * (bar - 16) : bar < 30 ? 1.0 : 0.86);

HARMONY.forEach((name, bar) => {
  const chord = CHORDS[name];
  const level = LEVEL(bar);
  // The harmony held in the strings, under everything.
  chord.pad.forEach((note, k) => strings(at(bar), BAR, note, 0.06 * level, [-0.45, 0.4, -0.15, 0.2][k % 4], { attack: 0.3, release: 0.5, bright: 0.6 + 0.5 * level }));
  strings(at(bar), BAR, chord.bass, 0.06 * level, 0, { attack: 0.2, release: 0.4, bright: 0.5 });
  if (bar >= 16) chord.pad.slice(-2).forEach((note, k) => strings(at(bar), BAR, up(note), 0.04 * level, k ? 0.5 : -0.5, { attack: 0.4, release: 0.5, bright: 1.1 }));
  // The marching figure in the low strings, a note to every quaver: this is what drives the piece.
  chord.figure.forEach((note, k) => pluck(at(bar, k * 0.5), note, (k % 2 ? 0.1 : 0.15) * level, 0.25, 0.3));
});

// The tune: [bar, beat, length in beats, note].
const FIGURE = [
  [1, 3.5, 0.5, 'A3'],
  [2, 0, 1.5, 'D4'], [2, 1.5, 0.5, 'D4'], [2, 2, 1, 'F4'], [2, 3, 1, 'D4'],
  [3, 0, 3, 'Bb3'], [3, 3, 1, 'D4'],
  [4, 0, 1.5, 'G4'], [4, 1.5, 0.5, 'G4'], [4, 2, 1, 'Bb4'], [4, 3, 1, 'G4'],
  [5, 0, 3, 'D4'], [5, 3, 1, 'G4'],
  [6, 0, 1.5, 'A4'], [6, 1.5, 0.5, 'A4'], [6, 2, 1, 'C#5'], [6, 3, 1, 'A4'],
  [7, 0, 2, 'E4'], [7, 2, 1.5, 'A4'],
];
const HIGHER = [
  [8, 0, 1.5, 'D5'], [8, 1.5, 0.5, 'D5'], [8, 2, 1, 'A4'], [8, 3, 1, 'F4'],
  [9, 0, 2, 'D4'], [9, 2, 1, 'F4'], [9, 3, 1, 'A4'],
  [10, 0, 1.5, 'C5'], [10, 1.5, 0.5, 'C5'], [10, 2, 1, 'A4'], [10, 3, 1, 'F4'],
  [11, 0, 2, 'G4'], [11, 2, 1, 'E4'], [11, 3, 1, 'G4'],
  [12, 0, 1, 'F4'], [12, 1, 1, 'A4'], [12, 2, 2, 'D5'],
  [13, 0, 1.5, 'D5'], [13, 1.5, 0.5, 'C5'], [13, 2, 2, 'Bb4'],
  [14, 0, 2, 'A4'], [14, 2, 1, 'C#5'], [14, 3, 1, 'E5'],
  [15, 0, 3, 'E5'],
];
const CLIMB = [
  [16, 0, 2, 'F5'], [16, 2, 1, 'D5'], [16, 3, 1, 'Bb4'],
  [17, 0, 3, 'F5'], [17, 3, 1, 'D5'],
  [18, 0, 2, 'G5'], [18, 2, 1, 'E5'], [18, 3, 1, 'C5'],
  [19, 0, 3, 'G5'], [19, 3, 1, 'E5'],
  [20, 0, 2, 'A5'], [20, 2, 1, 'F5'], [20, 3, 1, 'D5'],
  [21, 0, 2, 'A5'], [21, 2, 2, 'F5'],
  [22, 0, 1.5, 'A5'], [22, 1.5, 0.5, 'G5'], [22, 2, 1, 'E5'], [22, 3, 1, 'C#5'],
  [23, 0, 2, 'A4'], [23, 2, 2, 'E5'],
];
const DOWN = [
  [24, 0, 1.5, 'D5'], [24, 1.5, 0.5, 'D5'], [24, 2, 1, 'F5'], [24, 3, 1, 'A5'],
  [25, 0, 2, 'G5'], [25, 2, 2, 'Bb5'],
  [26, 0, 2, 'F5'], [26, 2, 2, 'D5'],
  [27, 0, 2, 'E5'], [27, 2, 2, 'C#5'],
  [28, 0, 1.5, 'D5'], [28, 1.5, 0.5, 'D5'], [28, 2, 1, 'A4'], [28, 3, 1, 'F4'],
  [29, 0, 2, 'G4'], [29, 2, 2, 'Bb4'],
  [30, 0, 4, 'A4'],
  [31, 0, 1.5, 'A4'],
];

const spoken = (beats) => beats * BEAT * 0.86; // each note a little short of its length, as a march is played
FIGURE.forEach(([bar, beat, len, note]) => horn(at(bar, beat), spoken(len), note, 0.3));
HIGHER.forEach(([bar, beat, len, note]) => {
  horn(at(bar, beat), spoken(len), up(note, -1), 0.26);
  strings(at(bar, beat), spoken(len), note, 0.13, -0.2, { attack: 0.08, release: 0.3, bright: 1.4 });
});
CLIMB.forEach(([bar, beat, len, note]) => {
  horn(at(bar, beat), spoken(len), up(note, -1), 0.3);
  strings(at(bar, beat), spoken(len), note, 0.16, -0.2, { attack: 0.08, release: 0.3, bright: 1.6 });
  strings(at(bar, beat), spoken(len), up(note, -1), 0.07, 0.25, { attack: 0.08, release: 0.3, bright: 1.2 });
});
DOWN.forEach(([bar, beat, len, note]) => {
  horn(at(bar, beat), spoken(len), up(note, -1), 0.3);
  strings(at(bar, beat), spoken(len), note, 0.15, -0.2, { attack: 0.08, release: 0.3, bright: 1.5 });
});

// Drums. The snare marches from the first bar; kettledrums on the first and third beats, and a roll into the repeat.
for (let bar = 0; bar < BARS; bar++) {
  const root = CHORDS[HARMONY[bar]].bass;
  const low = /1$/.test(root) ? up(root) : root; // keep the drums in the octave where they sound like drums
  const level = LEVEL(bar);
  timpani(at(bar), low, 0.3 * level);
  if (bar >= 8) timpani(at(bar, 2), low, 0.2 * level);
  if (bar >= 16 && bar % 2) timpani(at(bar, 3.5), low, 0.16 * level);
  // One bar of the march: beats, and how hard each is struck; a flam of semiquavers into every fourth bar.
  const loud = 0.1 * level;
  [[0, 1], [0.5, 0.4], [1, 0.8], [1.5, 0.45], [1.75, 0.4], [2, 0.95], [2.5, 0.4], [3, 0.8], [3.5, 0.5], [3.75, 0.45]]
    .forEach(([beat, hit]) => snare(at(bar, beat), loud * hit, 0.3));
  if (bar % 4 === 3) [3.125, 3.25, 3.375, 3.625, 3.875].forEach((beat) => snare(at(bar, beat), loud * 0.4, 0.3));
}
for (let k = 0; k < 8; k++) timpani(at(31, 2) + k * BEAT / 4, 'A2', 0.1 + 0.03 * k);

// ---- The room: echo, the fold for the loop, and the level -----------------------------------

/** A hall: eight echoes in parallel and four diffusers in series (the long-published Freeverb layout). */
function hall(input, spread) {
  const out = new Float32Array(input.length);
  const combs = [1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617].map((d) => ({ buf: new Float32Array(d + spread), i: 0, store: 0 }));
  const passes = [556, 441, 341, 225].map((d) => ({ buf: new Float32Array(d + spread), i: 0 }));
  const feedback = 0.9, damp = 0.32;
  for (let n = 0; n < input.length; n++) {
    const x = input[n] * 0.03;
    let y = 0;
    for (const c of combs) {
      const v = c.buf[c.i];
      c.store = v * (1 - damp) + c.store * damp;
      c.buf[c.i] = x + c.store * feedback;
      if (++c.i >= c.buf.length) c.i = 0;
      y += v;
    }
    for (const p of passes) {
      const v = p.buf[p.i];
      p.buf[p.i] = y + v * 0.5;
      if (++p.i >= p.buf.length) p.i = 0;
      y = v - y;
    }
    out[n] = y;
  }
  return out;
}

const wetL = hall(dryL, 0);
const wetR = hall(dryR, 23);
const outL = new Float32Array(N);
const outR = new Float32Array(N);
for (let i = 0; i < NT; i++) {
  const l = dryL[i] * 0.8 + wetL[i] * 0.9;
  const r = dryR[i] * 0.8 + wetR[i] * 0.9;
  // What rings on after the last bar is laid over the first bars, so the end runs into the beginning.
  outL[i % N] += l;
  outR[i % N] += r;
}

let peak = 0;
for (let i = 0; i < N; i++) peak = Math.max(peak, Math.abs(outL[i]), Math.abs(outR[i]));
const gain = 0.86 / peak;

const header = Buffer.alloc(44);
header.write('RIFF', 0); header.writeUInt32LE(36 + N * 4, 4); header.write('WAVE', 8);
header.write('fmt ', 12); header.writeUInt32LE(16, 16); header.writeUInt16LE(1, 20); header.writeUInt16LE(2, 22);
header.writeUInt32LE(SR, 24); header.writeUInt32LE(SR * 4, 28); header.writeUInt16LE(4, 32); header.writeUInt16LE(16, 34);
header.write('data', 36); header.writeUInt32LE(N * 4, 40);
const pcm = Buffer.alloc(N * 4);
for (let i = 0; i < N; i++) {
  pcm.writeInt16LE(Math.round(Math.max(-1, Math.min(1, outL[i] * gain)) * 32767), i * 4);
  pcm.writeInt16LE(Math.round(Math.max(-1, Math.min(1, outR[i] * gain)) * 32767), i * 4 + 2);
}

const out = process.argv[2] ?? 'battle.wav';
fs.mkdirSync(new URL('.', new URL(out, `file://${process.cwd()}/`)), { recursive: true });
fs.writeFileSync(out, Buffer.concat([header, pcm]));

// How loud each section came out, as a check that the shape is the one intended.
const sections = [['the figure', 0, 8], ['higher', 8, 16], ['the climb', 24 - 8, 24], ['down', 24, 32]];
const report = sections.map(([name, from, to]) => {
  let sum = 0;
  const a = Math.round(from * BAR * SR), b = Math.min(N, Math.round(to * BAR * SR));
  for (let i = a; i < b; i++) sum += (outL[i] * gain) ** 2 + (outR[i] * gain) ** 2;
  return `${name} ${(10 * Math.log10(sum / (2 * (b - a)))).toFixed(1)} dB`;
});
console.log(`${out}: ${(N / SR).toFixed(1)} s, peak before levelling ${peak.toFixed(2)}; ${report.join(', ')}`);
