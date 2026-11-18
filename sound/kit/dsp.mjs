// The workshop's tools: oscillators, noise, filters, echoes of a place, loudness and the WAV writer.
// Everything here is plain arithmetic on arrays of numbers. Nothing is read from any recording.

import fs from 'node:fs';

export const SR = 48000;
export const TAU = Math.PI * 2;
export const sec = (s) => Math.max(0, Math.round(s * SR));
export const db = (d) => Math.pow(10, d / 20);
export const toDb = (x) => 20 * Math.log10(Math.max(x, 1e-12));
export const clamp = (x, a, b) => Math.min(b, Math.max(a, x));
export const lerp = (a, b, t) => a + (b - a) * t;
export const smooth = (x) => { const t = clamp(x, 0, 1); return t * t * (3 - 2 * t); };
export const midi = (n) => 440 * Math.pow(2, (n - 69) / 12);

// ---- Chance: every sound has its own seed, so it is made the same way every time -------------------------

export function rng(seed) {
  let a = (seed >>> 0) || 1;
  const f = () => {
    a = (a + 0x6D2B79F5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
  const r = {
    f,
    bip: () => f() * 2 - 1,
    range: (lo, hi) => lo + (hi - lo) * f(),
    int: (n) => Math.floor(f() * n),
    pick: (list) => list[Math.floor(f() * list.length)],
    gauss: () => { let s = 0; for (let i = 0; i < 6; i++) s += f(); return (s - 3) / 0.7071; },
    chance: (p) => f() < p,
    fork: () => rng(Math.floor(f() * 4294967296)),
  };
  return r;
}
export function seedOf(name) {
  let h = 2166136261;
  for (let i = 0; i < name.length; i++) { h ^= name.charCodeAt(i); h = Math.imul(h, 16777619); }
  return h >>> 0;
}

// ---- Buffers ---------------------------------------------------------------------------------------------

export const mono = (seconds) => new Float32Array(sec(seconds));
export const stereo = (seconds) => ({ L: new Float32Array(sec(seconds)), R: new Float32Array(sec(seconds)) });
export const isStereo = (x) => x && x.L instanceof Float32Array;
export const lengthOf = (x) => (isStereo(x) ? x.L.length : x.length);
export const copy = (x) => (isStereo(x) ? { L: Float32Array.from(x.L), R: Float32Array.from(x.R) } : Float32Array.from(x));
export const toStereo = (x) => (isStereo(x) ? x : { L: Float32Array.from(x), R: Float32Array.from(x) });
export function toMono(x) {
  if (!isStereo(x)) return x;
  const out = new Float32Array(x.L.length);
  for (let i = 0; i < out.length; i++) out[i] = (x.L[i] + x.R[i]) * 0.5;
  return out;
}
export function each(x, fn) { if (isStereo(x)) { fn(x.L, 0); fn(x.R, 1); } else fn(x, 0); return x; }
export function gain(x, g) { return each(x, (ch) => { for (let i = 0; i < ch.length; i++) ch[i] *= g; }); }
export function mul(x, envelope) { return each(x, (ch) => { const n = Math.min(ch.length, envelope.length); for (let i = 0; i < n; i++) ch[i] *= envelope[i]; for (let i = n; i < ch.length; i++) ch[i] = 0; }); }
export function pad(x, seconds) {
  const n = sec(seconds);
  const grow = (ch) => { if (ch.length >= n) return ch; const out = new Float32Array(n); out.set(ch); return out; };
  return isStereo(x) ? { L: grow(x.L), R: grow(x.R) } : grow(x);
}
export function trim(x, seconds) {
  const n = sec(seconds);
  return isStereo(x) ? { L: x.L.slice(0, n), R: x.R.slice(0, n) } : x.slice(0, n);
}
/** Adds a mono sound into a mono one, at a time. */
export function add(dst, src, at = 0, g = 1) {
  const n0 = sec(at);
  const n = Math.min(src.length, dst.length - n0);
  for (let i = Math.max(0, -n0); i < n; i++) dst[n0 + i] += src[i] * g;
  return dst;
}
/** Places a sound in a stereo picture at a time: pan runs from -1 (left) to +1 (right). */
export function place(dst, src, at = 0, pan = 0, g = 1) {
  const n0 = sec(at);
  if (isStereo(src)) {
    const n = Math.min(src.L.length, dst.L.length - n0);
    const gl = g * Math.min(1, 1 - pan), gr = g * Math.min(1, 1 + pan);
    for (let i = Math.max(0, -n0); i < n; i++) { dst.L[n0 + i] += src.L[i] * gl; dst.R[n0 + i] += src.R[i] * gr; }
    return dst;
  }
  const gl = g * Math.cos((pan + 1) * Math.PI / 4), gr = g * Math.sin((pan + 1) * Math.PI / 4);
  const n = Math.min(src.length, dst.L.length - n0);
  for (let i = Math.max(0, -n0); i < n; i++) { dst.L[n0 + i] += src[i] * gl; dst.R[n0 + i] += src[i] * gr; }
  return dst;
}
/** A stereo picture from a mono sound: the right ear hears it a moment later and a little duller. */
export function widen(src, amount = 0.5, seed = 1) {
  const r = rng(seed);
  const out = { L: Float32Array.from(src), R: new Float32Array(src.length) };
  const d = Math.round((0.004 + 0.009 * r.f()) * SR * amount) + 1;
  const side = lp(delaySamples(src, d), 5200);
  for (let i = 0; i < src.length; i++) { out.R[i] = src[i] * (1 - 0.45 * amount) + side[i] * 0.45 * amount; out.L[i] = src[i] * (1 - 0.2 * amount) - side[i] * 0.2 * amount; }
  return out;
}
export function delaySamples(x, d) {
  const out = new Float32Array(x.length);
  for (let i = d; i < x.length; i++) out[i] = x[i - d];
  return out;
}

// ---- Envelopes -------------------------------------------------------------------------------------------

/** A line through points [seconds, value]; "curve" bends each stretch (1 straight, more than 1 hangs low). */
export function line(n, points, curve = 1) {
  const out = new Float32Array(n);
  let k = 0;
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    while (k < points.length - 2 && t >= points[k + 1][0]) k++;
    const [t0, v0] = points[k], [t1, v1] = points[Math.min(k + 1, points.length - 1)];
    let u = t1 > t0 ? clamp((t - t0) / (t1 - t0), 0, 1) : 1;
    if (curve !== 1) u = Math.pow(u, curve);
    out[i] = v0 + (v1 - v0) * u;
  }
  return out;
}
/** A quick rise and a dying fall: the shape of nearly everything that is struck or fired. */
export function decay(n, tau, attack = 0.001, hold = 0) {
  const out = new Float32Array(n);
  const na = Math.max(1, sec(attack)), nh = sec(hold);
  for (let i = 0; i < n; i++) out[i] = i < na ? i / na : i < na + nh ? 1 : Math.exp(-(i - na - nh) / (tau * SR));
  return out;
}
export function fade(x, fadeIn = 0.005, fadeOut = 0.02) {
  return each(x, (ch) => {
    const ni = Math.min(ch.length, sec(fadeIn)), no = Math.min(ch.length, sec(fadeOut));
    for (let i = 0; i < ni; i++) ch[i] *= smooth(i / ni);
    for (let i = 0; i < no; i++) ch[ch.length - 1 - i] *= smooth(i / no);
  });
}
/** Slow wandering between 0 and 1: for wind that gusts, a fire that breathes, an engine that hunts. */
export function wander(n, rate, r, depth = 1) {
  const out = new Float32Array(n);
  const step = Math.max(1, Math.round(SR / (rate * 2)));
  let a = r.f(), b = r.f(), c = r.f(), d = r.f();
  for (let i = 0; i < n; i++) {
    const k = i % step;
    if (k === 0 && i > 0) { a = b; b = c; c = d; d = r.f(); }
    const u = k / step;
    // Catmull-Rom through the four values, so it never jumps.
    const v = 0.5 * ((2 * b) + (-a + c) * u + (2 * a - 5 * b + 4 * c - d) * u * u + (-a + 3 * b - 3 * c + d) * u * u * u);
    out[i] = 1 - depth + depth * clamp(v, 0, 1);
  }
  return out;
}

// ---- Sources ---------------------------------------------------------------------------------------------

export function white(n, r) { const out = new Float32Array(n); for (let i = 0; i < n; i++) out[i] = r.bip(); return out; }
export function pink(n, r) {
  const out = new Float32Array(n);
  let b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
  for (let i = 0; i < n; i++) {
    const w = r.bip();
    b0 = 0.99886 * b0 + w * 0.0555179; b1 = 0.99332 * b1 + w * 0.0750759; b2 = 0.96900 * b2 + w * 0.1538520;
    b3 = 0.86650 * b3 + w * 0.3104856; b4 = 0.55000 * b4 + w * 0.5329522; b5 = -0.7616 * b5 - w * 0.0168980;
    out[i] = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362) * 0.11;
    b6 = w * 0.115926;
  }
  return out;
}
export function brown(n, r) {
  const out = new Float32Array(n);
  let y = 0;
  for (let i = 0; i < n; i++) { y = (y + 0.02 * r.bip()) / 1.02; out[i] = y * 3.5; }
  return out;
}
const at = (v, i) => (typeof v === 'number' ? v : v[Math.min(i, v.length - 1)]);
/** A pure tone. The pitch may be one number or change sample by sample. */
export function sine(n, freq, phase = 0) {
  const out = new Float32Array(n);
  let p = phase;
  for (let i = 0; i < n; i++) { out[i] = Math.sin(p); p += TAU * at(freq, i) / SR; }
  return out;
}
/** A buzzing tone (saw, square or pulse) with its edges rounded so that no false tones are heard. */
export function buzz(n, freq, shape = 'saw', width = 0.5, phase = 0) {
  const out = new Float32Array(n);
  let p = phase % 1;
  const blep = (t, dt) => {
    if (t < dt) { t /= dt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
    return 0;
  };
  for (let i = 0; i < n; i++) {
    const dt = at(freq, i) / SR;
    let v;
    if (shape === 'saw') v = 2 * p - 1 - blep(p, dt);
    else {
      const w = shape === 'square' ? 0.5 : width;
      v = (p < w ? 1 : -1) + blep(p, dt) - blep((p + 1 - w) % 1, dt);
    }
    out[i] = v;
    p += dt; if (p >= 1) p -= 1;
  }
  return out;
}

// ---- Filters ---------------------------------------------------------------------------------------------

/** The workhorse filter: keeps the low ("lp"), the high ("hp") or a band ("bp"). The pitch it turns at may move. */
export function svf(x, mode, freq, q = 0.707) {
  const out = new Float32Array(x.length);
  let ic1 = 0, ic2 = 0, g = 0, k = 1 / q, a1 = 0, a2 = 0, a3 = 0;
  const moving = typeof freq !== 'number' || typeof q !== 'number';
  const set = (f, qq) => { g = Math.tan(Math.PI * clamp(f, 8, SR * 0.47) / SR); k = 1 / qq; a1 = 1 / (1 + g * (g + k)); a2 = g * a1; a3 = g * a2; };
  set(at(freq, 0), at(q, 0));
  for (let i = 0; i < x.length; i++) {
    if (moving && (i & 15) === 0) set(at(freq, i), at(q, i));
    const v3 = x[i] - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
    ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
    out[i] = mode === 'lp' ? v2 : mode === 'bp' ? v1 * k : mode === 'hp' ? x[i] - k * v1 - v2 : mode === 'notch' ? x[i] - k * v1 : v1;
  }
  return out;
}
export const lp = (x, f, q = 0.707) => svf(x, 'lp', f, q);
export const hp = (x, f, q = 0.707) => svf(x, 'hp', f, q);
export const bp = (x, f, q = 1) => svf(x, 'bp', f, q);
export const lp4 = (x, f, q = 0.707) => svf(svf(x, 'lp', f, 0.54), 'lp', f, q * 1.85);
export const hp4 = (x, f, q = 0.707) => svf(svf(x, 'hp', f, 0.54), 'hp', f, q * 1.85);
/** A band of the sound lifted or lowered, or everything above or below a pitch ("high" and "low" shelves). */
export function eq(x, kind, f, gainDb, q = 0.9) {
  const A = Math.pow(10, gainDb / 40), w = TAU * f / SR, cs = Math.cos(w), sn = Math.sin(w);
  let b0, b1, b2, a0, a1, a2;
  if (kind === 'peak') {
    const al = sn / (2 * q);
    b0 = 1 + al * A; b1 = -2 * cs; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cs; a2 = 1 - al / A;
  } else {
    const al = sn / 2 * Math.sqrt(2), s = 2 * Math.sqrt(A) * al;
    if (kind === 'low') {
      b0 = A * ((A + 1) - (A - 1) * cs + s); b1 = 2 * A * ((A - 1) - (A + 1) * cs); b2 = A * ((A + 1) - (A - 1) * cs - s);
      a0 = (A + 1) + (A - 1) * cs + s; a1 = -2 * ((A - 1) + (A + 1) * cs); a2 = (A + 1) + (A - 1) * cs - s;
    } else {
      b0 = A * ((A + 1) + (A - 1) * cs + s); b1 = -2 * A * ((A - 1) + (A + 1) * cs); b2 = A * ((A + 1) + (A - 1) * cs - s);
      a0 = (A + 1) - (A - 1) * cs + s; a1 = 2 * ((A - 1) - (A + 1) * cs); a2 = (A + 1) - (A - 1) * cs - s;
    }
  }
  return biquad(x, b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0);
}
export function biquad(x, b0, b1, b2, a1, a2) {
  const out = new Float32Array(x.length);
  let x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  for (let i = 0; i < x.length; i++) {
    const y = b0 * x[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x[i]; y2 = y1; y1 = y; out[i] = y;
  }
  return out;
}
/** A hollow thing that rings when sound passes through it: a pipe, a hull, a funnel. */
export function comb(x, seconds, feedback, damp = 0.3, mix = 1) {
  const d = Math.max(1, Math.round(seconds * SR));
  const line = new Float32Array(d);
  const out = new Float32Array(x.length);
  let k = 0, low = 0;
  for (let i = 0; i < x.length; i++) {
    const y = line[k];
    low = y + (low - y) * damp;
    line[k] = x[i] + low * feedback;
    out[i] = x[i] * (1 - mix) + y * mix;
    if (++k >= d) k = 0;
  }
  return out;
}
/** Reads a sound through a delay that changes: the stretch and squeeze of something passing by. */
export function warp(x, delaySeconds) {
  const out = new Float32Array(delaySeconds.length);
  for (let i = 0; i < out.length; i++) {
    const p = i - delaySeconds[i] * SR;
    if (p < 0 || p >= x.length - 1) continue;
    const j = Math.floor(p), u = p - j;
    out[i] = x[j] + (x[j + 1] - x[j]) * u;
  }
  return out;
}
/** Plays a sound faster or slower (and so higher or lower), like a record at another speed. */
export function speed(x, ratio) {
  const n = Math.floor(x.length / ratio);
  const out = new Float32Array(n);
  for (let i = 0; i < n; i++) { const p = i * ratio, j = Math.floor(p), u = p - j; out[i] = x[j] + ((x[j + 1] ?? 0) - x[j]) * u; }
  return out;
}

// ---- Weight and loudness ---------------------------------------------------------------------------------

/** Thickens a sound by pressing its peaks: more of this is a gun, less is a warm valve. */
export function drive(x, amount = 2, makeup = true) {
  const norm = makeup ? 1 / Math.tanh(amount) : 1;
  return each(x, (ch) => { for (let i = 0; i < ch.length; i++) ch[i] = Math.tanh(ch[i] * amount) * norm; });
}
/** Holds the loud parts down and lets the quiet ones up, so a sound is full without being spiky. */
export function compress(x, { threshold = -18, ratio = 3, attack = 0.005, release = 0.12, makeup = 0 } = {}) {
  const n = lengthOf(x);
  const a = Math.exp(-1 / (attack * SR)), rl = Math.exp(-1 / (release * SR));
  let e = 0;
  const g = new Float32Array(n);
  const L = isStereo(x) ? x.L : x, R = isStereo(x) ? x.R : x;
  for (let i = 0; i < n; i++) {
    const level = Math.max(Math.abs(L[i]), Math.abs(R[i]));
    e = level > e ? a * e + (1 - a) * level : rl * e + (1 - rl) * level;
    const over = toDb(e) - threshold;
    g[i] = db((over > 0 ? -over * (1 - 1 / ratio) : 0) + makeup);
  }
  return mul(x, g);
}
/** Nothing passes this ceiling: peaks are turned down a moment before they arrive. */
export function limit(x, ceilingDb = -1) {
  const n = lengthOf(x), ceil = db(ceilingDb), look = sec(0.004);
  const L = isStereo(x) ? x.L : x, R = isStereo(x) ? x.R : x;
  const g = new Float32Array(n).fill(1);
  let e = 0;
  const rl = Math.exp(-1 / (0.08 * SR));
  for (let i = 0; i < n + look; i++) {
    const j = Math.min(n - 1, i);
    const level = i < n ? Math.max(Math.abs(L[j]), Math.abs(R[j])) : 0;
    e = Math.max(level, e * rl);
    if (i >= look) g[i - look] = e > ceil ? ceil / e : 1;
  }
  // Smooth the gain a little so that it never clicks.
  let s = 1;
  for (let i = 0; i < n; i++) { s = Math.min(g[i], s + (g[i] - s) * 0.02); g[i] = s; }
  mul(x, g);
  return each(x, (ch) => { for (let i = 0; i < ch.length; i++) ch[i] = clamp(ch[i], -ceil, ceil); });
}
export function peak(x) { let p = 0; each(x, (ch) => { for (let i = 0; i < ch.length; i++) { const v = Math.abs(ch[i]); if (v > p) p = v; } }); return p; }

const K1 = [1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585];
const K2 = [1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621];
/**
 * How loud a sound is to the ear, the broadcasters' way (LUFS). "integrated" is the whole of a long sound;
 * "moment" is its loudest four tenths of a second, which is what matters for a bang or a click.
 */
export function loudness(x) {
  const chans = isStereo(x) ? [x.L, x.R] : [x];
  const n = chans[0].length, win = sec(0.4), hop = sec(0.1);
  const weighted = chans.map((ch) => biquad(biquad(ch, ...K1), ...K2));
  const blocks = [];
  if (n <= win) {
    let s = 0;
    for (const ch of weighted) for (let i = 0; i < n; i++) s += ch[i] * ch[i];
    blocks.push(s / win);
  } else {
    for (let start = 0; start + win <= n; start += hop) {
      let s = 0;
      for (const ch of weighted) for (let i = start; i < start + win; i++) s += ch[i] * ch[i];
      blocks.push(s / win);
    }
  }
  const L = (ms) => -0.691 + 10 * Math.log10(Math.max(ms, 1e-12));
  const moment = L(Math.max(...blocks));
  let kept = blocks.filter((b) => L(b) > -70);
  if (kept.length) {
    const rel = L(kept.reduce((a, b) => a + b, 0) / kept.length) - 10;
    const gated = kept.filter((b) => L(b) > rel);
    if (gated.length) kept = gated;
  }
  const integrated = kept.length ? L(kept.reduce((a, b) => a + b, 0) / kept.length) : -70;
  return { moment, integrated };
}
/** Brings a sound to a set loudness, then makes sure no peak passes the ceiling. */
export function level(x, target, { by = 'moment', ceiling = -1 } = {}) {
  const now = loudness(x)[by];
  gain(x, db(target - now));
  if (peak(x) > db(ceiling)) limit(x, ceiling);
  return x;
}

// ---- The echo of a place ---------------------------------------------------------------------------------

export function fft(re, im, inverse) {
  const n = re.length;
  for (let i = 1, j = 0; i < n; i++) {
    let bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) { let t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
  }
  for (let len = 2; len <= n; len <<= 1) {
    const ang = (inverse ? TAU : -TAU) / len, wr = Math.cos(ang), wi = Math.sin(ang);
    for (let i = 0; i < n; i += len) {
      let cr = 1, ci = 0;
      for (let k = 0; k < len / 2; k++) {
        const a = i + k, b = a + len / 2;
        const xr = re[b] * cr - im[b] * ci, xi = re[b] * ci + im[b] * cr;
        re[b] = re[a] - xr; im[b] = im[a] - xi; re[a] += xr; im[a] += xi;
        const t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
      }
    }
  }
  if (inverse) for (let i = 0; i < n; i++) { re[i] /= n; im[i] /= n; }
}
/** Every sample of a sound answered by the whole echo of a place. */
export function convolve(x, ir) {
  const n = x.length + ir.length - 1;
  let size = 1; while (size < n) size <<= 1;
  const ar = new Float64Array(size), ai = new Float64Array(size), br = new Float64Array(size), bi = new Float64Array(size);
  ar.set(x); br.set(ir);
  fft(ar, ai, false); fft(br, bi, false);
  for (let i = 0; i < size; i++) { const r = ar[i] * br[i] - ai[i] * bi[i]; ai[i] = ar[i] * bi[i] + ai[i] * br[i]; ar[i] = r; }
  fft(ar, ai, true);
  const out = new Float32Array(n);
  for (let i = 0; i < n; i++) out[i] = ar[i];
  return out;
}
/**
 * The echo of a place, built from noise that dies away: "rt" is how long it rings, "bright" and "dark" how the ring
 * dulls as it dies, "echoes" are single returns [seconds, strength] from a wood, a hill or a wall.
 */
export function makePlace({ rt = 1, pre = 0.01, bright = 6000, dark = 900, tail = 1, echoes = [], echoTone = 2500, seed = 7 }) {
  const n = sec(pre + rt * 1.25 + (echoes.length ? Math.max(...echoes.map((e) => e[0])) + 0.3 : 0));
  const side = (s) => {
    const r = rng(seed * 31 + s * 977);
    const body = new Float32Array(n);
    const n0 = sec(pre), nt = sec(rt * 1.25);
    for (let i = 0; i < nt && n0 + i < n; i++) body[n0 + i] = r.gauss() * Math.exp(-6.908 * i / (rt * SR)) * smooth(i / (0.004 * SR));
    const tone = new Float32Array(n);
    for (let i = 0; i < n; i++) tone[i] = lerp(bright, dark, clamp((i - n0) / nt, 0, 1));
    let out = svf(body, 'lp', tone, 0.6);
    let e = 0; for (let i = 0; i < n; i++) e += out[i] * out[i];
    gain(out, tail / Math.sqrt(Math.max(e, 1e-9)));
    if (echoes.length) {
      const hits = new Float32Array(n);
      for (const [t, g] of echoes) {
        // Each return is itself a small spread of arrivals, never one clean copy.
        for (let k = 0; k < 9; k++) { const i = sec(t * (1 + 0.012 * s) + 0.0025 * k * (1 + r.f())); if (i < n) hits[i] += g * r.bip() * Math.exp(-k * 0.35); }
      }
      out = addArrays(out, lp(hits, echoTone, 0.6));
    }
    return out;
  };
  return { L: side(0), R: side(1) };
}
function addArrays(a, b) { const out = new Float32Array(Math.max(a.length, b.length)); out.set(a); for (let i = 0; i < b.length; i++) out[i] += b[i]; return out; }
const places = new Map();
export const PLACES = {
  // A desk in a panelled room: close, dry, wooden.
  desk: { rt: 0.22, pre: 0.004, bright: 7000, dark: 2500, echoes: [[0.007, 0.5], [0.013, 0.35], [0.021, 0.22]], echoTone: 5000 },
  // The war room itself, heard from the middle of it.
  room: { rt: 0.55, pre: 0.012, bright: 6000, dark: 1800, echoes: [[0.017, 0.4], [0.031, 0.3], [0.046, 0.2]], echoTone: 4200 },
  // A band in a hall.
  hall: { rt: 1.7, pre: 0.025, bright: 5200, dark: 1300, echoes: [[0.031, 0.3], [0.052, 0.22], [0.083, 0.15]], echoTone: 3600 },
  // Open country: almost nothing comes back, and what does comes late and dull, off a wood or a rise.
  field: { rt: 1.3, pre: 0.09, bright: 2200, dark: 500, tail: 0.5, echoes: [[0.21, 0.5], [0.38, 0.32], [0.66, 0.2]], echoTone: 1500 },
  // Big guns in broken country: the report rolls round the hills.
  valley: { rt: 3.6, pre: 0.14, bright: 1500, dark: 280, tail: 0.8, echoes: [[0.42, 0.55], [0.87, 0.4], [1.45, 0.3], [2.3, 0.2]], echoTone: 800 },
  // A town at night: the walls answer quickly and for a long while.
  city: { rt: 2.1, pre: 0.03, bright: 3600, dark: 600, tail: 0.9, echoes: [[0.06, 0.45], [0.11, 0.35], [0.19, 0.28], [0.33, 0.2]], echoTone: 2400 },
  // Open water: one long faint return and a low wash.
  sea: { rt: 2.8, pre: 0.2, bright: 1600, dark: 300, tail: 0.5, echoes: [[0.95, 0.4], [1.9, 0.2]], echoTone: 700 },
  // High in the air: dry, and then the ground answers.
  sky: { rt: 1.6, pre: 0.3, bright: 1800, dark: 400, tail: 0.45, echoes: [[0.55, 0.4], [0.8, 0.25]], echoTone: 1100 },
  // Inside iron: a hull, a boiler, a tank.
  iron: { rt: 0.9, pre: 0.002, bright: 4200, dark: 1500, echoes: [[0.004, 0.6], [0.009, 0.5], [0.015, 0.4], [0.024, 0.3]], echoTone: 3000 },
};
/** Sets a sound in a place: "wet" is how much of the place is heard beside the sound itself, in dB. */
export function inPlace(x, name, wet = -12, extra = {}) {
  const key = name + JSON.stringify(extra);
  if (!places.has(key)) places.set(key, makePlace({ ...PLACES[name], ...extra, seed: seedOf(key) }));
  const ir = places.get(key);
  const src = toStereo(x);
  const n = src.L.length + ir.L.length - 1;
  const out = { L: new Float32Array(n), R: new Float32Array(n) };
  const g = db(wet);
  // The left ear hears mostly the left of the picture, with a little of the right, and so across.
  const ll = convolve(src.L, ir.L), rr = convolve(src.R, ir.R);
  for (let i = 0; i < n; i++) {
    out.L[i] = (i < src.L.length ? src.L[i] : 0) + g * (0.85 * ll[i] + 0.15 * rr[i]);
    out.R[i] = (i < src.R.length ? src.R[i] : 0) + g * (0.85 * rr[i] + 0.15 * ll[i]);
  }
  return out;
}
/** Heard from further off: quieter is the engine's business, but duller and thinner is done here. */
export function far(x, metres) {
  const top = clamp(16000 / (1 + metres / 220), 500, 16000);
  return each(x, (ch) => { ch.set(lp(hp(ch, 20 + Math.min(60, metres / 60)), top, 0.6)); });
}

// ---- Loops -----------------------------------------------------------------------------------------------

/** Makes a sound that can go round for ever: its last stretch is folded over its first, so there is no join. */
export function loopOf(x, overlap = 1.0) {
  const no = sec(overlap), n = lengthOf(x) - no;
  const fold = (ch) => {
    const out = ch.slice(0, n);
    for (let i = 0; i < no; i++) {
      const u = i / no;
      out[i] = ch[i] * Math.sin(u * Math.PI / 2) + ch[n + i] * Math.cos(u * Math.PI / 2);
    }
    return out;
  };
  return isStereo(x) ? { L: fold(x.L), R: fold(x.R) } : fold(x);
}

/** Does something (a filter, say) to a sound that goes round, as if it had been going round for ever: no start, no join. */
export function around(x, fn) {
  const one = (ch) => { const n = ch.length, twice = new Float32Array(n * 2); twice.set(ch); twice.set(ch, n); return fn(twice).slice(n, n * 2); };
  return isStereo(x) ? { L: one(x.L), R: one(x.R) } : one(x);
}

// ---- Writing them out ------------------------------------------------------------------------------------

export function writeWav(path, x) {
  const chans = isStereo(x) ? [x.L, x.R] : [x];
  const n = chans[0].length, c = chans.length;
  const data = Buffer.alloc(44 + n * c * 2);
  data.write('RIFF', 0); data.writeUInt32LE(36 + n * c * 2, 4); data.write('WAVEfmt ', 8);
  data.writeUInt32LE(16, 16); data.writeUInt16LE(1, 20); data.writeUInt16LE(c, 22);
  data.writeUInt32LE(SR, 24); data.writeUInt32LE(SR * c * 2, 28); data.writeUInt16LE(c * 2, 32); data.writeUInt16LE(16, 34);
  data.write('data', 36); data.writeUInt32LE(n * c * 2, 40);
  // A whisper of noise is added before rounding to 16 bits, so that quiet tails fade and do not crumble.
  const r = rng(n + 17);
  for (let i = 0; i < n; i++) {
    for (let k = 0; k < c; k++) {
      const v = chans[k][i] * 32767 + (r.f() - r.f()) * 0.5;
      data.writeInt16LE(clamp(Math.round(v), -32768, 32767), 44 + (i * c + k) * 2);
    }
  }
  fs.writeFileSync(path, data);
}
/** Reads back a WAV that this kit wrote (16 bits, one or two channels). */
export function readWav(path) {
  const b = fs.readFileSync(path);
  const c = b.readUInt16LE(22), n = b.readUInt32LE(40) / (c * 2);
  const chans = Array.from({ length: c }, () => new Float32Array(n));
  for (let i = 0; i < n; i++) for (let k = 0; k < c; k++) chans[k][i] = b.readInt16LE(44 + (i * c + k) * 2) / 32768;
  return c === 2 ? { L: chans[0], R: chans[1] } : chans[0];
}
