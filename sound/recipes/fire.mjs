// Fire: a building alight, a town burning by night, a roof falling in.

import {
  SR, TAU, sec, db, clamp, lerp, smooth, rng, mono, stereo, add, place, gain, mul, line, decay, fade, wander,
  white, pink, brown, sine, buzz, svf, lp, hp, bp, lp4, eq, comb, drive, inPlace, far, loopOf, toMono,
} from '../kit/dsp.mjs';
import { blast, debris, clang, knock, glide, hiss } from '../kit/parts.mjs';

/** The crackle of burning timber: thousands of tiny snaps, thicker and thinner by turns, and now and then a loud one. */
function crackle(r, dur, { thick = 60, loud = 1 } = {}) {
  const n = sec(dur), out = mono(dur), breath = wander(n, 0.7, r, 1);
  let t = 0;
  while (t < dur) {
    const b = breath[Math.min(sec(t), n - 1)];
    t += -Math.log(1 - r.f()) / (thick * (0.3 + 1.2 * b));
    const big = r.chance(0.04), m = sec(big ? 0.05 : 0.012);
    const snap = mul(bp(white(m, r), r.range(big ? 600 : 1800, big ? 2400 : 7500), r.range(1, 3)), decay(m, big ? 0.008 : r.range(0.0008, 0.003), 0.0002));
    add(out, snap, t, (big ? 1 : r.range(0.08, 0.5)) * loud);
  }
  return out;
}
/** The roar: air drawn into a fire, low and unsteady. */
function roar(r, dur, { lo = 110, hi = 620, depth = 0.6 } = {}) {
  const n = sec(dur), w = wander(n, 0.5, r, 1), f = wander(n, 3.1, r, 1);
  const out = svf(brown(n, r), 'bp', w.map((v) => lerp(lo, hi, v)), 0.6);
  for (let i = 0; i < n; i++) out[i] *= ((1 - depth) + depth * w[i]) * (0.8 + 0.2 * f[i]);
  return out;
}
const collapse = (r, size = 1) => {
  const out = mono(4.5);
  for (let k = 0; k < 4; k++) add(out, knock(r, { f: [r.range(70, 110), r.range(170, 260), r.range(380, 520)], tau: 0.09, weight: 1.5, dur: 0.6 }), r.range(0, 0.5), r.range(0.5, 1)); // timbers going
  add(out, blast(r, { dur: 3.5, crack: 0.1, body: 1, bodyFrom: 1800, bodyTo: 120, bodyTime: 0.35 * size, grit: 2.4, thump: 0.9, thumpFrom: 75, thumpTo: 32, thumpTime: 0.45, rumble: 1, rumbleTone: 240, rumbleTime: 1.3 * size }), 0.35, 1);
  add(out, debris(r, 3.6, { count: 60, from: 0.3, to: 3.0, lo: 600, hi: 5000, level: 0.3 }), 0.1, 1);
  add(out, mul(crackle(r, 2.5, { thick: 160 }), line(sec(2.5), [[0, 0], [0.4, 1], [2.5, 0]])), 0.5, 0.6); // sparks going up
  return out;
};

export default function (def) {
  def('fire_small_loop', { loop: true, ch: 1, lufs: -29, about: 'One building alight: a steady crackle with a small roar under it. Carried by a siege fire, a burning depot, a works that has been hit.' }, (r) => {
    const dur = 11, out = crackle(r, dur, { thick: 55 });
    add(out, roar(r, dur, { lo: 140, hi: 520 }), 0, 0.5);
    return loopOf(out, 1.6);
  });
  def('fire_town_loop', { loop: true, ch: 2, lufs: -26, about: 'A town burning by night: the roar of many fires drawing air, the crackle on top, timbers going now and then. Carried by a bombed town for as long as its smoke and its red glow last.' }, (r) => {
    const dur = 20, out = stereo(dur);
    for (const pn of [-0.6, 0.1, 0.7]) { place(out, roar(r.fork(), dur, { lo: 90, hi: 560, depth: 0.7 }), 0, pn, 1.0); place(out, far(crackle(r.fork(), dur, { thick: 90 }), 60), 0, pn * 1.2, 0.5); }
    for (let t = 2.5; t < dur - 3; t += r.range(4.5, 8)) place(out, far(collapse(r.fork(), 0.7), 220), t, r.range(-0.7, 0.7), 0.35);
    const w = inPlace(out, 'city', -15);
    return loopOf({ L: w.L.slice(0, sec(dur)), R: w.R.slice(0, sec(dur)) }, 2.2);
  });
  def('fire_start', { ch: 1, lufs: -24, about: 'Something is set alight: the whoosh as it catches and the first crackle. For the order to burn stores, bridges or a town\'s works before a retreat.' }, (r) => {
    const out = mono(3.4), n = sec(1.2);
    add(out, mul(svf(pink(n, r), 'bp', glide(n, 300, 1500, 0.4), 0.8), line(n, [[0, 0], [0.25, 1], [1.2, 0]], 1.4)), 0.02, 1.2);
    add(out, mul(crackle(r, 3.2, { thick: 80 }), line(sec(3.2), [[0, 0], [0.5, 1], [2.2, 0.8], [3.2, 0]])), 0.15, 0.6);
    add(out, mul(roar(r, 3.2), line(sec(3.2), [[0, 0], [0.6, 1], [3.2, 0]])), 0.1, 0.5);
    return inPlace(out, 'field', -18);
  });
  def('fire_collapse_01', { ch: 1, lufs: -19, about: 'A burning roof falls in: timbers crack, the fall, a rush of sparks.' }, (r) => inPlace(collapse(r, 1), 'city', -13));
  def('fire_collapse_02', { ch: 1, lufs: -19, about: 'A wall comes down in a burning street, a second voice.' }, (r) => inPlace(collapse(r, 1.25), 'city', -13));
}
