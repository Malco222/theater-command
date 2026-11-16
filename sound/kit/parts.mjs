// The parts sounds are built from: a blast, a struck piece of iron, an engine, a breath of steam, a sheet of paper,
// a brass note. Each is made here from noise and tones; the recipes put them together.

import {
  SR, TAU, sec, db, clamp, lerp, smooth, midi, rng, mono, add, gain, mul, line, decay, fade, wander,
  white, pink, brown, sine, buzz, svf, lp, hp, bp, lp4, eq, comb, drive, peak, lengthOf, each, isStereo, toMono,
  loudness, level, limit,
} from './dsp.mjs';

/** A falling or rising pitch, bent the way real things bend: fast at first, then settling. */
export function glide(n, from, to, tau) {
  const out = new Float32Array(n);
  for (let i = 0; i < n; i++) out[i] = to + (from - to) * Math.exp(-i / (tau * SR));
  return out;
}

// ---- Things that go off ----------------------------------------------------------------------------------

/**
 * Anything that goes off, from a rifle to a bomb. Four layers: the crack (the first thousandth of a second), the
 * body (the roar, which dulls as it dies), the thump (felt more than heard) and the rumble that rolls on after.
 */
export function blast(r, o = {}) {
  const {
    dur = 3, crack = 1, crackTone = 2500, crackTime = 0.004,
    body = 1, bodyFrom = 5000, bodyTo = 160, bodyTime = 0.16, grit = 2.5,
    thump = 1, thumpFrom = 120, thumpTo = 36, thumpTime = 0.3,
    rumble = 0.5, rumbleTone = 240, rumbleTime = 1.1, rumbleRoll = 5,
  } = o;
  const n = sec(dur);
  const out = new Float32Array(n);
  if (crack > 0) {
    const nc = Math.min(n, sec(crackTime * 12));
    const c = mul(hp(white(nc, r), crackTone, 0.7), decay(nc, crackTime, 0.0002));
    add(out, c, 0, crack * 1.4);
    out[0] += crack * 0.6; out[1] -= crack * 0.3;
  }
  if (body > 0) {
    const nb = Math.min(n, sec(bodyTime * 9));
    const tone = glide(nb, bodyFrom, bodyTo, bodyTime * 0.55);
    const b = mul(svf(white(nb, r), 'lp', tone, 0.8), decay(nb, bodyTime, 0.0008));
    drive(b, grit);
    add(out, b, 0, body);
  }
  if (thump > 0) {
    const nt = Math.min(n, sec(thumpTime * 8));
    const t = mul(sine(nt, glide(nt, thumpFrom, thumpTo, thumpTime * 0.35)), decay(nt, thumpTime, 0.002));
    drive(t, 1.6);
    add(out, t, 0, thump * 1.1);
  }
  if (rumble > 0) {
    const roll = wander(n, rumbleRoll, r, 0.75);
    const u = mul(mul(lp4(brown(n, r), rumbleTone), decay(n, rumbleTime, 0.06)), roll);
    add(out, u, 0, rumble * 2.2);
  }
  return out;
}

/** What is thrown up coming down again: earth, stones, splinters, tiles. */
export function debris(r, dur, { count = 30, from = 0.2, to = 1.8, lo = 700, hi = 4200, size = 0.012, level: g = 0.3 } = {}) {
  const n = sec(dur), out = new Float32Array(n);
  for (let k = 0; k < count; k++) {
    const at = lerp(from, to, Math.pow(r.f(), 1.6));
    const ng = sec(size * r.range(1, 6));
    const grain = mul(bp(white(ng, r), r.range(lo, hi), r.range(1, 4)), decay(ng, size * r.range(0.3, 1.2), 0.0005));
    add(out, grain, at, g * r.range(0.2, 1) * (1 - 0.6 * (at - from) / (to - from)));
  }
  return out;
}

/** The whistle of something falling or flying: a narrow band of rushing air whose pitch moves. */
export function whistle(r, dur, { from = 2600, to = 900, q = 14, breath = 0.35, curve = 1.6 } = {}) {
  const n = sec(dur);
  const tone = new Float32Array(n);
  for (let i = 0; i < n; i++) tone[i] = from * Math.pow(to / from, Math.pow(i / n, curve));
  const air = white(n, r);
  const out = svf(air, 'bp', tone, q);
  const second = svf(air, 'bp', tone.map((f) => f * 2.02), q * 0.7);
  for (let i = 0; i < n; i++) out[i] = out[i] * 1.0 + second[i] * 0.25 + air[i] * breath * 0.02;
  return out;
}

// ---- Things that are struck ------------------------------------------------------------------------------

const PLATE = [1, 1.58, 2.24, 2.92, 3.47, 4.15, 5.03, 5.87, 6.71, 7.9];
const BELL = [0.5, 1, 1.19, 1.5, 2, 2.51, 2.66, 3.01, 4.1, 5.43];
/** Struck iron: a rail, a breech, a buffer, a shell case. Many tones that are not in tune with each other. */
export function clang(r, { f0 = 420, dur = 1.2, tau = 0.35, partials = 8, bright = 0.7, spread = PLATE, click = 0.5, jitter = 0.04 } = {}) {
  const n = sec(dur), out = new Float32Array(n);
  for (let k = 0; k < partials; k++) {
    const f = f0 * spread[k % spread.length] * (1 + r.bip() * jitter) * (k >= spread.length ? 2 : 1);
    if (f > SR * 0.45) continue;
    const a = Math.pow(bright, k) * r.range(0.6, 1), t = tau / (1 + k * 0.55);
    const ph = r.f() * TAU;
    for (let i = 0; i < n; i++) out[i] += a * Math.sin(ph + TAU * f * i / SR) * Math.exp(-i / (t * SR));
  }
  if (click > 0) { const nc = sec(0.01); add(out, mul(hp(white(nc, r), 2500), decay(nc, 0.0015, 0.0001)), 0, click); }
  return fade(out, 0, Math.min(dur * 0.3, 0.6)); // rung out to nothing, never cut off
}
export function bell(r, { f0 = 520, dur = 3, tau = 1.4, bright = 0.72, strike = 0.4 } = {}) {
  const out = clang(r, { f0, dur, tau, partials: 10, bright, spread: BELL, click: 0, jitter: 0.003 });
  const ns = sec(0.02);
  add(out, mul(bp(white(ns, r), f0 * 4, 1.2), decay(ns, 0.004, 0.0003)), 0, strike);
  return out;
}
/** Wood on wood: a piece set down on the table, a knuckle on a desk, a drawer shut. */
export function knock(r, { f = [190, 340, 560, 910], tau = 0.035, dur = 0.25, click = 0.5, weight = 1 } = {}) {
  const n = sec(dur), out = new Float32Array(n);
  f.forEach((hz, k) => {
    const ph = r.f() * TAU, a = (k === 0 ? weight : 1 / (1 + k)) * r.range(0.8, 1), t = tau / (1 + k * 0.4);
    for (let i = 0; i < n; i++) out[i] += a * Math.sin(ph + TAU * hz * (1 + r.bip() * 0.0) * i / SR) * Math.exp(-i / (t * SR));
  });
  const nc = sec(0.012);
  add(out, mul(lp(hp(white(nc, r), 900), 6000), decay(nc, 0.002, 0.0002)), 0, click);
  return fade(out, 0.0003, 0.01);
}
/** The smallest sound in the game: a switch, a key, a catch. */
export function tick(r, { tone = 3200, body = 900, tau = 0.004, dur = 0.05, air = 1 } = {}) {
  const n = sec(dur), out = new Float32Array(n);
  add(out, mul(bp(white(n, r), tone, 1.6), decay(n, tau * 0.6, 0.0001)), 0, air);
  add(out, mul(sine(n, body), decay(n, tau * 2.2, 0.0002)), 0, 0.5);
  add(out, mul(sine(n, body * 2.7), decay(n, tau, 0.0002)), 0, 0.25);
  return out;
}

// ---- Paper, pencil, cloth, steam, air --------------------------------------------------------------------

/** Something dragged or drawn: a sheet across a table, a pencil across a map, a sleeve across baize. */
export function swish(r, dur, { lo = 1800, hi = 6500, q = 0.9, rough = 0.6, grain = 140, shape = 0.4, move = 1 } = {}) {
  const n = sec(dur);
  const tone = new Float32Array(n), env = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const u = i / n;
    tone[i] = lerp(lo, hi, move > 0 ? Math.sin(u * Math.PI * 0.5 * move + (1 - move) * 0.5) : 1 - u);
    env[i] = Math.pow(Math.sin(Math.PI * Math.pow(u, shape * 2)), 1.3);
  }
  const fibre = lp(white(n, r), grain).map((v) => Math.abs(v) * 6);
  const out = svf(white(n, r), 'bp', tone, q);
  for (let i = 0; i < n; i++) out[i] *= env[i] * ((1 - rough) + rough * Math.min(1.6, fibre[i]));
  return fade(out, 0.004, 0.012);
}
/** Steam let go, air through a valve, spray off a bow. */
export function hiss(r, dur, { tone = 5200, q = 0.7, low = 1800, attack = 0.03, tau = 0.6, hold = 0 } = {}) {
  const n = sec(dur);
  const out = bp(hp(white(n, r), low), tone, q);
  return mul(out, decay(n, tau, attack, hold));
}
/** Wind: air moving past things, rising and falling, never the same twice. */
export function wind(r, dur, { lo = 220, hi = 900, q = 1.4, gust = 0.18, depth = 0.7, howl = 0, hissy = 0.25 } = {}) {
  const n = sec(dur);
  const g = wander(n, gust, r, 1);
  const tone = g.map((v) => lerp(lo, hi, v));
  let out = svf(pink(n, r), 'bp', tone, q);
  for (let i = 0; i < n; i++) out[i] *= (1 - depth) + depth * g[i];
  if (howl > 0) {
    const g2 = wander(n, gust * 1.7, r, 1);
    const h = svf(white(n, r), 'bp', g2.map((v) => lerp(380, 1150, v)), 26);
    for (let i = 0; i < n; i++) out[i] += h[i] * howl * g2[i] * g2[i] * 0.5;
  }
  if (hissy > 0) {
    const s = hp(white(n, r), 3500);
    for (let i = 0; i < n; i++) out[i] += s[i] * hissy * 0.06 * g[i] * g[i];
  }
  return out;
}

// ---- Engines ---------------------------------------------------------------------------------------------

/**
 * A piston engine. Each cylinder fires in turn; no two fire quite alike, which is what makes an engine sound rough
 * and alive rather than like an organ pipe. Then the exhaust pipes colour it, and a propeller (if it has one)
 * beats the air in front.
 */
export function engine(r, dur, o = {}) {
  const {
    cyl = 9, rpm = 2000, rev = null, rough = 0.3, sharp = 3.2,
    pipes = [[140, 1.4, 1.0], [380, 1.2, 0.7], [950, 1.0, 0.35]],
    tone = 2600, breath = 0.35, prop = 0, blades = 3, gearing = 0.56, whine = 0, whineAt = 3400, low = 0.5,
  } = o;
  const n = sec(dur), size = 8192;
  const table = new Float32Array(size);
  for (let c = 0; c < cyl; c++) {
    const pos = (c / cyl + r.bip() * 0.006 * rough * 3 + 1) % 1, g = 1 + r.gauss() * rough * 0.55;
    for (let i = 0; i < size; i++) {
      const x = ((i / size - pos) % 1 + 1) % 1;
      table[i] += g * Math.exp(-x * cyl * sharp) * (1 - Math.exp(-x * cyl * 60));
    }
  }
  let mean = 0; for (let i = 0; i < size; i++) mean += table[i]; mean /= size;
  let top = 0; for (let i = 0; i < size; i++) { table[i] -= mean; top = Math.max(top, Math.abs(table[i])); }
  for (let i = 0; i < size; i++) table[i] /= top;
  const raw = new Float32Array(n), air = white(n, r), hunt = wander(n, 3, r, 1), flutter = wander(n, 11, r, 1);
  let ph = r.f(), firing = -1, charge = 1, chargeNow = 1;
  for (let i = 0; i < n; i++) {
    const f = (rpm / 120) * (rev ? rev[Math.min(i, rev.length - 1)] : 1) * (1 + (hunt[i] - 0.5) * 0.012 * rough * 3);
    ph += f / SR; if (ph >= 1) ph -= 1;
    const p = ph * size, j = Math.floor(p), u = p - j;
    const v = table[j] + (table[(j + 1) % size] - table[j]) * u;
    // No two firings burn alike: each stroke is a little stronger or weaker than the last one of the same cylinder.
    const now = Math.floor(ph * cyl);
    if (now !== firing) { firing = now; charge = 1 + r.gauss() * rough * 0.45; }
    chargeNow += (charge - chargeNow) * Math.min(1, f * cyl * 6 / SR);
    raw[i] = v * chargeNow * (1 + (flutter[i] - 0.5) * rough * 0.9) + air[i] * Math.max(0, v) * breath;
  }
  const out = gain(lp(raw, 110, 0.8), low);
  for (const [f, q, g] of pipes) add(out, bp(raw, f, q), 0, g);
  if (prop > 0) {
    const fb = new Float32Array(n);
    for (let i = 0; i < n; i++) fb[i] = (rpm / 60) * gearing * blades * (rev ? rev[Math.min(i, rev.length - 1)] : 1);
    const blade = lp(buzz(n, fb, 'saw'), 1500, 0.7);
    const wash = bp(pink(n, r), 1100, 0.6);
    const beat = sine(n, fb);
    for (let i = 0; i < n; i++) out[i] += prop * (blade[i] * 0.55 + wash[i] * 2.2 * (0.6 + 0.4 * beat[i]));
  }
  if (whine > 0) {
    const fw = new Float32Array(n);
    for (let i = 0; i < n; i++) fw[i] = whineAt * (rev ? rev[Math.min(i, rev.length - 1)] : 1) * (1 + (hunt[i] - 0.5) * 0.01);
    add(out, sine(n, fw), 0, whine * 0.05); add(out, sine(n, fw.map((f) => f * 1.503)), 0, whine * 0.02);
  }
  return lp(out, tone, 0.7);
}

/** A tank's tracks: the clatter of the links over the sprocket and the squeal of dry pins. */
export function tracks(r, dur, { links = 13, weight = 1, squeal = 0.5, speed: sp = null } = {}) {
  const n = sec(dur), out = new Float32Array(n);
  let t = 0.02;
  while (t < dur - 0.05) {
    const s = sp ? sp[Math.min(sec(t), sp.length - 1)] : 1;
    if (s > 0.05) {
      const ng = sec(0.06);
      const link = mul(bp(white(ng, r), r.range(1300, 2600), r.range(1.5, 3)), decay(ng, r.range(0.006, 0.014), 0.0004));
      add(out, link, t, weight * r.range(0.45, 1) * Math.min(1, s));
      if (r.chance(0.3)) add(out, mul(sine(ng, r.range(520, 760)), decay(ng, 0.012, 0.0005)), t, weight * 0.3);
    }
    t += (1 / links) / Math.max(0.15, s) * r.range(0.9, 1.1);
  }
  if (squeal > 0) {
    const w = wander(n, 0.9, r, 1), w2 = wander(n, 2.3, r, 1);
    const sq = svf(white(n, r), 'bp', w2.map((v) => 2500 + 900 * v), 40);
    for (let i = 0; i < n; i++) out[i] += sq[i] * squeal * 0.6 * Math.pow(w[i], 3) * (sp ? Math.min(1, sp[Math.min(i, sp.length - 1)]) : 1);
  }
  const low = lp4(brown(n, r), 90);
  for (let i = 0; i < n; i++) out[i] += low[i] * 1.6 * weight * (sp ? Math.min(1, sp[Math.min(i, sp.length - 1)]) : 1);
  return out;
}

// ---- Railways --------------------------------------------------------------------------------------------

/** One beat of a steam engine's exhaust up the chimney. */
export function chuff(r, { tau = 0.09, tone = 900, stack = 210, weight = 1 } = {}) {
  const n = sec(tau * 7);
  let c = bp(white(n, r), tone * r.range(0.9, 1.1), 0.7);
  add(c, bp(white(n, r), stack, 2.5), 0, 0.9 * weight);
  add(c, hp(white(n, r), 4000), 0, 0.12);
  c = comb(c, 1 / stack / 2, 0.35, 0.4, 0.5);
  return mul(c, decay(n, tau, 0.012));
}
/** A wheel over a rail joint: iron on iron and the thud of the sleeper under it. */
export function railJoint(r, { weight = 1 } = {}) {
  const n = sec(0.16), out = new Float32Array(n);
  add(out, mul(sine(n, glide(n, 150, 78, 0.02)), decay(n, 0.03, 0.001)), 0, 0.9 * weight);
  add(out, mul(bp(white(n, r), r.range(1900, 3100), 2.2), decay(n, 0.006, 0.0003)), 0, 0.7);
  add(out, clang(r, { f0: r.range(900, 1300), dur: 0.16, tau: 0.03, partials: 4, bright: 0.6, click: 0 }), 0, 0.12);
  return out;
}
/** A steam whistle: one or more pipes blown together, breathy, sagging in pitch as it starts and stops. */
export function steamWhistle(r, dur, { notes = [1180], breath = 0.5, attack = 0.07, release = 0.16, sag = 0.07 } = {}) {
  const n = sec(dur + release + 0.05), out = new Float32Array(n);
  const env = line(n, [[0, 0], [attack, 1], [dur, 0.92], [dur + release, 0]], 1);
  const bend = new Float32Array(n);
  for (let i = 0; i < n; i++) { const t = i / SR; bend[i] = 1 - sag * Math.exp(-t / 0.05) - sag * 0.5 * smooth((t - dur) / release); }
  const air = white(n, r), drift = wander(n, 6, r, 1);
  for (const f of notes) {
    const pitch = bend.map((b, i) => f * b * (1 + (drift[i] - 0.5) * 0.004));
    const pipe = svf(air, 'bp', pitch, 55);
    const tone = sine(n, pitch), over = sine(n, pitch.map((p) => p * 2.004)), third = sine(n, pitch.map((p) => p * 3.01));
    for (let i = 0; i < n; i++) out[i] += (tone[i] * 0.5 + over[i] * 0.16 + third[i] * 0.07 + pipe[i] * 3.2 * breath) / notes.length;
  }
  const steam = hp(air, 2800);
  for (let i = 0; i < n; i++) out[i] = (out[i] + steam[i] * 0.05 * breath) * env[i];
  return out;
}
/** Brake blocks on wheel rims: a thin scream that drops as the train slows, over the grind of iron. */
export function brakeSqueal(r, dur, { from = 3400, to = 2500 } = {}) {
  const n = sec(dur), air = white(n, r), out = new Float32Array(n);
  const w = wander(n, 3, r, 1);
  for (const m of [1, 1.31, 1.74]) {
    const tone = new Float32Array(n);
    for (let i = 0; i < n; i++) tone[i] = lerp(from, to, i / n) * m * (1 + (w[i] - 0.5) * 0.02);
    add(out, svf(air, 'bp', tone, 70), 0, 1 / m);
  }
  const grind = bp(air, 520, 0.8);
  const env = line(n, [[0, 0], [dur * 0.15, 1], [dur * 0.8, 0.7], [dur, 0]]);
  for (let i = 0; i < n; i++) out[i] = (out[i] * 1.4 * (0.5 + 0.5 * w[i]) + grind[i] * 0.25) * env[i];
  return out;
}

// ---- A band, for the calls and flourishes -----------------------------------------------------------------

/** A brass note: it brightens as it swells, and wavers a little once it is held. */
export function brassNote(r, note, dur, { loud = 0.8, players = 3, bright = 1, vib = 0.006 } = {}) {
  const n = sec(dur + 0.25), out = new Float32Array(n);
  const f = midi(note);
  const env = line(n, [[0, 0], [0.035, 1], [0.12, 0.82], [dur, 0.7], [dur + 0.12, 0]]);
  for (let p = 0; p < players; p++) {
    const det = 1 + (p - (players - 1) / 2) * 0.0035 + r.bip() * 0.001;
    const pitch = new Float32Array(n);
    const rate = r.range(5, 5.9), late = r.range(0.12, 0.22);
    for (let i = 0; i < n; i++) { const t = i / SR; pitch[i] = f * det * (1 - 0.02 * Math.exp(-t / 0.018)) * (1 + vib * Math.sin(TAU * rate * t) * smooth((t - late) / 0.25)); }
    const raw = buzz(n, pitch, 'saw', 0.5, r.f());
    const cut = env.map((e) => f * (1.2 + 6.5 * bright * loud * e * e));
    add(out, svf(raw, 'lp', cut, 1.1), 0, 1 / players);
  }
  const breathy = bp(white(n, r), f * 3, 1.5);
  for (let i = 0; i < n; i++) out[i] = (out[i] + breathy[i] * 0.03) * env[i] * loud;
  return eq(out, 'peak', 1300, 3, 1.2);
}
/** A kettledrum. */
export function timpani(r, note, { loud = 1, dur = 2.2 } = {}) {
  const n = sec(dur), f = midi(note), out = new Float32Array(n);
  [[1, 1, 0.9], [1.5, 0.55, 0.6], [1.99, 0.4, 0.45], [2.44, 0.22, 0.3], [2.89, 0.12, 0.2]].forEach(([m, a, t]) => {
    const pitch = glide(n, f * m * 1.07, f * m, 0.03);
    add(out, mul(sine(n, pitch, r.f() * TAU), decay(n, t, 0.002)), 0, a);
  });
  const ns = sec(0.05);
  add(out, mul(lp(white(ns, r), 1800), decay(ns, 0.008, 0.0005)), 0, 0.7);
  return gain(fade(out, 0, Math.min(dur * 0.3, 0.5)), loud);
}
/** A side drum: one stroke. A roll is many of them. */
export function snare(r, { loud = 1, tau = 0.09 } = {}) {
  const n = sec(0.4), out = new Float32Array(n);
  add(out, mul(sine(n, glide(n, 240, 182, 0.01)), decay(n, 0.05, 0.0005)), 0, 0.7);
  add(out, mul(bp(white(n, r), 3200, 0.6), decay(n, tau, 0.0005)), 0, 0.8);
  add(out, mul(hp(white(n, r), 5500), decay(n, tau * 0.6, 0.0005)), 0, 0.25);
  return gain(fade(out, 0, 0.08), loud);
}
export function bassDrum(r, { loud = 1 } = {}) {
  const n = sec(1.2), out = new Float32Array(n);
  add(out, mul(sine(n, glide(n, 88, 46, 0.03)), decay(n, 0.32, 0.003)), 0, 1.2);
  const ns = sec(0.05);
  add(out, mul(lp(white(ns, r), 900), decay(ns, 0.01, 0.001)), 0, 0.5);
  return gain(fade(out, 0, 0.2), loud);
}
/** Low strings held: for the dark flourishes. */
export function stringsNote(r, note, dur, { loud = 0.6, players = 5 } = {}) {
  const n = sec(dur + 0.6), out = new Float32Array(n), f = midi(note);
  const env = line(n, [[0, 0], [0.25, 1], [dur, 0.85], [dur + 0.5, 0]]);
  for (let p = 0; p < players; p++) {
    const det = 1 + r.bip() * 0.006, rate = r.range(4.6, 6.2);
    const pitch = new Float32Array(n);
    for (let i = 0; i < n; i++) pitch[i] = f * det * (1 + 0.005 * Math.sin(TAU * rate * i / SR + p));
    add(out, lp(buzz(n, pitch, 'saw', 0.5, r.f()), f * 5, 0.8), 0, 1 / players);
  }
  return mul(eq(out, 'peak', 2400, -4, 0.8), env.map((e) => e * loud));
}

// ---- Finishing -------------------------------------------------------------------------------------------

/** Cuts the dead air off the end of a sound, leaving a short fade. */
export function crop(x, floorDb = -66, keep = 0.06) {
  const n = lengthOf(x), floor = db(floorDb);
  const L = isStereo(x) ? x.L : x, R = isStereo(x) ? x.R : x;
  let last = 0;
  for (let i = n - 1; i >= 0; i--) if (Math.abs(L[i]) > floor || Math.abs(R[i]) > floor) { last = i; break; }
  const end = Math.min(n, last + sec(keep));
  const out = isStereo(x) ? { L: x.L.slice(0, end), R: x.R.slice(0, end) } : x.slice(0, end);
  return fade(out, 0, Math.min(keep, 0.05));
}
/** The last step for a one-shot: trim, fade, bring to its loudness. */
export function oneShot(x, target, { fadeIn = 0.001, fadeOut = 0.03, floor = -62, ceiling = -1 } = {}) {
  const peakNow = peak(x);
  if (peakNow > 0) gain(x, 0.5 / peakNow);
  // No dead air in front: a sound that answers a click must start at once.
  const L = isStereo(x) ? x.L : x, R = isStereo(x) ? x.R : x;
  let first = 0; while (first < L.length - 1 && Math.abs(L[first]) < 0.0025 && Math.abs(R[first]) < 0.0025) first++;
  first = Math.max(0, first - sec(0.004));
  if (first > 0) x = isStereo(x) ? { L: x.L.slice(first), R: x.R.slice(first) } : x.slice(first);
  let out = crop(x, floor);
  fade(out, fadeIn, fadeOut);
  return level(out, target, { by: 'moment', ceiling });
}
/** The last step for a sound that goes round for ever. */
export function loopShot(x, target, { ceiling = -3 } = {}) {
  return level(x, target, { by: 'integrated', ceiling });
}
export { toMono };
