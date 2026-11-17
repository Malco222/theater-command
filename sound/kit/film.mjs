// The mixing desk for real recordings. Nothing here makes a sound: every function takes a recording and does to it
// what a film's sound mixer does, so that a field recording of a gun or an engine carries weight and distance.

import { SR, sec, db, toDb, clamp, isStereo, lengthOf, each, gain, mul, copy, lp, hp, hp4, bp, eq, drive, compress, speed, peak, loudness, fade, loopOf, around, toMono, toStereo, add } from './dsp.mjs';

/** Applies a one-channel function to each side of a sound and returns a new sound. */
export const both = (x, fn) => (isStereo(x) ? { L: fn(x.L), R: fn(x.R) } : fn(x));

/** A cut from a longer recording: seconds "from" to "to" of samples already read. */
export function cut(x, from, to) {
  const a = sec(from), b = to == null ? lengthOf(x) : Math.min(lengthOf(x), sec(to));
  return both(x, (ch) => ch.slice(a, b));
}

/**
 * Cleans a field recording: takes out what is below the sound itself (wind, handling, traffic rumble) and, if asked,
 * the hiss above it. "low" and "high" are in Hz.
 */
export function clean(x, { low = 30, high = null } = {}) {
  let y = both(x, (ch) => hp4(ch, low));
  if (high) y = both(y, (ch) => lp(lp(ch, high, 0.6), high, 0.8));
  return y;
}

/**
 * Weight. A small speaker cannot move the air a gun moves, so a film mix does two things: it lifts the body of the
 * sound, and it lays over it the harmonics of its own low end, which a small speaker can play and the ear reads as
 * depth. "lift" is dB at "hz"; "harmonics" is how much of the pressed low end is laid back in (dB, below the sound).
 */
export function weight(x, { hz = 90, lift = 3, harmonics = -12, from = 160 } = {}) {
  let y = both(x, (ch) => eq(Float32Array.from(ch), 'low', hz * 1.6, lift, 0.7));
  if (harmonics != null) {
    const low = both(x, (ch) => lp(lp(ch, from, 0.6), from, 0.7));
    const p = peak(low) || 1;
    const pressed = both(low, (ch) => { const c = Float32Array.from(ch); for (let i = 0; i < c.length; i++) c[i] = Math.tanh(c[i] / p * 4) * p; return bp(hp(c, from * 0.9), from * 2.2, 0.6); });
    y = both(y, (ch) => ch); // already a copy
    if (isStereo(y)) { add(y.L, pressed.L, 0, db(harmonics + 12)); add(y.R, pressed.R, 0, db(harmonics + 12)); } else add(y, pressed, 0, db(harmonics + 12));
  }
  return y;
}

/**
 * Punch. Makes the first instant of each event stand further out of what follows (more is a harder crack), or, with
 * a negative amount, sits it back (a gun heard from far off). It follows the sound's own level two ways, fast and
 * slow, and turns the sound up where the fast one is ahead.
 */
export function punch(x, amount = 0.5, { fast = 0.001, slow = 0.03, most = 9 } = {}) {
  const n = lengthOf(x), L = isStereo(x) ? x.L : x, R = isStereo(x) ? x.R : x;
  const af = Math.exp(-1 / (fast * SR)), as = Math.exp(-1 / (slow * SR)), rel = Math.exp(-1 / (0.08 * SR));
  let ef = 0, es = 0;
  const g = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const v = Math.max(Math.abs(L[i]), Math.abs(R[i]));
    ef = v > ef ? af * ef + (1 - af) * v : rel * ef + (1 - rel) * v;
    es = v > es ? as * es + (1 - as) * v : rel * es + (1 - rel) * v;
    const ahead = clamp(toDb(ef + 1e-9) - toDb(es + 1e-9), 0, 24);
    g[i] = db(clamp(ahead * amount, -most, most));
  }
  return mul(copy(x), g);
}

/** Holds a mix together without flattening its hits: slow to act, quick to let go. */
export function glue(x, { threshold = -16, ratio = 2, makeup = 0 } = {}) {
  return compress(copy(x), { threshold, ratio, attack: 0.03, release: 0.25, makeup });
}

/**
 * Grit. A little of the sound pressed hard and laid back under itself: the edge a war film puts on guns and engines.
 * "under" is how far below the clean sound the pressed copy sits (dB).
 */
export function grit(x, { amount = 3, under = -14, low = 200, high = 5000 } = {}) {
  const p = peak(x) || 1;
  const dirty = both(x, (ch) => { const c = Float32Array.from(ch); for (let i = 0; i < c.length; i++) c[i] = Math.tanh(c[i] / p * amount) * p; return lp(hp(c, low), high, 0.6); });
  const y = copy(x);
  if (isStereo(y)) { add(y.L, dirty.L, 0, db(under)); add(y.R, dirty.R, 0, db(under)); } else add(y, dirty, 0, db(under));
  return y;
}

/**
 * Distance. Air takes the top off a sound and the ground takes the bottom; what arrives from a mile away is the
 * middle, softened. "metres" is how far the listener stands from where the recording was made.
 */
export function away(x, metres) {
  const top = clamp(18000 / (1 + metres / 180), 600, 18000);
  const soft = clamp(metres / 1500, 0, 1);
  let y = both(x, (ch) => lp(lp(hp(Float32Array.from(ch), 20 + Math.min(50, metres / 80)), top, 0.6), top * 1.4, 0.6));
  if (soft > 0.05) y = punch(y, -0.5 * soft);
  return y;
}

/** The same recording a touch faster or slower: a bigger engine, or a second engine out of step with the first. */
export const tuned = (x, ratio) => both(x, (ch) => speed(ch, ratio));

/**
 * Several cuts laid over each other. Each layer is { x, at (seconds), db, pan (-1 to 1) }. The result is as long as
 * its longest layer; give stereo: true for a two-sided mix.
 */
export function layers(list, { stereo = false } = {}) {
  const n = Math.max(...list.map((l) => sec(l.at ?? 0) + lengthOf(l.x)));
  const out = stereo ? { L: new Float32Array(n), R: new Float32Array(n) } : new Float32Array(n);
  for (const l of list) {
    const g = db(l.db ?? 0), at = l.at ?? 0;   // seconds: add() does its own counting
    if (stereo) {
      const s = toStereo(l.x), pan = clamp(l.pan ?? 0, -1, 1), a = (pan + 1) * Math.PI / 4;
      const gl = isStereo(l.x) ? (pan > 0 ? 1 - pan : 1) : Math.cos(a) * Math.SQRT2, gr = isStereo(l.x) ? (pan < 0 ? 1 + pan : 1) : Math.sin(a) * Math.SQRT2;
      add(out.L, s.L, at, g * gl); add(out.R, s.R, at, g * gr);
    } else add(out, toMono(l.x), at, g);
  }
  return out;
}

/** A loop cut from the steadiest stretch of a recording, folded so that it has no join. */
export function loopFrom(x, { from = 0, seconds = 8, overlap = 1.5 } = {}) {
  return loopOf(cut(x, from, from + seconds + overlap), overlap);
}

/**
 * The last step for a single event made from a recording. A real gunshot's peak stands far above its body, so
 * bringing it to a set loudness would crush the peak flat. This allows the tip of the peak to be rounded off by at
 * most "squash" dB and no more: if the sound would still pass the ceiling, it is left quieter than the target and
 * says so, and the game makes up the difference with its own volume.
 */
export function finishShot(x, target, { squash = 4, ceiling = -1, fadeIn = 0.001, fadeOut = 0.05, floor = -60, lead = 0.003 } = {}) {
  const L = isStereo(x) ? x.L : x, R = isStereo(x) ? x.R : x, n = lengthOf(x), p0 = peak(x) || 1;
  let first = 0; while (first < n - 1 && Math.abs(L[first]) < p0 * 0.01 && Math.abs(R[first]) < p0 * 0.01) first++;
  first = Math.max(0, first - sec(lead));
  let last = n - 1; const fl = p0 * db(floor); while (last > first && Math.abs(L[last]) < fl && Math.abs(R[last]) < fl) last--;
  let y = both(x, (ch) => ch.slice(first, Math.min(n, last + sec(0.08))));
  fade(y, fadeIn, fadeOut);
  const now = loudness(y).moment;
  let g = target - now;
  const over = toDb(peak(y)) + g - ceiling;
  if (over > squash) g -= over - squash;
  gain(y, db(g));
  const c = db(ceiling), knee = db(ceiling - squash);
  each(y, (ch) => { for (let i = 0; i < ch.length; i++) { const v = Math.abs(ch[i]); if (v > knee) { const t = (v - knee) / (c - knee); ch[i] = Math.sign(ch[i]) * (knee + (c - knee) * Math.tanh(t)); } } });
  return { x: y, short: +(target - loudness(y).moment).toFixed(1) };
}

/** The last step for a loop made from a recording: no sub-rumble, a set loudness, peaks rounded rather than cut. */
export function finishLoop(x, target, { ceiling = -3, lowCut = 28 } = {}) {
  let y = around(x, (c) => hp4(c, lowCut));
  gain(y, db(target - loudness(y).integrated));
  const c = db(ceiling), knee = db(ceiling - 4);
  each(y, (ch) => { for (let i = 0; i < ch.length; i++) { const v = Math.abs(ch[i]); if (v > knee) { const t = (v - knee) / (c - knee); ch[i] = Math.sign(ch[i]) * (knee + (c - knee) * Math.tanh(t)); } } });
  return { x: y, short: +(target - loudness(y).integrated).toFixed(1) };
}

/**
 * Evens out slow swells. An aircraft passing comes up and dies away; a loop made from it must hold one level. The
 * sound's own level is followed over "window" seconds and divided out: strength 1 holds it flat, 0 leaves it alone.
 * Only slow movement is taken out, so the beat of an engine or an exhaust is untouched.
 */
export function even(x, { window = 1.5, strength = 0.85 } = {}) {
  const n = lengthOf(x), m = toMono(x), w = sec(window), hop = sec(0.05);
  const ps = new Float64Array(n + 1); for (let i = 0; i < n; i++) ps[i + 1] = ps[i] + m[i] * m[i];
  const whole = Math.sqrt(ps[n] / Math.max(1, n)) || 1e-9;
  const at = (c) => { const a = Math.max(0, c - (w >> 1)), b = Math.min(n, c + (w >> 1)); return Math.sqrt((ps[b] - ps[a]) / Math.max(1, b - a)) || 1e-9; };
  const g = new Float32Array(n);
  for (let i = 0; i < n; i += hop) {
    const g0 = Math.pow(whole / at(i), strength), g1 = Math.pow(whole / at(Math.min(n - 1, i + hop)), strength);
    for (let k = 0; k < hop && i + k < n; k++) g[i + k] = g0 + (g1 - g0) * (k / hop);
  }
  return mul(copy(x), g);
}

/** A short stretch made long: laid end over end with long folds, until it lasts "seconds". For a drone, not a rhythm. */
export function sustain(x, seconds, overlap = 1.5) {
  const n = lengthOf(x), no = Math.min(sec(overlap), n >> 1), step = n - no, want = sec(seconds);
  const one = (ch) => {
    const out = new Float32Array(want + n);
    for (let at = 0; at < want; at += step) for (let i = 0; i < n; i++) {
      const up = at > 0 && i < no ? Math.sin(i / no * Math.PI / 2) : 1, down = i >= n - no ? Math.cos((i - (n - no)) / no * Math.PI / 2) : 1;
      out[at + i] += ch[i] * up * down;
    }
    return out.slice(0, want);
  };
  return both(x, one);
}

/**
 * Where the beats of a regular sound fall, in seconds: an exhaust, a wheel on rail joints. Give its rough "period";
 * each beat is looked for near where the last two say it should be, so a train gathering speed is followed.
 */
export function beats(x, { period, over = 300 } = {}) {
  const m = hp(Float32Array.from(toMono(x)), over), hop = sec(0.004), env = [];
  for (let i = 0; i + hop <= m.length; i += hop) { let s = 0; for (let k = 0; k < hop; k++) s += m[i + k] * m[i + k]; env.push(Math.sqrt(s / hop)); }
  const sm = env.map((_, i) => { let s = 0, c = 0; for (let k = -3; k <= 3; k++) if (env[i + k] != null) { s += env[i + k]; c++; } return s / c; });
  const top = (a, b) => { let best = a; for (let i = Math.max(0, a); i <= Math.min(sm.length - 1, b); i++) if (sm[i] > sm[best]) best = i; return best; };
  let p = period / (hop / SR), t = top(0, Math.round(p));
  const out = [t];
  while (t + p * 1.3 < sm.length) { const next = top(Math.round(t + p * 0.78), Math.round(t + p * 1.22)); p = 0.75 * p + 0.25 * (next - t); t = next; out.push(t); }
  return out.map((i) => i * hop / SR);
}

/**
 * Sets a sound's beats exactly "period" apart, by playing each stretch between two beats a shade faster or slower.
 * A train gathering speed becomes a train at one speed, which can then go round without a stumble. Starts "lead"
 * seconds before the first beat given and ends at the last.
 */
export function steady(x, at, period, lead = 0.06) {
  const N = sec(period);
  const one = (ch) => {
    const out = new Float32Array(N * (at.length - 1));
    for (let b = 0; b + 1 < at.length; b++) {
      const a0 = (at[b] - lead) * SR, len = (at[b + 1] - at[b]) * SR;
      for (let i = 0; i < N; i++) { const pos = a0 + i / N * len, j = Math.floor(pos), u = pos - j; out[b * N + i] = (ch[j] ?? 0) + ((ch[j + 1] ?? 0) - (ch[j] ?? 0)) * u; }
    }
    return out;
  };
  return both(x, one);
}

/** A shelf or a band of the sound lifted or lowered: tone(x, 'high', 700, -5) takes 5 dB off everything above 700 Hz. */
export const tone = (x, kind, hz, gainDb, q) => both(x, (ch) => eq(Float32Array.from(ch), kind, hz, gainDb, q));

// ---- For the battle pack ----------------------------------------------------------------------------------

/** The gains that set a mono or two-sided layer at "pan" (-1 left to 1 right) in a two-sided mix. */
const sides = (x, pan) => {
  const p = clamp(pan ?? 0, -1, 1), a = (p + 1) * Math.PI / 4;
  return isStereo(x) ? [p > 0 ? 1 - p : 1, p < 0 ? 1 + p : 1] : [Math.cos(a) * Math.SQRT2, Math.sin(a) * Math.SQRT2];
};

/**
 * Events laid on a ring "seconds" long, as layers() lays them, except that whatever runs past the end comes round to
 * the start. The result loops with no join at all: a firefight in which shots keep falling, round and round.
 */
export function ring(list, seconds, { stereo = false } = {}) {
  const n = sec(seconds);
  const out = stereo ? { L: new Float32Array(n), R: new Float32Array(n) } : new Float32Array(n);
  const put = (dst, src, at, g) => { const o = ((sec(at) % n) + n) % n; for (let i = 0; i < src.length; i++) dst[(o + i) % n] += src[i] * g; };
  for (const l of list) {
    const g = db(l.db ?? 0);
    if (stereo) { const s = toStereo(l.x), [gl, gr] = sides(l.x, l.pan); put(out.L, s.L, l.at ?? 0, g * gl); put(out.R, s.R, l.at ?? 0, g * gr); }
    else put(out, toMono(l.x), l.at ?? 0, g);
  }
  return out;
}

/**
 * Plays a recording at a speed that changes as it goes: "ratioAt(t)" gives the speed at t, from 0 (its start) to 1
 * (its end). An engine that speeds up rises in pitch, as a plane's does when it falls into a dive.
 */
export function bend(x, ratioAt) {
  const one = (ch) => {
    const out = []; let p = 0;
    while (p < ch.length - 1) { const j = Math.floor(p), u = p - j; out.push(ch[j] + (ch[j + 1] - ch[j]) * u); p += Math.max(0.05, ratioAt(p / ch.length)); }
    return Float32Array.from(out);
  };
  return both(x, one);
}

/** The quietest moment within "span" seconds of "t": the gap between two rounds of a burst, where a cut is not heard. */
export function gapNear(x, t, span = 0.05) {
  const m = toMono(x), w = sec(0.006), a = Math.max(0, sec(t - span)), b = Math.min(m.length - w, sec(t + span));
  let best = a, low = Infinity;
  for (let i = a; i <= b; i += 24) { let s = 0; for (let k = 0; k < w; k++) s += m[i + k] * m[i + k]; if (s < low) { low = s; best = i; } }
  return (best + w / 2) / SR;
}

/**
 * A long burst made short, as a gunner who lets go sooner: its first "keep" seconds, then its own last round and the
 * echo after it, joined in a gap between two rounds so that no round is cut in half.
 */
export function shorten(x, keep, tailFrom, fold = 0.008) {
  const a = sec(gapNear(x, keep)), b = sec(gapNear(x, tailFrom)), f = sec(fold);
  const one = (ch) => {
    const tail = ch.subarray(b), out = new Float32Array(a + tail.length);
    out.set(ch.subarray(0, a));
    for (let i = 0; i < f; i++) out[a - f + i] *= 1 - i / f;
    for (let i = 0; i < tail.length; i++) out[a - f + i] += tail[i] * (i < f ? i / f : 1);
    return out.subarray(0, a - f + tail.length);
  };
  return both(x, one);
}

/**
 * A cut made to die away as a sound does in the open: whole for "hold" seconds, then falling smoothly (by 8.7 dB
 * every "tau" seconds). Without it, the hiss and wind under a cut stop dead where the cut ends, and a bed built from
 * many cuts steps up and down in level.
 */
export function tailOff(x, hold = 0.15, tau = 0.4) {
  const n = lengthOf(x), h = sec(hold), env = new Float32Array(n);
  for (let i = 0; i < n; i++) env[i] = i < h ? 1 : Math.exp(-(i - h) / (tau * SR));
  return mul(copy(x), env);
}
