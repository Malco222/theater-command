// The war at sea: horns and bells, the great guns, a fleet under way, the hunt for a submarine, a ship going down.

import {
  SR, TAU, sec, db, clamp, lerp, smooth, rng, mono, stereo, add, place, gain, mul, line, decay, fade, wander,
  white, pink, brown, sine, buzz, svf, lp, hp, bp, lp4, eq, comb, drive, inPlace, far, loopOf, toMono,
} from '../kit/dsp.mjs';
import { blast, debris, clang, bell, knock, engine, glide, hiss, tick, swish } from '../kit/parts.mjs';
import { crump, bigBang } from './guns.mjs';

/** A big naval gun: slower, deeper and longer than anything on land. */
const navalGun = (r, size = 1) => blast(r, { dur: 5.5, crack: 0.7, crackTone: 1600, body: 1, bodyFrom: 3600 * r.range(0.85, 1.15), bodyTo: 110, bodyTime: 0.2 * size, grit: 3.2, thump: 1.4, thumpFrom: 82 * r.range(0.9, 1.1), thumpTo: 25, thumpTime: 0.55 * size, rumble: 1.1, rumbleTone: 170, rumbleTime: 1.9 * size, rumbleRoll: 3 });
/** Water thrown up and falling back. */
const plume = (r, dur = 2.6) => mul(svf(white(sec(dur), r), 'bp', glide(sec(dur), 3600, 800, dur * 0.35), 0.7), line(sec(dur), [[0, 0], [0.14, 1], [dur * 0.4, 0.55], [dur, 0]], 1.4));
/** A ship's horn: a deep reed, slow to speak, with a second horn a little out of tune beside it. */
function horn(r, dur, f = 98, second = 1.26) {
  const n = sec(dur + 0.5), out = mono(dur + 0.5);
  const env = line(n, [[0, 0], [0.22, 1], [dur, 0.95], [dur + 0.35, 0]], 1);
  for (const [m, g] of [[1, 1], [second, 0.7]]) {
    const pitch = new Float32Array(n);
    for (let i = 0; i < n; i++) { const t = i / SR; pitch[i] = f * m * (1 - 0.06 * Math.exp(-t / 0.09)) * (1 + 0.003 * Math.sin(TAU * 3.7 * t + m)); }
    add(out, lp(buzz(n, pitch, 'pulse', 0.38, r.f()), 950, 1.1), 0, g * 0.6);
  }
  const breath = bp(white(n, r), 700, 0.8);
  for (let i = 0; i < n; i++) out[i] = (out[i] + breath[i] * 0.05) * env[i];
  return eq(out, 'peak', 240, 4, 1);
}
/** The sea against a hull under way. */
const wash = (r, dur) => { const n = sec(dur), w = wander(n, 0.35, r, 1); const out = svf(pink(n, r), 'bp', w.map((v) => 500 + 900 * v), 0.6); for (let i = 0; i < n; i++) out[i] *= 0.55 + 0.45 * w[i]; return out; };

export default function (def) {
  def('ship_horn', { ch: 1, same: 'ship_horn_01', about: 'A big ship\'s horn: deep, slow to speak, carrying over the water. Played when a fleet is picked up, sails or changes its orders.' }, (r) => inPlace(far(horn(r, 2.0), 150), 'sea', -10));
  def('ship_horn_02', { ch: 1, about: 'A destroyer\'s siren: three rising whoops. For escorts and submarine hunters.' }, (r) => {
    const out = mono(3.2);
    for (let k = 0; k < 3; k++) {
      const n = sec(0.62), pitch = line(n, [[0, 330], [0.45, 880], [0.62, 860]], 1);
      const w = svf(buzz(n, pitch, 'pulse', 0.3), 'bp', pitch.map((f) => f * 1.8), 1.6);
      add(out, mul(w, line(n, [[0, 0], [0.05, 1], [0.5, 1], [0.62, 0]])), 0.02 + k * 0.78, 0.8);
    }
    return inPlace(far(out, 200), 'sea', -10);
  });
  def('ship_bell', { fam: 'desk', ch: 1, lufs: -24, about: 'A ship\'s bell: two pairs of strokes. For the fleet\'s cards and orders, as the stamp is for the army\'s.' }, (r) => {
    const out = mono(3.4);
    for (const t of [0.02, 0.3, 0.95, 1.23]) add(out, bell(r, { f0: 640, dur: 2.2, tau: 0.9, bright: 0.75, strike: 0.6 }), t, r.range(0.8, 1));
    return inPlace(out, 'sea', -16);
  });
  for (const [name, same, guns, about] of [
    ['ship_guns', 'ship_guns_01', [0, 0.22, 0.47], 'A salvo from a turret of heavy guns: three reports close together and the long roll back across the water.'],
    ['ship_guns_02', null, [0, 0.16, 0.3, 1.25, 1.42, 1.6], 'A full broadside: two turrets, one after the other.'],
  ]) def(name, { ch: 1, lufs: -15, same, about }, (r) => {
    const out = mono(8);
    for (const t of guns) add(out, far(navalGun(r.fork(), r.range(0.9, 1.1)), r.range(120, 260)), 0.02 + t, r.range(0.8, 1));
    return inPlace(drive(out, 1.3), 'sea', -9);
  });
  def('shore_bombardment', { ch: 1, lufs: -16, about: 'A fleet shells the coast: the salvo, the shells rumbling away overhead, and the bursts inland a few seconds later.' }, (r) => {
    const out = mono(9);
    for (const t of [0, 0.2, 0.43]) add(out, far(navalGun(r.fork()), 200), 0.02 + t, 0.9);
    const over = mul(svf(pink(sec(2.6), r), 'bp', glide(sec(2.6), 900, 260, 1.2), 1.4), line(sec(2.6), [[0, 0], [0.5, 1], [2.6, 0]], 1.3));
    add(out, over, 0.5, 0.5);
    for (const t of [3.1, 3.35, 3.7]) add(out, far(crump(r.fork(), 1.2), r.range(900, 1500)), t, 0.7);
    return inPlace(out, 'sea', -10);
  });
  def('ship_loop', { loop: true, ch: 1, lufs: -25, about: 'A fleet under way: the slow thrum of engines and the sea along the hull. Carried by each fleet at sea; heard only close to.' }, (r) => {
    const dur = 10.4, n = sec(dur), out = mono(dur);
    add(out, lp(buzz(n, 41, 'saw', 0.5), 260), 0, 0.3);
    const beat = sine(n, 3.2);
    const thr = lp4(brown(n, r), 95);
    for (let i = 0; i < n; i++) out[i] += thr[i] * 2.4 * (0.6 + 0.4 * beat[i]);
    add(out, wash(r, dur), 0, 2.2);
    add(out, sine(n, 1870), 0, 0.006); // a whisper of turbine
    return loopOf(out, 1.6);
  });
  def('anchor_chain', { ch: 1, lufs: -22, about: 'A fleet comes to anchor: the cable roaring out through the hawse and the splash. For a fleet sent to harbour.' }, (r) => {
    const out = mono(3.6);
    for (let t = 0.02; t < 1.9; t += r.range(0.028, 0.05)) add(out, clang(r, { f0: r.range(380, 620), dur: 0.15, tau: 0.02, partials: 5, bright: 0.7, click: 0.8 }), t, r.range(0.3, 0.7) * (1 - t / 3));
    add(out, plume(r, 1.6), 1.85, 0.5);
    add(out, mul(sine(sec(0.3), glide(sec(0.3), 90, 45, 0.05)), decay(sec(0.3), 0.1, 0.004)), 1.85, 0.8);
    return inPlace(out, 'sea', -15);
  });
  def('sonar_ping', { ch: 1, lufs: -24, about: 'The submarine hunter\'s ping going out, and its faint echo coming back off something. For destroyers hunting and for a submarine found.' }, (r) => {
    const out = mono(3.6), n = sec(1.4);
    const ping = (f, g) => { const p = mono(1.4); add(p, mul(sine(n, f), decay(n, 0.22, 0.002)), 0, 1); add(p, mul(sine(n, f * 2.005), decay(n, 0.08, 0.002)), 0, 0.2); return gain(p, g); };
    add(out, ping(2850, 1), 0.02, 1);
    add(out, lp(ping(2838, 0.22), 2600), 1.25, 1);
    return inPlace(out, 'iron', -9, { rt: 2.4, dark: 1200 });
  });
  def('depth_charge', { ch: 1, lufs: -16, about: 'A depth charge: the splash, the deep muffled blow underneath, and the sea heaving up and falling back.' }, (r) => {
    const out = mono(6.5);
    add(out, plume(r, 0.7), 0.02, 0.25);
    add(out, lp4(blast(r, { dur: 4, crack: 0, body: 1, bodyFrom: 700, bodyTo: 70, bodyTime: 0.3, grit: 3, thump: 1.6, thumpFrom: 66, thumpTo: 24, thumpTime: 0.6, rumble: 1.2, rumbleTone: 120, rumbleTime: 1.6 }), 420), 1.0, 1.5);
    add(out, plume(r, 3.2), 1.45, 0.75);
    return inPlace(out, 'sea', -12);
  });
  def('torpedo_hit', { ch: 1, lufs: -15, about: 'A torpedo strikes: a blow against the hull, a column of water, and plates ringing.' }, (r) => {
    const out = mono(7);
    add(out, bigBang(r.fork(), 1), 0.02, 1);
    add(out, plume(r, 3.4), 0.12, 0.7);
    add(out, clang(r, { f0: 140, dur: 2.5, tau: 0.8, partials: 10, bright: 0.8, click: 0 }), 0.05, 0.2);
    return inPlace(out, 'sea', -11);
  });
  def('ship_sinking', { ch: 1, lufs: -19, about: 'A ship goes down: her plates groan, the sea pours in, air and steam come roaring up, and she is gone. Played with the picture of a ship sinking.' }, (r) => {
    const dur = 8.5, n = sec(dur), out = mono(dur);
    for (let k = 0; k < 5; k++) { // the hull groaning as it breaks
      const len = r.range(0.9, 1.9), m = sec(len), f = r.range(55, 120);
      const g = svf(buzz(m, wander(m, 2.2, r, 1).map((v) => f * (0.75 + 0.6 * v)), 'saw'), 'bp', r.range(260, 720), 6);
      add(out, mul(g, line(m, [[0, 0], [len * 0.3, 1], [len, 0]])), r.range(0.1, 4.6), r.range(0.25, 0.5));
    }
    const rush = mul(svf(pink(n, r), 'bp', line(n, [[0, 500], [5, 1500], [dur, 500]]), 0.7), line(n, [[0, 0], [1.5, 0.5], [5.2, 1], [7, 0.4], [dur, 0]]));
    add(out, rush, 0, 2.0);
    for (let t = 2.2; t < 7.8; t += r.range(0.03, 0.16)) { // air bubbling up
      const m = sec(0.09), f = r.range(180, 620);
      add(out, mul(sine(m, glide(m, f, f * 1.9, 0.05)), decay(m, 0.02, 0.004)), t, r.range(0.1, 0.3) * smooth((t - 2.2) / 2));
    }
    add(out, hiss(r, 3.2, { tone: 4200, tau: 1.0, attack: 0.5, hold: 0.6 }), 3.6, 0.3);
    return inPlace(out, 'sea', -12);
  });
  def('submarine_dive', { ch: 1, lufs: -21, about: 'A submarine dives: two blasts of her horn, the vents roaring, the sea closing over.' }, (r) => {
    const out = mono(4.6);
    for (const t of [0.02, 0.62]) {
      const n = sec(0.42), pitch = line(n, [[0, 300], [0.07, 470], [0.3, 470], [0.42, 380]]);
      add(out, mul(svf(buzz(n, pitch, 'pulse', 0.25), 'bp', 1100, 1.2), line(n, [[0, 0], [0.03, 1], [0.34, 1], [0.42, 0]])), t, 0.5);
    }
    add(out, hiss(r, 2.6, { tone: 3200, low: 900, tau: 0.8, attack: 0.25, hold: 0.5 }), 1.2, 0.5);
    add(out, plume(r, 2.2), 2.0, 0.45);
    return inPlace(out, 'sea', -14);
  });
  def('landing_craft', { ch: 1, lufs: -20, about: 'Troops go ashore: the craft\'s engines, the surf, the ramp chains running and the ramp down on the beach. For a landing ordered and a landing made.' }, (r) => {
    const dur = 5.6, n = sec(dur), out = mono(dur);
    const eng = engine(r, dur, { cyl: 6, rpm: 1700, rough: 0.4, pipes: [[85, 1.4, 1], [260, 1.1, 0.7], [700, 0.9, 0.3]], tone: 1600, low: 0.9 });
    add(out, mul(eng, line(n, [[0, 0], [0.5, 1], [3.0, 0.9], [4.2, 0.4], [dur, 0]])), 0, 0.8);
    for (const t of [0.4, 2.3]) add(out, plume(r, 2.4), t, 0.4); // surf
    for (let t = 3.0; t < 3.7; t += r.range(0.03, 0.05)) add(out, clang(r, { f0: r.range(500, 800), dur: 0.12, tau: 0.02, partials: 4, bright: 0.7, click: 0.7 }), t, 0.3);
    add(out, knock(r, { f: [90, 170, 330], tau: 0.09, weight: 1.6, dur: 0.5 }), 3.75, 1.3);
    add(out, plume(r, 1.3), 3.78, 0.5);
    return inPlace(out, 'sea', -14);
  });
  def('carrier_launch', { ch: 1, lufs: -20, about: 'An aircraft leaves a carrier\'s deck: the engine opened right up, the run, and away over the bow.' }, (r) => {
    const dur = 4.8, n = sec(dur + 1);
    const rev = line(n, [[0, 0.7], [0.6, 1.0], [dur + 1, 1.05]]);
    const e = engine(r, dur + 1, { cyl: 14, rpm: 2600, rev, rough: 0.3, pipes: [[300, 1.7, 1], [480, 1.1, 0.7], [1000, 0.9, 0.42], [2200, 0.8, 0.16]], prop: 0.9, tone: 3600 });
    const out = mono(dur);
    for (let i = 0; i < sec(dur); i++) { const t = i / SR; out[i] = e[i] * smooth(t / 0.4) * (1 - 0.85 * smooth((t - 2.2) / 2.4)); }
    add(out, mul(lp4(brown(sec(1.8), r), 140), line(sec(1.8), [[0, 0], [0.4, 1], [1.8, 0]])), 0.6, 1.2); // wheels on the deck planking
    return inPlace(svf(out, 'lp', line(sec(dur), [[0, 5000], [2.2, 5000], [dur, 900]]), 0.7), 'sea', -15);
  });
  def('sea_battle_loop', { loop: true, ch: 2, lufs: -23, about: 'A fleet action heard across the water: salvoes far and near, shells falling in the sea. The bed under a sea battle that is being watched.' }, (r) => {
    const dur = 18, out = stereo(dur + 6);
    for (let t = 0.1; t < dur; t += r.range(0.7, 2.2)) { const pn = r.range(-0.8, 0.8), d = r.range(400, 1400); for (let k = 0; k < r.pick([2, 3, 3, 4]); k++) place(out, far(navalGun(r.fork()), d), t + k * r.range(0.15, 0.3), pn, r.range(0.5, 0.9)); }
    for (let t = 0.5; t < dur; t += r.range(1.0, 2.6)) place(out, far(plume(r, 2.4), 500), t, r.range(-0.8, 0.8), 0.25);
    const wet = inPlace({ L: out.L.slice(0, sec(dur)), R: out.R.slice(0, sec(dur)) }, 'sea', -10);
    return loopOf({ L: wet.L.slice(0, sec(dur)), R: wet.R.slice(0, sec(dur)) }, 2);
  });
}
