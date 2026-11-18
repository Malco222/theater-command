// Ambience. Two kinds of thing live here.
//
// Beds: long stereo loops of pure texture (the air of the war room, wind over open country, the sea, rain, crickets,
// the guns muttering on the horizon). A bed has nothing in it that the ear can pick out and wait for, which is what
// lets it go round all evening without grating.
//
// Sprinkles: the single events that make a place alive (a bird, an owl, a gull, a far salvo, steps past the door).
// These are separate short sounds, and the game drops one in at random every so often, never the same one twice
// running. Baked into a loop they would come round like a stuck record; sprinkled they never repeat.

import {
  SR, TAU, sec, db, clamp, lerp, smooth, rng, mono, stereo, add, place, gain, mul, line, decay, fade, wander,
  white, pink, brown, sine, buzz, svf, lp, hp, bp, lp4, eq, comb, drive, inPlace, far, loopOf, toMono, lengthOf, around,
} from '../kit/dsp.mjs';
import { bell, knock, tick, swish, wind, glide, clang } from '../kit/parts.mjs';
import { fieldGun, crump } from './guns.mjs';

// ---- Making things that go round ---------------------------------------------------------------------------

/** Puts an event into a loop: what runs off the end comes round to the beginning. */
function wrapPlace(dst, src, at, pan = 0, g = 1) {
  const n = dst.L.length, n0 = ((sec(at) % n) + n) % n;
  const gl = g * Math.cos((pan + 1) * Math.PI / 4), gr = g * Math.sin((pan + 1) * Math.PI / 4);
  const m = Math.min(src.length, n);
  for (let i = 0; i < m; i++) { const j = (n0 + i) % n; dst.L[j] += src[i] * gl; dst.R[j] += src[i] * gr; }
  return dst;
}
/** A steady layer (wind, sea, hiss) made a little too long and folded on itself, so that it has no join. */
function layer(make, dur, overlap = 2.5) {
  const y = loopOf(make(dur + overlap), overlap), n = sec(dur);
  return y.length === n ? y : y.slice(0, n);
}
/** The echo of a place for something that goes round: the end's echo is heard over the beginning. */
function loopInPlace(x, name, wet) {
  const n = lengthOf(x);
  const twice = { L: new Float32Array(n * 2), R: new Float32Array(n * 2) };
  twice.L.set(x.L); twice.L.set(x.L, n); twice.R.set(x.R); twice.R.set(x.R, n);
  const w = inPlace(twice, name, wet);
  return { L: w.L.slice(n, n * 2), R: w.R.slice(n, n * 2) };
}

// ---- Living things -----------------------------------------------------------------------------------------

/** One note of birdsong: a whistle that slides. */
function chirp(r, f1, f2, dur, { harm = 0.1, trem = 0, tremRate = 30, bend = 1 } = {}) {
  const n = sec(dur), f = new Float32Array(n), out = new Float32Array(n);
  for (let i = 0; i < n; i++) f[i] = f1 + (f2 - f1) * Math.pow(i / n, bend);
  const a = sine(n, f), b = sine(n, f.map((v) => v * 2));
  for (let i = 0; i < n; i++) {
    const u = i / n;
    out[i] = (a[i] + b[i] * harm) * Math.pow(Math.sin(Math.PI * u), 0.7) * (1 - trem * (0.5 + 0.5 * Math.sin(TAU * tremRate * i / SR)));
  }
  return out;
}
/** A phrase from a small songbird: a handful of quick notes in its own part of the scale. */
function songPhrase(r, { lo = 2800, hi = 5200, notes = null } = {}) {
  const k = notes ?? r.int(4, 9), out = mono(k * 0.16 + 0.2);
  let t = 0.01;
  for (let i = 0; i < k; i++) {
    const f = r.range(lo, hi), d = r.range(0.035, 0.11), kind = r.f();
    const to = kind < 0.4 ? f * r.range(1.12, 1.45) : kind < 0.8 ? f * r.range(0.7, 0.9) : f;
    add(out, chirp(r, f, Math.min(to, hi * 1.3), d, { trem: r.chance(0.25) ? 0.7 : 0, tremRate: r.range(35, 60) }), t, r.range(0.6, 1));
    t += d + r.range(0.02, 0.08);
  }
  return out.slice(0, sec(t + 0.05));
}
const twoNote = (r, hiF = 3300, loF = 2650) => { const out = mono(0.75); add(out, chirp(r, hiF, hiF * 0.98, 0.2), 0.01, 1); add(out, chirp(r, loF, loF * 0.96, 0.3), 0.3, 0.9); return out; };
const trill = (r, f = 4300, count = 14, rate = 24) => { const out = mono(count / rate + 0.1); for (let i = 0; i < count; i++) add(out, chirp(r, f * r.range(0.98, 1.02), f * 1.12, 0.024), 0.005 + i / rate, lerp(0.5, 1, Math.sin(Math.PI * i / count))); return out; };
/** A crow: not a whistle but a rasp. */
function caw(r, f0 = 520) {
  const n = sec(0.26), pitch = new Float32Array(n);
  for (let i = 0; i < n; i++) pitch[i] = f0 * (1 + 0.1 * Math.sin(Math.PI * i / n)) * (1 - 0.12 * i / n);
  const src = buzz(n, pitch, 'pulse', 0.2);
  const noisy = white(n, r);
  for (let i = 0; i < n; i++) src[i] = src[i] * (0.7 + 0.6 * noisy[i]);
  const out = bp(src, 1650, 2.2); add(out, bp(src, 2700, 3), 0, 0.5);
  return mul(out, line(n, [[0, 0], [0.03, 1], [0.18, 0.8], [0.26, 0]]));
}
/** An owl's hoot: a soft round note with a little breath in it. */
function hoot(r, f = 345, dur = 0.3, shake = 0) {
  const n = sec(dur + 0.1), pitch = new Float32Array(n);
  for (let i = 0; i < n; i++) { const t = i / SR; pitch[i] = f * (1 + 0.03 * Math.sin(Math.PI * Math.min(1, t / dur))) * (1 - 0.04 * smooth((t - dur * 0.6) / (dur * 0.5))) * (1 + shake * 0.012 * Math.sin(TAU * 9 * t)); }
  const out = sine(n, pitch); add(out, sine(n, pitch.map((v) => v * 2)), 0, 0.12); add(out, svf(white(n, r), 'bp', pitch, 12), 0, 0.5);
  const env = line(n, [[0, 0], [0.07, 1], [dur * 0.7, 0.85], [dur + 0.1, 0]], 1.3);
  for (let i = 0; i < n; i++) out[i] *= env[i] * (1 - shake * 0.35 * (0.5 + 0.5 * Math.sin(TAU * 9 * i / SR)));
  return out;
}
/** A gull's cry: up, over and down, with a crack in the voice. */
function gullCry(r, dur = 0.34, f0 = 1500) {
  const n = sec(dur), pitch = new Float32Array(n);
  for (let i = 0; i < n; i++) { const u = i / n; pitch[i] = f0 * (1 + 0.55 * Math.sin(Math.PI * Math.pow(u, 0.6))) * (1 - 0.2 * u); }
  const src = buzz(n, pitch, 'saw');
  const out = bp(src, 2300, 1.6); add(out, bp(src, 3600, 2.5), 0, 0.5);
  const rough = wander(n, 45, r, 0.5);
  return mul(out, line(n, [[0, 0], [0.03, 1], [dur * 0.7, 0.8], [dur, 0]]).map((e, i) => e * rough[i]));
}
/** One cricket's chirp: three or four quick strokes of its wing. */
function cricketChirp(r, fc, strokes = 3) {
  const out = mono(0.03 * strokes + 0.02), m = sec(0.011);
  for (let k = 0; k < strokes; k++) {
    const p = sine(m, fc * r.range(0.995, 1.005));
    for (let i = 0; i < m; i++) p[i] *= Math.pow(Math.sin(Math.PI * i / m), 2);
    add(out, p, 0.002 + k * 0.024, k === 0 ? 0.7 : 1);
  }
  return out;
}
/** A field of crickets that goes round: each sings at its own pitch and pace, and swells and rests. */
function crickets(r, dur, { count = 6, lo = 3700, hi = 5100, level = 1 } = {}) {
  const out = stereo(dur);
  for (let c = 0; c < count; c++) {
    const fc = r.range(lo, hi), period = dur / Math.round(dur / r.range(0.36, 0.62)), pan = r.range(-0.9, 0.9), g = r.range(0.35, 1) * level;
    const waves = r.int(1, 3), phase = r.f(), strokes = r.int(3, 4), off = r.f() * period;
    for (let t = off; t < dur; t += period) {
      const swell = Math.pow(Math.sin(Math.PI * (t / dur * waves + phase)), 2);
      if (swell < 0.12) continue; // it rests
      wrapPlace(out, cricketChirp(r, fc, strokes), t + r.range(-0.006, 0.006), pan, g * (0.35 + 0.65 * swell));
    }
  }
  return out;
}
/** A frog: a short dry rattle. */
function croak(r, f = 850, dur = 0.16, rate = 30) {
  const n = sec(dur), out = sine(n, f); add(out, sine(n, f * 1.52), 0, 0.4);
  for (let i = 0; i < n; i++) out[i] *= Math.pow(0.5 + 0.5 * Math.sin(TAU * rate * i / SR), 3) * Math.sin(Math.PI * i / n);
  return out;
}

// ---- Weather and water -------------------------------------------------------------------------------------

/** One wave coming in and drawing back. */
function wave(r, size = 1) {
  const dur = 6.5 * r.range(0.85, 1.15), n = sec(dur), tone = new Float32Array(n), env = new Float32Array(n);
  const rise = r.range(1.4, 2.1);
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    tone[i] = t < rise ? lerp(260, 1500 * size, smooth(t / rise)) : lerp(1500 * size, 2600, smooth((t - rise) / (dur - rise)));
    env[i] = t < rise ? Math.pow(smooth(t / rise), 1.6) : Math.exp(-(t - rise) / 1.5) * (1 - smooth((t - dur + 1) / 1));
  }
  const out = svf(pink(n, r), 'bp', tone, 0.7);
  const foam = hp(white(n, r), 3200);
  for (let i = 0; i < n; i++) out[i] = (out[i] + foam[i] * 0.05 * smooth((i / SR - rise * 0.8) / 0.6)) * env[i];
  return out;
}
/** Rain, as it goes round: a great many drops, and the steady rush of the rest. */
function rain(r, dur, { drops = 260, heavy = 1 } = {}) {
  const out = stereo(dur), n = sec(dur);
  const count = Math.round(drops * dur);
  for (let k = 0; k < count; k++) {
    const m = sec(r.range(0.002, 0.006)), d = bp(white(m, r), r.range(1800, 7500), r.range(1, 3));
    for (let i = 0; i < m; i++) d[i] *= Math.exp(-i / (m * 0.3));
    wrapPlace(out, d, r.f() * dur, r.range(-1, 1), r.range(0.1, 0.6) * (r.chance(0.03) ? 2.2 : 1));
  }
  for (const pn of [-0.7, 0.7]) {
    place(out, layer((d) => { const x = hp(white(sec(d), r), 2400); add(x, bp(pink(sec(d), r), 620, 0.5), 0, 2.2 * heavy); return mul(x, wander(sec(d), 0.25, r, 0.3)); }, dur), 0, pn, 0.16 * heavy);
  }
  return out;
}
/** Thunder a long way off: it cracks dully, then rolls about the sky. */
function thunder(r, { dur = 9, near = 0.3 } = {}) {
  const n = sec(dur), out = mono(dur);
  const roll = lp4(brown(n, r), r.range(90, 140)), bumps = wander(n, 1.7, r, 1), tail = line(n, [[0, 0], [0.25, 1], [1.2, 0.8], [dur * 0.6, 0.35], [dur, 0]]);
  for (let i = 0; i < n; i++) out[i] = roll[i] * 3 * Math.pow(bumps[i], 1.6) * tail[i];
  const mid = bp(brown(n, r), 260, 0.6), bumps2 = wander(n, 3.1, r, 1);
  for (let i = 0; i < n; i++) out[i] += mid[i] * 1.2 * Math.pow(bumps2[i], 2.5) * tail[i];
  for (let k = 0; k < 6; k++) { const m = sec(0.25), c = mul(bp(white(m, r), r.range(350, 1300), 1), decay(m, r.range(0.02, 0.06), 0.002)); add(out, c, 0.2 + r.range(0, 0.9), near * r.range(0.4, 1)); }
  return out;
}

// ---- The room ----------------------------------------------------------------------------------------------

const clockTick = (r, tock) => { const o = mono(0.12); add(o, tick(r, { tone: tock ? 1700 : 2150, body: tock ? 520 : 640, tau: 0.006 }), 0.001, 1); add(o, knock(r, { f: tock ? [210, 380] : [250, 430], tau: 0.018, dur: 0.1, click: 0.1 }), 0.001, 0.35); return o; };
const farKey = (r) => { const o = mono(0.1); add(o, tick(r, { tone: r.range(2300, 3100), body: r.range(650, 900), tau: 0.0035 }), 0, 1); add(o, knock(r, { f: [r.range(260, 330), 610], tau: 0.012, dur: 0.09, click: 0.3 }), 0.004, 0.6); return o; };
/** Somebody typing: bursts of keys with the little halts of a person thinking. */
function typing(r, dur, rate = 7.5) {
  const out = mono(dur + 0.2);
  let t = 0.02;
  while (t < dur) { add(out, farKey(r), t, r.range(0.6, 1)); t += (1 / rate) * r.range(0.6, 1.5) + (r.chance(0.12) ? r.range(0.2, 0.5) : 0); }
  return out;
}
const roomTone = (r, d, dark = 1) => { const n = sec(d), x = gain(hp(lp(brown(n, r), 150 * dark), 60), 0.12); add(x, bp(pink(n, r), 420 * dark, 0.5), 0, 0.06); return x; };
/** The weight under a bed: a little low air, never a rumble. */
const low = (r, d, f = 120) => hp(lp(brown(sec(d), r), f), 55);
/** Last thing done to a bed: nothing below what a speaker can use. */
const bed = (x) => around(x, (c) => hp(c, 45));

export default function (def) {
  const B = { loop: true, ch: 2 };

  // ---- Beds: the war room --------------------------------------------------------------------------------------
  def('room_day_loop', { ...B, lufs: -33, about: 'The war room by day: the air of a big panelled room, the wall clock, somebody typing two rooms away. Heard when the camera is high above the map, and under every card.' }, (r) => {
    const dur = 24, dry = stereo(dur);
    for (let k = 0; k < dur; k++) wrapPlace(dry, clockTick(r, k % 2), k + 0.5, -0.25, 0.5);
    for (const [t, len] of [[2.2, 2.6], [8.9, 1.4], [13.1, 3.4], [20.3, 1.9]]) wrapPlace(dry, lp(typing(r, len), 1900), t, 0.6, 0.11);
    const out = loopInPlace(dry, 'room', -9);
    for (const pn of [-0.6, 0.6]) place(out, layer((d) => roomTone(r, d), dur), 0, pn, 0.5);
    return bed(out);
  });
  def('room_night_loop', { ...B, lufs: -34, about: 'The war room at night: the staff gone home, the clock, the lamp hissing, wind at the window. The same room, emptier.' }, (r) => {
    const dur = 24, dry = stereo(dur);
    for (let k = 0; k < dur; k++) wrapPlace(dry, clockTick(r, k % 2), k + 0.5, -0.25, 0.5);
    const out = loopInPlace(dry, 'room', -8);
    for (const pn of [-0.6, 0.6]) place(out, layer((d) => roomTone(r, d, 0.8), dur), 0, pn, 0.45);
    place(out, layer((d) => lp(wind(r, d, { lo: 170, hi: 560, q: 1.6, gust: 0.14, howl: 0.5, hissy: 0 }), 900), dur, 3), 0, -0.55, 0.5);
    place(out, layer((d) => mul(bp(white(sec(d), r), 5200, 1.2), wander(sec(d), 0.4, r, 0.25)), dur), 0, 0.35, 0.012); // the lamp
    return bed(out);
  });

  // ---- Beds: the country ---------------------------------------------------------------------------------------
  def('land_day_loop', { ...B, about: 'Open country by day: wind in grass and trees, and birdsong a long way off, too far to pick one bird from another. Heard when the camera comes down over land in daylight.' }, (r) => {
    const dur = 28, dry = stereo(dur);
    for (let b = 0; b < 5; b++) {
      const lo = r.range(2400, 3800), hi = lo * r.range(1.3, 1.7), pan = r.range(-0.9, 0.9), g = r.range(0.25, 0.6), dist = r.range(120, 380);
      for (let t = r.range(0, 4); t < dur; t += r.range(2.2, 7)) wrapPlace(dry, far(songPhrase(r, { lo, hi }), dist), t, pan, g * r.range(0.6, 1));
    }
    const out = loopInPlace(dry, 'field', -10);
    gain(out, 0.3);
    for (const pn of [-0.75, 0.75]) place(out, layer((d) => wind(r.fork(), d, { lo: 300, hi: 1150, q: 0.9, gust: 0.16, depth: 0.65, hissy: 0.9 }), dur, 3), 0, pn, 0.5);
    place(out, layer((d) => low(r, d), dur), 0, 0, 0.06);
    return bed(out);
  });
  def('land_night_loop', { ...B, lufs: -31, about: 'Open country by night: crickets, and a little wind. No birds. The change from the day bed is the plainest sign that night has come.' }, (r) => {
    const dur = 28, out = crickets(r, dur, { count: 7 });
    gain(out, 0.12);
    const wet = loopInPlace(out, 'field', -14);
    for (const pn of [-0.75, 0.75]) place(wet, layer((d) => wind(r.fork(), d, { lo: 200, hi: 620, q: 1.1, gust: 0.1, depth: 0.6, hissy: 0.2 }), dur, 3), 0, pn, 0.4);
    place(wet, layer((d) => low(r, d, 110), dur), 0, 0, 0.05);
    return bed(wet);
  });
  def('winter_loop', { ...B, about: 'Winter: a hard wind that finds every gap, and fine snow driven on it. No life in it at all. For snow-covered country, by day or night.' }, (r) => {
    const dur = 28, out = stereo(dur);
    for (const pn of [-0.75, 0.75]) place(out, layer((d) => wind(r.fork(), d, { lo: 220, hi: 1250, q: 1.5, gust: 0.22, depth: 0.8, howl: 0.9, hissy: 0.6 }), dur, 3), 0, pn, 0.6);
    place(out, layer((d) => { const n = sec(d), g = wander(n, 0.3, r, 1), s = hp(white(n, r), 5200); for (let i = 0; i < n; i++) s[i] *= g[i] * g[i]; return s; }, dur), 0, 0.2, 0.03);
    place(out, layer((d) => low(r, d, 130), dur), 0, 0, 0.08);
    return bed(out);
  });
  def('desert_loop', { ...B, about: 'Desert: a dry steady wind, and sand hissing along the ground. For North Africa and the like.' }, (r) => {
    const dur = 28, out = stereo(dur);
    for (const pn of [-0.75, 0.75]) place(out, layer((d) => wind(r.fork(), d, { lo: 150, hi: 520, q: 0.9, gust: 0.12, depth: 0.5, howl: 0.12, hissy: 0 }), dur, 3), 0, pn, 0.6);
    for (const pn of [-0.5, 0.6]) place(out, layer((d) => { const n = sec(d), g = wander(n, 0.45, r.fork(), 1), s = bp(white(n, r), 4600, 0.8); for (let i = 0; i < n; i++) s[i] *= Math.pow(g[i], 3); return s; }, dur), 0, pn, 0.05);
    return bed(out);
  });
  def('jungle_loop', { ...B, about: 'Jungle: a wall of insects, frogs, water dripping from leaves. For Burma, the islands and the Pacific shore.' }, (r) => {
    const dur = 28, dry = stereo(dur), n = sec(dur);
    for (let f = 0; f < 6; f++) {
      const pitch = r.range(650, 1150), pan = r.range(-0.9, 0.9), g = r.range(0.3, 0.7), period = dur / Math.round(dur / r.range(1.3, 2.6)), waves = r.int(1, 3), ph = r.f();
      for (let t = r.f() * period; t < dur; t += period) {
        if (Math.pow(Math.sin(Math.PI * (t / dur * waves + ph)), 2) < 0.3) continue;
        wrapPlace(dry, croak(r, pitch, 0.15), t, pan, g); wrapPlace(dry, croak(r, pitch * 1.03, 0.13), t + 0.21, pan, g * 0.8);
      }
    }
    for (let k = 0; k < 40; k++) wrapPlace(dry, lp(tick(r, { tone: r.range(1400, 2600), body: r.range(500, 900), tau: 0.006 }), 3000), r.f() * dur, r.range(-1, 1), r.range(0.2, 0.7));
    for (let k = 0; k < 5; k++) { const t = r.f() * dur, f0 = r.range(600, 900), pan = r.range(-0.8, 0.8); for (let j = 0; j < 3; j++) wrapPlace(dry, far(chirp(r, f0, f0 * 1.9, 0.22, { harm: 0.2 }), 200), t + j * 0.34, pan, 0.5); }
    const out = loopInPlace(dry, 'field', -9);
    gain(out, 0.3);
    for (const [fc, rate, pn, g] of [[7200, 46, -0.6, 0.05], [5300, 31, 0.6, 0.06], [8600, 58, 0.1, 0.03]]) {
      const wall = layer((d) => bp(white(sec(d), r), fc, 5), dur);
      for (let i = 0; i < n; i++) wall[i] *= 0.55 + 0.45 * Math.sin(TAU * Math.round(rate * dur) / dur * i / SR);
      place(out, wall, 0, pn, g * 6);
    }
    place(out, layer((d) => low(r, d, 140), dur), 0, 0, 0.05);
    return bed(out);
  });
  def('rain_loop', { ...B, about: 'Steady rain on open ground. For bad weather over land; lay it over the day or night bed, and thin the birds out.' }, (r) => bed(rain(r, 24)));

  // ---- Beds: the sea -------------------------------------------------------------------------------------------
  def('sea_day_loop', { ...B, about: 'The open sea by day: the swell lifting and falling away, and wind. Heard when the camera comes down over water.' }, (r) => {
    const dur = 30, out = stereo(dur);
    for (let t = r.range(0, 2); t < dur - 1; t += r.range(4.2, 7.5)) wrapPlace(out, wave(r, r.range(0.8, 1.2)), t, r.range(-0.8, 0.8), r.range(0.5, 1));
    for (const pn of [-0.75, 0.75]) place(out, layer((d) => { const n = sec(d), x = hp(lp4(brown(n, r), 210), 50), w = wander(n, 0.16, r.fork(), 0.6); for (let i = 0; i < n; i++) x[i] *= w[i]; return x; }, dur, 3), 0, pn, 0.3);
    for (const pn of [-0.6, 0.7]) place(out, layer((d) => wind(r.fork(), d, { lo: 260, hi: 900, q: 0.9, gust: 0.13, depth: 0.6, hissy: 0.5 }), dur, 3), 0, pn, 0.35);
    return bed(out);
  });
  def('sea_night_loop', { ...B, lufs: -31, about: 'The sea at night: the same swell, slower and darker, with less wind. The gulls are gone.' }, (r) => {
    const dur = 30, out = stereo(dur);
    for (let t = r.range(0, 2); t < dur - 1; t += r.range(6, 9.5)) wrapPlace(out, lp(wave(r, r.range(0.6, 0.85)), 1900), t, r.range(-0.8, 0.8), r.range(0.4, 0.8));
    for (const pn of [-0.75, 0.75]) place(out, layer((d) => { const n = sec(d), x = hp(lp4(brown(n, r), 170), 50), w = wander(n, 0.12, r.fork(), 0.7); for (let i = 0; i < n; i++) x[i] *= w[i]; return x; }, dur, 3), 0, pn, 0.35);
    for (const pn of [-0.6, 0.7]) place(out, layer((d) => wind(r.fork(), d, { lo: 200, hi: 600, q: 1, gust: 0.09, depth: 0.6, hissy: 0.15 }), dur, 3), 0, pn, 0.2);
    return bed(out);
  });

  // ---- Beds: the war on the horizon ----------------------------------------------------------------------------
  def('front_loop', { ...B, lufs: -31, about: 'A front line a long way off: the guns muttering on the horizon, never still and never near. Laid under the land bed when the camera is over a province where armies face each other.' }, (r) => {
    const dur = 32, dry = stereo(dur);
    const shots = 60; // spread evenly and then shaken, so that it is never silent for long and never in step
    for (let k = 0; k < shots; k++) {
      const g = lp(r.chance(0.5) ? fieldGun(r, r.range(0.9, 1.4)) : crump(r, r.range(0.9, 1.3)), r.range(180, 460));
      wrapPlace(dry, g, (k + r.f() * 1.6) * dur / shots, r.range(-0.9, 0.9), r.range(0.25, 1) * (r.chance(0.1) ? 1.5 : 1));
    }
    return bed(loopInPlace(dry, 'valley', -7));
  });

  // ---- Sprinkles: dropped in at random by the game -------------------------------------------------------------
  const S = { ch: 1, lufs: -28 };
  def('bird_01', { ...S, about: 'Sprinkle, land by day: a small songbird, one phrase, near.' }, (r) => inPlace(songPhrase(r, { lo: 2900, hi: 4600, notes: 7 }), 'field', -16));
  def('bird_02', { ...S, about: 'Sprinkle, land by day: a two-note call, twice.' }, (r) => { const o = mono(2.2); add(o, twoNote(r), 0.01, 1); add(o, twoNote(r, 3320, 2630), 1.2, 0.9); return inPlace(o, 'field', -15); });
  def('bird_03', { ...S, about: 'Sprinkle, land by day: a quick trill and an answer.' }, (r) => { const o = mono(2); add(o, trill(r, 4300, 14), 0.01, 1); add(o, trill(r, 3900, 9), 1.05, 0.7); return inPlace(o, 'field', -15); });
  def('bird_04', { ...S, about: 'Sprinkle, land by day: a crow, three times, a field away. Good over farmland and over a battlefield after the fighting.' }, (r) => { const o = mono(2); for (const [t, g] of [[0.01, 1], [0.45, 0.9], [0.92, 0.75]]) add(o, caw(r, r.range(500, 540)), t, g); return inPlace(far(o, 90), 'field', -11); });
  def('owl_01', { ...S, about: 'Sprinkle, land by night: an owl. One hoot, then three.' }, (r) => { const o = mono(3); add(o, hoot(r, 345, 0.34), 0.02, 1); for (const [t, d] of [[0.95, 0.14], [1.2, 0.14], [1.47, 0.5]]) add(o, hoot(r, 340, d), t, 0.9); return inPlace(far(o, 60), 'field', -14); });
  def('owl_02', { ...S, about: 'Sprinkle, land by night: another owl, one long shaking note.' }, (r) => inPlace(far(hoot(r, 420, 1.1, 1), 60), 'field', -14));
  def('gull_01', { ...S, about: 'Sprinkle, sea and harbour by day: a gull, three cries.' }, (r) => { const o = mono(2); for (const [t, g] of [[0.01, 1], [0.42, 0.9], [0.82, 0.7]]) add(o, gullCry(r, 0.32, r.range(1420, 1560)), t, g); return inPlace(far(o, 50), 'sea', -12); });
  def('gull_02', { ...S, about: 'Sprinkle, sea and harbour by day: a gull, one long cry and two short.' }, (r) => { const o = mono(2.2); add(o, gullCry(r, 0.62, 1380), 0.01, 1); add(o, gullCry(r, 0.24, 1600), 0.85, 0.8); add(o, gullCry(r, 0.24, 1560), 1.15, 0.7); return inPlace(far(o, 50), 'sea', -12); });
  def('bell_buoy', { ...S, about: 'Sprinkle, sea by night and harbours: a bell buoy rocking, three uneven strokes.' }, (r) => { const o = mono(5); for (const [t, g] of [[0.02, 1], [0.9, 0.6], [2.3, 0.85]]) add(o, bell(r, { f0: 740, dur: 2.6, tau: 0.9, bright: 0.6, strike: 0.5 }), t, g); return inPlace(far(o, 180), 'sea', -9); });
  def('church_bell_far', { ...S, about: 'Sprinkle, land by day in country that is at peace: a church bell across the fields, three strokes.' }, (r) => { const o = mono(8); for (const t of [0.02, 1.9, 3.8]) add(o, bell(r, { f0: 294, dur: 4, tau: 1.6, bright: 0.72, strike: 0.4 }), t, 1); return inPlace(far(o, 500), 'field', -8); });
  def('far_guns_01', { ...S, lufs: -26, about: 'Sprinkle, near a front: a far salvo, four guns, and its roll.' }, (r) => { const o = mono(6.5); for (let k = 0; k < 4; k++) add(o, lp(fieldGun(r, 1.2), 560), 0.02 + k * r.range(0.16, 0.34), r.range(0.6, 1)); return inPlace(o, 'valley', -6); });
  def('far_guns_02', { ...S, lufs: -26, about: 'Sprinkle, near a front: two heavy shells landing far off.' }, (r) => { const o = mono(6.5); add(o, lp(crump(r, 1.3), 520), 0.02, 1); add(o, lp(crump(r, 1.2), 460), 1.1, 0.8); return inPlace(o, 'valley', -6); });
  def('far_guns_03', { ...S, lufs: -26, about: 'Sprinkle, near a front: a long ragged ripple of fire, a battery firing in turn.' }, (r) => { const o = mono(8); let t = 0.02; for (let k = 0; k < 7; k++) { add(o, lp(fieldGun(r, r.range(0.9, 1.3)), r.range(420, 680)), t, r.range(0.5, 1)); t += r.range(0.25, 0.7); } return inPlace(o, 'valley', -6); });
  def('thunder_01', { ...S, lufs: -25, about: 'Sprinkle, storms: thunder a long way off, rolling.' }, (r) => inPlace(thunder(r, { dur: 9, near: 0.15 }), 'valley', -9));
  def('thunder_02', { ...S, lufs: -25, about: 'Sprinkle, storms: thunder nearer, with a dull crack before the roll.' }, (r) => inPlace(thunder(r, { dur: 8, near: 0.6 }), 'valley', -10));
  def('room_typing_01', { ...S, ch: 2, lufs: -34, about: 'Sprinkle, war room by day: a clerk typing a line next door.' }, (r) => inPlace(lp(typing(r, 2.8), 2100), 'room', -8));
  def('room_typing_02', { ...S, ch: 2, lufs: -34, about: 'Sprinkle, war room by day: a line typed, the bell, and the carriage sent back.' }, (r) => {
    const o = mono(4.2); add(o, typing(r, 2.3, 8.5), 0, 1);
    add(o, bell(r, { f0: 2350, dur: 0.6, tau: 0.16, bright: 0.5, strike: 0.5 }), 2.45, 0.35);
    add(o, swish(r, 0.42, { lo: 900, hi: 2600, rough: 0.8, grain: 320 }), 2.75, 0.9); add(o, knock(r, { f: [240, 420], tau: 0.02, click: 0.4 }), 3.16, 0.8);
    return inPlace(lp(o, 2100), 'room', -8);
  });
  def('room_steps', { ...S, ch: 2, lufs: -34, about: 'Sprinkle, war room: somebody walks past the door on a wooden floor.' }, (r) => {
    const o = stereo(4.6);
    for (let k = 0; k < 7; k++) { const u = k / 6, st = knock(r, { f: [r.range(80, 96), r.range(150, 175), 250], tau: 0.045, dur: 0.3, click: 0.35, weight: 1.4 }); place(o, lp(st, 1500), 0.05 + k * 0.56 + r.range(-0.02, 0.02), lerp(-0.8, 0.8, u), 0.35 + 0.65 * Math.sin(Math.PI * u)); }
    return inPlace(o, 'room', -8);
  });
  def('room_papers', { ...S, ch: 2, lufs: -34, about: 'Sprinkle, war room: papers gathered and squared on a desk.' }, (r) => {
    const o = mono(2.4);
    add(o, swish(r, 0.3, { lo: 1200, hi: 5200, rough: 0.7, grain: 120 }), 0.02, 0.8); add(o, swish(r, 0.22, { lo: 1500, hi: 4800, rough: 0.7, grain: 150, move: 0 }), 0.5, 0.6);
    for (const t of [1.05, 1.27]) add(o, knock(r, { f: [210, 390, 640], tau: 0.016, click: 0.5 }), t, 0.6);
    return inPlace(lp(o, 4200), 'room', -10);
  });
}
