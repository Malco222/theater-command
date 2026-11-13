// Makes the game's sound effects: every one synthesised here from sine waves and noise, so nothing is
// borrowed from any recording, film or game.
//
//   node Tools/make_sounds.mjs <out-folder>
//
// Each effect is a short stereo WAV named for what it is (rifles.wav, tank_engine.wav, ...). They are
// stand-ins of the same kind as the music: they say "a gun has fired" or "a tank is moving", and any of
// them can be replaced by a recording later (a real radial engine for the B-17, say) by dropping a file of
// the same name into Content/Campaign/Sounds.

import fs from 'node:fs';
import path from 'node:path';

const SR = 44100;
const TAU = 2 * Math.PI;

// ---- The makings ----------------------------------------------------------------------------

let seed = 20261112;
function rnd() {
  seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
  return seed / 4294967296;
}
const noise = () => rnd() * 2 - 1;
const between = (a, b) => a + (b - a) * rnd();
const clamp01 = (x) => Math.min(1, Math.max(0, x));
const smooth = (x) => { const t = clamp01(x); return t * t * (3 - 2 * t); };

function buffer(seconds) {
  const n = Math.round(seconds * SR);
  return { L: new Float32Array(n), R: new Float32Array(n), n };
}

/** Adds a sound made by fn(t) (seconds from its start) at a time, panned left (-1) to right (+1). */
function add(b, at, seconds, pan, fn) {
  const n0 = Math.round(at * SR), n = Math.round(seconds * SR);
  const gl = Math.cos((pan + 1) * Math.PI / 4), gr = Math.sin((pan + 1) * Math.PI / 4);
  for (let i = 0; i < n && n0 + i < b.n; i++) {
    if (n0 + i < 0) continue;
    const v = fn(i / SR);
    b.L[n0 + i] += v * gl;
    b.R[n0 + i] += v * gr;
  }
}

function lowpass(fc) {
  let y = 0;
  const a = 1 - Math.exp(-TAU * fc / SR);
  return (x) => (y += a * (x - y));
}
function highpass(fc) {
  const lp = lowpass(fc);
  return (x) => x - lp(x);
}
function bandpass(f, q) {
  const w = TAU * f / SR, alpha = Math.sin(w) / (2 * q), a0 = 1 + alpha;
  const b0 = alpha / a0, b2 = -alpha / a0, a1 = -2 * Math.cos(w) / a0, a2 = (1 - alpha) / a0;
  let x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  return (x) => {
    const y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x; y2 = y1; y1 = y;
    return y;
  };
}

/** The echo of a big space, laid over the whole buffer. */
function reverb(b, decay, mix, tone = 2500) {
  for (const [ch, spread] of [[b.L, 1.0], [b.R, 1.07]]) {
    const dry = Float32Array.from(ch);
    const wet = new Float32Array(b.n);
    for (const ms of [29.7, 37.1, 41.1, 43.7]) {
      const d = Math.round(ms * spread * SR / 1000), g = Math.pow(0.001, (ms * spread / 1000) / decay);
      const line = new Float32Array(b.n);
      const lp = lowpass(tone);
      for (let i = 0; i < b.n; i++) {
        line[i] = dry[i] + (i >= d ? lp(line[i - d]) * g : 0);
        wet[i] += line[i] * 0.25;
      }
    }
    for (const ms of [5.0, 1.7]) {
      const d = Math.round(ms * spread * SR / 1000), g = 0.7;
      const out = new Float32Array(b.n);
      for (let i = 0; i < b.n; i++) {
        const delayed = i >= d ? wet[i - d] + g * out[i - d] : 0;
        out[i] = -g * wet[i] + delayed * (1 - g * g) + (i >= d ? 0 : 0);
      }
      wet.set(out);
    }
    for (let i = 0; i < b.n; i++) ch[i] = dry[i] * (1 - mix * 0.3) + wet[i] * mix;
  }
}

/** Presses the loud parts down so the sound is thick rather than spiky. */
function drive(b, amount) {
  for (const ch of [b.L, b.R]) for (let i = 0; i < b.n; i++) ch[i] = Math.tanh(ch[i] * amount);
}

/** Fades the ends and brings the loudest point to just under full. */
function finish(b, fadeIn = 0.004, fadeOut = 0.05, peak = 0.89) {
  const ni = Math.round(fadeIn * SR), no = Math.round(fadeOut * SR);
  let top = 1e-9;
  for (const ch of [b.L, b.R]) {
    for (let i = 0; i < ni && i < b.n; i++) ch[i] *= i / ni;
    for (let i = 0; i < no && i < b.n; i++) ch[b.n - 1 - i] *= i / no;
    for (let i = 0; i < b.n; i++) top = Math.max(top, Math.abs(ch[i]));
  }
  for (const ch of [b.L, b.R]) for (let i = 0; i < b.n; i++) ch[i] *= peak / top;
  return b;
}

function mixInto(dest, src, at, gain) {
  const n0 = Math.round(at * SR);
  for (let i = 0; i < src.n && n0 + i < dest.n; i++) {
    dest.L[n0 + i] += src.L[i] * gain;
    dest.R[n0 + i] += src.R[i] * gain;
  }
}

// ---- The parts sounds are built from ----------------------------------------------------------

/** One shot from a small arm: a crack and a thump. */
function shot(b, at, pan, gain, { bright = 2200, crack = 0.018, thump = 0.05, low = 170 } = {}) {
  const bp = bandpass(bright * between(0.85, 1.15), 0.7);
  let phase = 0;
  add(b, at, 0.25, pan, (t) => {
    const f = low * (0.4 + 0.6 * Math.exp(-t / 0.03));
    phase += TAU * f / SR;
    return gain * (bp(noise()) * Math.exp(-t / crack) * 2.2 + Math.sin(phase) * Math.exp(-t / thump) * 0.8);
  });
}

/** A gun going off: a bang of noise over a falling boom. */
function boom(b, at, pan, gain, { from = 110, to = 38, ring = 0.35, bang = 0.12, tone = 1200 } = {}) {
  const lp = lowpass(tone);
  let phase = 0;
  add(b, at, ring * 6, pan, (t) => {
    const f = to + (from - to) * Math.exp(-t / (ring * 0.5));
    phase += TAU * f / SR;
    const click = t < 0.003 ? 1 - t / 0.003 : 0;
    return gain * (lp(noise()) * Math.exp(-t / bang) * 1.6 + Math.sin(phase) * Math.exp(-t / ring) * 1.2 + click * 0.6);
  });
}

/** A struck piece of metal: a few partials that are not in tune with each other. */
function clank(b, at, pan, gain, partials, decay) {
  const phases = partials.map(() => rnd() * TAU);
  add(b, at, decay * 7, pan, (t) => {
    let v = 0;
    partials.forEach((f, k) => { v += Math.sin(TAU * f * t + phases[k]) * Math.exp(-t / (decay / (1 + k * 0.35))) / (1 + k * 0.5); });
    return gain * v;
  });
}

/** An engine: a row of harmonics on a low note that wobbles as the cylinders fire, with the breath of the exhaust. */
function engine(b, at, seconds, pan, gain, { f0 = 31, harmonics = 10, tone = 500, rev = 0.15, rough = 0.3, shape = () => 1 } = {}) {
  const lp = lowpass(tone), breath = lowpass(220);
  const phases = Array.from({ length: harmonics }, () => rnd() * TAU);
  let phase = 0;
  add(b, at, seconds, pan, (t) => {
    const f = f0 * (1 + rev * smooth(t / (seconds * 0.45)));
    phase += TAU * f / SR;
    let v = 0;
    for (let h = 1; h <= harmonics; h++) v += Math.sin(h * phase + phases[h - 1]) / Math.pow(h, 0.8);
    v *= 1 + rough * Math.sin(phase * 0.25);
    v += breath(noise()) * 2.5 * (0.6 + 0.4 * Math.sin(phase * 2));
    return gain * lp(v) * shape(t);
  });
}

/** A brass note: bright at the start, with a little vibrato once it is held. */
function brass(b, at, seconds, pan, gain, note) {
  const f = 440 * Math.pow(2, (note - 69) / 12);
  const lp = lowpass(f * 5);
  let phase = 0;
  add(b, at, seconds + 0.12, pan, (t) => {
    const vib = 1 + 0.006 * Math.sin(TAU * 5.5 * t) * smooth((t - 0.15) / 0.2);
    phase += TAU * f * vib / SR;
    let v = 0;
    for (let h = 1; h <= 9; h++) v += Math.sin(h * phase) / h * (h % 2 ? 1 : 0.8);
    const env = smooth(t / 0.025) * (t < seconds ? 1 - 0.25 * (t / seconds) : Math.exp(-(t - seconds) / 0.03) * 0.75);
    return gain * lp(v) * env * 0.5;
  });
}

// ---- The effects ------------------------------------------------------------------------------

const made = {};

made.rifles = () => {
  const b = buffer(2.4);
  for (let k = 0; k < 9; k++) shot(b, between(0.02, 1.3), between(-0.7, 0.7), between(0.5, 1));
  reverb(b, 1.2, 0.28);
  return finish(b);
};

made.machine_gun = () => {
  const b = buffer(2.0);
  let t = 0.03;
  for (const count of [14, 8]) {
    for (let k = 0; k < count; k++) {
      shot(b, t, 0.15, between(0.7, 1), { bright: 1900, crack: 0.012, thump: 0.03, low: 150 });
      t += 0.085 * between(0.96, 1.04);
    }
    t += 0.28;
  }
  reverb(b, 0.9, 0.2);
  return finish(b);
};

made.tank_gun = () => {
  const b = buffer(3.0);
  boom(b, 0.02, 0, 1, { from: 120, to: 40, ring: 0.32, bang: 0.11, tone: 1500 });
  clank(b, 0.85, 0.2, 0.12, [1830, 2710, 4120], 0.12); // the empty case on the turret floor
  drive(b, 1.8);
  reverb(b, 2.0, 0.35, 1800);
  return finish(b);
};

made.field_gun = () => {
  const b = buffer(3.4);
  boom(b, 0.02, -0.1, 1, { from: 95, to: 30, ring: 0.5, bang: 0.16, tone: 900 });
  boom(b, 0.65, 0.5, 0.25, { from: 70, to: 30, ring: 0.5, bang: 0.2, tone: 400 }); // its echo off the hills
  drive(b, 1.7);
  reverb(b, 2.4, 0.4, 1200);
  return finish(b);
};

made.explosion = () => {
  const b = buffer(3.6);
  let fc = 3000, y = 0, phase = 0;
  add(b, 0.02, 3.0, 0, (t) => {
    fc = 150 + 2850 * Math.exp(-t / 0.35);
    y += (1 - Math.exp(-TAU * fc / SR)) * (noise() - y);
    phase += TAU * (30 + 25 * Math.exp(-t / 0.3)) / SR;
    return y * Math.exp(-t / 0.6) * 2.4 + Math.sin(phase) * Math.exp(-t / 0.8) * 1.3;
  });
  for (let k = 0; k < 22; k++) { // what was thrown up coming down again
    const bp = bandpass(between(900, 3200), 2);
    add(b, between(0.35, 2.2), 0.06, between(-0.8, 0.8), (t) => bp(noise()) * Math.exp(-t / 0.012) * 0.35);
  }
  drive(b, 1.6);
  reverb(b, 2.5, 0.35, 1500);
  return finish(b);
};

/** The clatter of a tank's tracks, laid over an engine. */
function tracks(b, seconds, gain) {
  for (let t = 0.1; t < seconds - 0.2; t += 0.11 * between(0.9, 1.1)) {
    const bp = bandpass(between(1500, 2300), 2.5), g = between(0.4, 1) * gain;
    add(b, t, 0.05, between(-0.3, 0.3), (u) => bp(noise()) * Math.exp(-u / 0.012) * g);
  }
  add(b, 0, seconds, 0.2, (t) => Math.sin(TAU * 2600 * t + 3 * Math.sin(TAU * 0.7 * t)) * 0.02 * gain * (0.5 + 0.5 * Math.sin(TAU * 0.45 * t)));
}

made.tank_engine = () => {
  const b = buffer(3.6);
  const shape = (t) => smooth(t / 0.3) * smooth((3.6 - t) / 0.7);
  engine(b, 0, 3.6, 0, 1, { f0: 31, harmonics: 10, tone: 480, rev: 0.18, shape });
  tracks(b, 3.6, 0.3);
  drive(b, 1.4);
  return finish(b, 0.05, 0.4);
};

made.truck_engine = () => {
  const b = buffer(3.0);
  const shape = (t) => smooth(t / 0.25) * smooth((3.0 - t) / 0.6);
  engine(b, 0, 3.0, 0, 1, { f0: 47, harmonics: 7, tone: 700, rev: 0.25, rough: 0.2, shape });
  add(b, 0, 3.0, 0.1, (t) => Math.sin(TAU * (400 + 130 * smooth(t / 1.6)) * t) * 0.025 * shape(t)); // the whine of the gears
  drive(b, 1.3);
  return finish(b, 0.05, 0.4);
};

made.halftrack = () => {
  const b = buffer(3.2);
  const shape = (t) => smooth(t / 0.25) * smooth((3.2 - t) / 0.6);
  engine(b, 0, 3.2, 0, 1, { f0: 42, harmonics: 8, tone: 620, rev: 0.2, shape });
  tracks(b, 3.2, 0.22);
  drive(b, 1.3);
  return finish(b, 0.05, 0.4);
};

made.marching = () => {
  const b = buffer(3.2);
  for (let step = 0; step < 6; step++) {
    for (let man = 0; man < 10; man++) {
      const at = 0.1 + step * 0.5 + between(-0.03, 0.03);
      const bp = bandpass(between(1200, 2200), 1.2), g = between(0.25, 0.5);
      add(b, at, 0.16, (step % 2 ? 0.35 : -0.35) + between(-0.3, 0.3), (t) => bp(noise()) * Math.exp(-t / 0.03) * g + Math.sin(TAU * 88 * t) * Math.exp(-t / 0.04) * g * 0.8);
    }
    for (let k = 0; k < 3; k++) { // the rattle of kit
      const bp = bandpass(between(4200, 6500), 4);
      add(b, 0.1 + step * 0.5 + between(0.05, 0.4), 0.03, between(-0.6, 0.6), (t) => bp(noise()) * Math.exp(-t / 0.006) * 0.12);
    }
  }
  reverb(b, 0.5, 0.12);
  return finish(b, 0.05, 0.4);
};

made.horses = () => {
  const b = buffer(2.8);
  for (const [offset, pan] of [[0, -0.3], [0.17, 0.35]]) {
    for (let stride = 0; stride < 6; stride++) {
      for (const beat of [0, 0.1, 0.2]) {
        const at = 0.08 + offset + stride * 0.42 + beat + between(-0.01, 0.01);
        const bp = bandpass(between(800, 1100), 3);
        let phase = 0;
        add(b, at, 0.1, pan, (t) => { phase += TAU * (420 + 320 * Math.exp(-t / 0.01)) / SR; return Math.sin(phase) * Math.exp(-t / 0.02) * 0.6 + bp(noise()) * Math.exp(-t / 0.015) * 0.7; });
      }
    }
  }
  reverb(b, 0.5, 0.12);
  return finish(b, 0.03, 0.3);
};

made.fighter_pass = () => {
  const b = buffer(3.8), mid = 1.9;
  const lpL = lowpass(2600), lpR = lowpass(2600), wind = bandpass(1500, 0.6);
  let phase = 0;
  for (let i = 0; i < b.n; i++) {
    const t = i / SR;
    const f = 108 * (1 - 0.13 * Math.tanh((t - mid) * 2.2)); // the note falls as it goes by
    phase += TAU * f / SR;
    let v = 0;
    for (let h = 1; h <= 12; h++) v += Math.sin(h * phase) / h;
    v *= 1 + 0.35 * Math.sin(phase * 0.5);
    const near = 1 / (1 + Math.pow((t - mid) / 0.55, 2));
    v = v * near + wind(noise()) * near * 0.5;
    const pan = Math.tanh((t - mid) * 1.6) * 0.85;
    b.L[i] = lpL(v) * Math.cos((pan + 1) * Math.PI / 4);
    b.R[i] = lpR(v) * Math.sin((pan + 1) * Math.PI / 4);
  }
  reverb(b, 1.0, 0.15);
  return finish(b, 0.2, 0.5);
};

made.bomber_drone = () => {
  const b = buffer(5.0);
  const shape = (t) => Math.pow(Math.sin(Math.PI * t / 5.0), 0.7);
  [[62.0, -0.7], [62.9, -0.25], [61.2, 0.25], [63.6, 0.7]].forEach(([f0, pan]) => {
    engine(b, 0, 5.0, pan, 0.6, { f0, harmonics: 8, tone: 650, rev: 0, rough: 0.25, shape });
  });
  drive(b, 1.2);
  reverb(b, 1.2, 0.15, 1500);
  return finish(b, 0.3, 0.6);
};

made.ship_guns = () => {
  const b = buffer(4.8);
  [0.02, 0.24, 0.5].forEach((at, k) => boom(b, at, -0.4 + k * 0.4, 1, { from: 75, to: 24, ring: 0.7, bang: 0.28, tone: 600 }));
  boom(b, 1.5, 0, 0.18, { from: 55, to: 24, ring: 0.8, bang: 0.4, tone: 300 }); // rolling back across the water
  drive(b, 1.7);
  reverb(b, 3.4, 0.45, 700);
  return finish(b);
};

made.ship_horn = () => {
  const b = buffer(2.8);
  for (const [f, pan] of [[116.5, -0.15], [146.8, 0.15]]) {
    const lp = lowpass(900);
    let phase = 0;
    add(b, 0, 2.3, pan, (t) => {
      phase += TAU * f * (1 + 0.004 * Math.sin(TAU * 4 * t)) / SR;
      let v = 0;
      for (let h = 1; h <= 10; h++) v += Math.sin(h * phase) / h;
      return lp(v) * smooth(t / 0.18) * smooth((2.3 - t) / 0.4) * 0.6;
    });
  }
  reverb(b, 1.6, 0.3, 1200);
  return finish(b, 0.02, 0.3);
};

made.siren = () => {
  const b = buffer(5.5);
  const lp = lowpass(2200);
  let p1 = 0, p2 = 0;
  add(b, 0, 5.2, 0, (t) => {
    const f = 250 + 260 * (smooth(t / 2.0) - 0.75 * smooth((t - 3.0) / 2.2));
    p1 += TAU * f / SR;
    p2 += TAU * f * 1.2 / SR;
    const voice = (p) => Math.sin(p) + 0.4 * Math.sin(2 * p) + 0.2 * Math.sin(3 * p);
    return lp(voice(p1) + voice(p2)) * smooth(t / 0.4) * smooth((5.2 - t) / 0.8) * 0.4;
  });
  reverb(b, 2.0, 0.35, 2000);
  return finish(b, 0.1, 0.4);
};

made.bell = () => {
  const b = buffer(3.2);
  [[0.5, 0.5, 3.0], [1.0, 1.0, 2.2], [1.2, 0.6, 1.6], [1.5, 0.5, 1.3], [2.0, 0.7, 1.0], [2.5, 0.35, 0.7], [3.0, 0.3, 0.5], [4.2, 0.2, 0.3]].forEach(([ratio, amp, decay]) => {
    add(b, 0.01, 3.1, 0, (t) => Math.sin(TAU * 392 * ratio * t) * amp * Math.exp(-t / (decay * 0.45)));
  });
  reverb(b, 1.5, 0.2);
  return finish(b, 0.002, 0.3);
};

made.click = () => {
  const b = buffer(0.09);
  const bp = bandpass(3000, 1.5);
  add(b, 0.002, 0.08, 0, (t) => bp(noise()) * Math.exp(-t / 0.004) * 1.5 + Math.sin(TAU * 1200 * t) * Math.exp(-t / 0.01) * 0.5);
  return finish(b, 0.001, 0.02, 0.6);
};

made.tick = () => {
  // The pointer crossing a button: the lightest of touches, hardly more than a breath.
  const b = buffer(0.045);
  const bp = bandpass(3600, 2.0);
  add(b, 0.001, 0.04, 0, (t) => bp(noise()) * Math.exp(-t / 0.0025) * 0.9 + Math.sin(TAU * 2100 * t) * Math.exp(-t / 0.006) * 0.25);
  return finish(b, 0.0005, 0.012, 0.16);
};

made.stamp = () => {
  const b = buffer(0.4);
  const bp = bandpass(2500, 0.8);
  let phase = 0;
  add(b, 0.005, 0.38, 0, (t) => { phase += TAU * (60 + 70 * Math.exp(-t / 0.02)) / SR; return Math.sin(phase) * Math.exp(-t / 0.05) + bp(noise()) * Math.exp(-t / 0.02) * 0.7; });
  return finish(b, 0.001, 0.05, 0.8);
};

made.coins = () => {
  const b = buffer(1.0);
  for (let k = 0; k < 6; k++) {
    const f = between(3800, 6200);
    clank(b, 0.02 + k * 0.09 + between(0, 0.05), between(-0.5, 0.5), between(0.4, 0.8), [f, f * 1.47, f * 2.4], 0.07);
  }
  return finish(b, 0.001, 0.1, 0.7);
};

made.hammer = () => {
  const b = buffer(1.4);
  [0.02, 0.37, 0.72].forEach((at, k) => clank(b, at, -0.2 + k * 0.2, 1 - k * 0.1, [820, 1340, 2150, 3420, 5100], 0.11));
  reverb(b, 0.8, 0.2);
  return finish(b, 0.001, 0.2);
};

made.radio = () => {
  const b = buffer(2.6);
  const hiss = bandpass(1800, 0.5), band = bandpass(1300, 0.6);
  add(b, 0, 2.6, 0, (t) => hiss(noise()) * 0.25 * (0.7 + 0.3 * Math.sin(TAU * 0.8 * t)) + (rnd() < 0.0015 ? noise() * 1.2 : 0));
  // A message in dots and dashes (it spells the game's initials).
  let at = 0.25;
  const dot = 0.065;
  for (const symbol of '- / -.-.') {
    if (symbol === '/') { at += dot * 4; continue; }
    if (symbol === ' ') { at += dot * 2; continue; }
    const len = symbol === '-' ? dot * 3 : dot;
    add(b, at, len, 0, (t) => band(Math.sin(TAU * 720 * t)) * smooth(t / 0.005) * smooth((len - t) / 0.005) * 2.0);
    at += len + dot;
  }
  for (const symbol of '... -') {
    if (symbol === ' ') { at += dot * 2; continue; }
    const len = symbol === '-' ? dot * 3 : dot;
    add(b, at + 0.3, len, 0, (t) => band(Math.sin(TAU * 720 * t)) * smooth(t / 0.005) * smooth((len - t) / 0.005) * 2.0);
    at += len + dot;
  }
  return finish(b, 0.05, 0.2, 0.7);
};

made.bugle = () => {
  // An original call: up the notes of the open horn and held at the top.
  const b = buffer(2.4);
  [[0.0, 0.11, 67], [0.14, 0.11, 72], [0.28, 0.11, 76], [0.42, 0.34, 79], [0.84, 0.11, 76], [0.98, 0.11, 79], [1.12, 0.7, 84]].forEach(([at, len, note]) => brass(b, 0.03 + at, len, 0, 1, note));
  reverb(b, 1.4, 0.28);
  return finish(b, 0.002, 0.3);
};

made.victory = () => {
  const b = buffer(3.0);
  [[0.0, 0.16, [60, 64, 67]], [0.2, 0.16, [60, 64, 67]], [0.4, 0.22, [64, 67, 72]], [0.7, 1.3, [67, 72, 76, 84]]].forEach(([at, len, notes]) => {
    notes.forEach((note, k) => brass(b, 0.03 + at, len, -0.4 + k * 0.27, 0.7, note));
  });
  reverb(b, 1.8, 0.3);
  return finish(b, 0.002, 0.4);
};

made.defeat = () => {
  const b = buffer(3.2);
  [[0.0, 0.6, [57, 60]], [0.7, 0.6, [53, 57]], [1.4, 1.3, [50, 53, 45]]].forEach(([at, len, notes]) => {
    notes.forEach((note, k) => brass(b, 0.03 + at, len, -0.3 + k * 0.3, 0.6, note));
  });
  for (const ch of [b.L, b.R]) { const lp = lowpass(900); for (let i = 0; i < b.n; i++) ch[i] = lp(ch[i]); }
  reverb(b, 2.2, 0.35, 1200);
  return finish(b, 0.002, 0.5);
};

made.drums = () => {
  const b = buffer(2.0);
  for (let t = 0.03; t < 1.5; t += 0.052) { // a roll on the side drum, growing
    const bp = bandpass(between(1500, 2200), 0.8), g = 0.25 + 0.75 * (t / 1.5);
    add(b, t, 0.06, between(-0.15, 0.15), (u) => bp(noise()) * Math.exp(-u / 0.016) * g + Math.sin(TAU * 190 * u) * Math.exp(-u / 0.02) * g * 0.5);
  }
  boom(b, 1.55, 0, 0.9, { from: 90, to: 50, ring: 0.18, bang: 0.03, tone: 500 }); // and the bass drum to end it
  reverb(b, 0.9, 0.2);
  return finish(b, 0.002, 0.2);
};

made.newsreel = () => {
  // The band at the head of a newsreel: a short march figure on the brass, a drum under it.
  const b = buffer(3.6);
  [[0.0, 0.14, [67, 71, 74]], [0.18, 0.14, [67, 71, 74]], [0.36, 0.14, [67, 71, 74]], [0.54, 0.4, [72, 76, 79]],
    [1.0, 0.18, [71, 74, 79]], [1.22, 0.18, [72, 76, 81]], [1.44, 0.2, [74, 79, 83]], [1.72, 1.4, [72, 79, 84, 88]]].forEach(([at, len, notes]) => {
    notes.forEach((note, k) => brass(b, 0.04 + at, len, -0.45 + k * 0.3, 0.62, note));
  });
  for (const at of [0.0, 0.18, 0.36, 0.54, 1.0, 1.22, 1.44, 1.72]) {
    const bp = bandpass(1900, 0.9);
    add(b, 0.04 + at, 0.07, 0, (u) => bp(noise()) * Math.exp(-u / 0.02) * 0.4 + Math.sin(TAU * 180 * u) * Math.exp(-u / 0.025) * 0.3);
  }
  boom(b, 1.76, 0, 0.7, { from: 90, to: 48, ring: 0.2, bang: 0.03, tone: 500 });
  // The thin sound of an old loudspeaker: the bottom and the top taken off.
  for (const ch of [b.L, b.R]) { const hp = highpass(320), lp = lowpass(3400); for (let i = 0; i < b.n; i++) ch[i] = lp(hp(ch[i])) * 1.5; }
  reverb(b, 1.2, 0.18);
  return finish(b, 0.002, 0.5);
};

made.projector = () => {
  // A film projector running: the claw pulling the film down sixteen times a second, and the motor's hum.
  const b = buffer(6.0);
  const shape = (t) => smooth(t / 0.4) * smooth((6.0 - t) / 0.8);
  for (let t = 0.02; t < 5.95; t += 1 / 16 + between(-0.002, 0.002)) {
    const bp = bandpass(between(2300, 2900), 2.5), g = between(0.5, 0.8) * shape(t);
    add(b, t, 0.03, between(-0.2, 0.2), (u) => bp(noise()) * Math.exp(-u / 0.006) * g);
    add(b, t + 0.012, 0.03, between(-0.2, 0.2), (u) => Math.sin(TAU * 410 * u) * Math.exp(-u / 0.008) * g * 0.25);
  }
  add(b, 0, 6.0, 0, (t) => (Math.sin(TAU * 96 * t) * 0.12 + Math.sin(TAU * 192 * t) * 0.05 + Math.sin(TAU * 48 * t) * 0.08) * shape(t));
  { const lp = lowpass(1400); add(b, 0, 6.0, 0, (t) => lp(noise()) * 0.05 * shape(t)); }
  return finish(b, 0.05, 0.6, 0.6);
};

made.battle = () => {
  // A fight heard from a little way off: rifles, a machine gun, shells bursting.
  const b = buffer(4.4);
  for (let k = 0; k < 26; k++) shot(b, between(0.05, 3.6), between(-0.8, 0.8), between(0.25, 0.7));
  let t = 0.6;
  for (let k = 0; k < 11; k++) { shot(b, t, -0.4, 0.55, { bright: 1900, crack: 0.012, thump: 0.03 }); t += 0.085; }
  t = 2.3;
  for (let k = 0; k < 9; k++) { shot(b, t, 0.5, 0.5, { bright: 2000, crack: 0.012, thump: 0.03 }); t += 0.09; }
  boom(b, 0.4, 0.3, 0.8, { from: 95, to: 32, ring: 0.45, bang: 0.2, tone: 900 });
  boom(b, 1.7, -0.5, 0.9, { from: 100, to: 30, ring: 0.5, bang: 0.22, tone: 1000 });
  boom(b, 3.0, 0.1, 0.7, { from: 90, to: 30, ring: 0.5, bang: 0.2, tone: 800 });
  drive(b, 1.5);
  reverb(b, 1.8, 0.32, 1800);
  return finish(b, 0.01, 0.6);
};

// ---- Writing them out -------------------------------------------------------------------------

function wav(b) {
  const data = Buffer.alloc(44 + b.n * 4);
  data.write('RIFF', 0); data.writeUInt32LE(36 + b.n * 4, 4); data.write('WAVEfmt ', 8);
  data.writeUInt32LE(16, 16); data.writeUInt16LE(1, 20); data.writeUInt16LE(2, 22);
  data.writeUInt32LE(SR, 24); data.writeUInt32LE(SR * 4, 28); data.writeUInt16LE(4, 32); data.writeUInt16LE(16, 34);
  data.write('data', 36); data.writeUInt32LE(b.n * 4, 40);
  for (let i = 0; i < b.n; i++) {
    data.writeInt16LE(Math.round(Math.max(-1, Math.min(1, b.L[i])) * 32767), 44 + i * 4);
    data.writeInt16LE(Math.round(Math.max(-1, Math.min(1, b.R[i])) * 32767), 46 + i * 4);
  }
  return data;
}

const out = process.argv[2];
if (!out) {
  console.error('usage: node make_sounds.mjs <out-folder>');
  process.exit(1);
}
fs.mkdirSync(out, { recursive: true });
for (const [name, make] of Object.entries(made)) {
  const b = make();
  let peak = 0, sum = 0;
  for (let i = 0; i < b.n; i++) { peak = Math.max(peak, Math.abs(b.L[i]), Math.abs(b.R[i])); sum += b.L[i] * b.L[i] + b.R[i] * b.R[i]; }
  const rms = Math.sqrt(sum / (2 * b.n));
  fs.writeFileSync(path.join(out, name + '.wav'), wav(b));
  console.log(`${name.padEnd(14)} ${(b.n / SR).toFixed(2)} s  peak ${(20 * Math.log10(peak)).toFixed(1)} dB  level ${(20 * Math.log10(rms)).toFixed(1)} dB`);
}
