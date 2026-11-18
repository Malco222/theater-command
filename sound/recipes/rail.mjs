// The railways: a steam engine working, its whistle, its brakes, the buffers, and a train turning round at the end
// of its line.

import {
  SR, TAU, sec, db, clamp, lerp, smooth, rng, mono, stereo, add, place, gain, mul, line, decay, fade, wander,
  white, pink, brown, sine, buzz, svf, lp, hp, bp, lp4, eq, comb, drive, inPlace, far, loopOf, toMono,
} from '../kit/dsp.mjs';
import { blast, debris, clang, knock, glide, hiss, tick, chuff, railJoint, steamWhistle, brakeSqueal } from '../kit/parts.mjs';
import { bigBang } from './guns.mjs';

/**
 * A steam train over a stretch of time. "beats" is the exhaust beats a second through the stretch (four to a turn
 * of the wheels); where it is nothing the train stands. The wagons' wheels cross the rail joints behind it.
 */
function steamTrain(r, dur, beats, { wagons = 4, working = 1 } = {}) {
  const out = mono(dur + 0.6);
  const rate = (t) => beats[Math.min(sec(t), beats.length - 1)];
  let t = 0.02, k = 0;
  while (t < dur) {
    const b = rate(t);
    if (b < 0.4) { t += 0.05; continue; }
    const c = chuff(r, { tau: clamp(0.3 / b, 0.028, 0.16), tone: 800 + 60 * b, stack: 205, weight: 1 });
    add(out, c, t, working * (k % 4 === 0 ? 1 : 0.8) * r.range(0.9, 1));
    t += (1 / b) * r.range(0.985, 1.015); k++;
  }
  // The joints: a rail every 12 metres; the speed follows the exhaust (a beat is about 1.2 m of road).
  let x = 0, next = 3;
  for (let i = 0; i < sec(dur); i += 48) {
    const v = rate(i / SR) * 1.2; x += v * 48 / SR;
    if (x >= next) {
      next += 12;
      for (let w = 0; w < wagons; w++) for (const axle of [0, 1.9]) {
        const at = i / SR + (w * 7.5 + axle) / Math.max(v, 0.5);
        if (at < dur) add(out, railJoint(r, { weight: w === 0 ? 1.2 : 0.8 }), at, r.range(0.35, 0.6) * Math.min(1, v / 4));
      }
    }
  }
  const moving = new Float32Array(out.length);
  for (let i = 0; i < moving.length; i++) moving[i] = Math.min(1, rate(i / SR) / 5);
  add(out, mul(lp4(brown(out.length, r), 120), moving), 0, 1.0);
  add(out, mul(hp(white(out.length, r), 4500), moving.map((m) => 0.012 + 0.02 * m)), 0, 1);
  return out;
}
/** A run of buffers closing up down the train, or drawing out. */
function buffers(r, count, gap = 0.11) {
  const out = mono(count * gap + 1.2);
  for (let k = 0; k < count; k++) {
    add(out, clang(r, { f0: r.range(260, 420), dur: 0.9, tau: 0.16, partials: 7, bright: 0.7, click: 0.8 }), k * gap * r.range(0.85, 1.15), 0.8 - k * 0.07);
    add(out, mul(sine(sec(0.12), glide(sec(0.12), 110, 60, 0.02)), decay(sec(0.12), 0.04, 0.001)), k * gap, 0.6);
  }
  return out;
}
const peep = (r, dur) => steamWhistle(r, dur, { notes: [1420], breath: 0.45 });
const chime = (r, dur) => steamWhistle(r, dur, { notes: [392, 494, 587], breath: 0.6, attack: 0.1, sag: 0.09 });

export default function (def) {
  def('train_loop', { loop: true, ch: 1, lufs: -24, about: 'A steam train running at an easy speed: the quick beat of the exhaust, wheels over the rail joints, the rumble of wagons. Carried by each little train on the map; heard only close to.' }, (r) => {
    const dur = 9.6, beats = new Float32Array(sec(dur + 1)).fill(6.25); // sixty beats in 9.6 s, so the fold lands on a beat
    return loopOf(steamTrain(r, dur + 0.8, beats, { wagons: 4 }).slice(0, sec(dur + 0.8)), 0.8);
  });
  def('train', { ch: 1, same: 'train_depart', about: 'A train pulls out: a short whistle and the first heavy beats of the exhaust quickening. Under the name the 3D game already calls (it has been silent until now).' }, (r) => {
    const dur = 4.6, out = mono(dur + 0.5);
    add(out, peep(r, 0.45), 0.02, 0.5);
    const beats = line(sec(dur), [[0, 0], [0.6, 0], [0.62, 1.6], [dur, 6.5]], 0.8);
    add(out, steamTrain(r, dur, beats, { wagons: 3 }), 0, 1);
    return inPlace(fade(out, 0.01, 1.2), 'field', -17);
  });
  def('train_depart', { ch: 1, about: 'A heavy train starts away: the couplings draw out one by one, the wheels slip once, and the beat settles and quickens.' }, (r) => {
    const dur = 6.2, out = mono(dur + 0.5);
    add(out, buffers(r, 5, 0.16), 0.25, 0.35);
    const beats = line(sec(dur), [[0, 0], [0.2, 0], [0.22, 1.3], [1.3, 2.0], [1.32, 6.5], [1.9, 5.5], [1.92, 2.2], [dur, 6.2]], 1);
    add(out, steamTrain(r, dur, beats, { wagons: 4, working: 1.1 }), 0, 1);
    return inPlace(fade(out, 0.01, 1.4), 'field', -17);
  });
  def('train_arrive', { ch: 1, about: 'A train runs in and stops: the beat slows, the brakes squeal, the buffers close up down the train and steam is let go.' }, (r) => {
    const dur = 6.4, out = mono(dur + 1);
    const beats = line(sec(dur), [[0, 6.5], [2.0, 3.6], [3.6, 1.0], [3.9, 0], [dur, 0]], 1);
    add(out, mul(steamTrain(r, dur, beats, { wagons: 4, working: 0.6 }), line(sec(dur + 0.6), [[0, 0], [0.8, 1], [dur, 1]])), 0, 1);
    add(out, brakeSqueal(r, 2.4), 1.7, 0.16);
    add(out, buffers(r, 5, 0.1), 3.95, 0.5);
    add(out, hiss(r, 2.2, { tone: 5200, tau: 0.7, attack: 0.04, hold: 0.25 }), 4.4, 0.3);
    return inPlace(out, 'field', -17);
  });
  def('train_turn_round', { ch: 1, about: 'A train reaches the end of its line and turns back: it brakes to a stand, the buffers close, steam blows off, one short whistle, and it pulls away the other way. Played at the end of a line as a little train turns round, in place of train_loop for these seconds.' }, (r) => {
    const dur = 8.2, out = mono(dur + 1);
    const beats = line(sec(dur), [[0, 5.5], [1.2, 2.6], [2.2, 0.8], [2.4, 0], [4.9, 0], [4.92, 1.5], [dur, 5.8]], 1);
    add(out, steamTrain(r, dur, beats, { wagons: 3, working: 0.9 }), 0, 1);
    add(out, brakeSqueal(r, 1.7), 0.8, 0.14);
    add(out, buffers(r, 4, 0.1), 2.45, 0.45);
    add(out, hiss(r, 1.8, { tone: 5000, tau: 0.5, attack: 0.04, hold: 0.2 }), 2.8, 0.25);
    add(out, peep(r, 0.3), 4.35, 0.42);
    add(out, buffers(r, 4, 0.15), 4.95, 0.3);
    return inPlace(fade(out, 0.3, 1.4), 'field', -17);
  });
  def('train_whistle_01', { ch: 1, lufs: -22, about: 'An engine\'s whistle, high and clear: one short and one long.' }, (r) => {
    const out = mono(2.6);
    add(out, peep(r, 0.28), 0.02, 1); add(out, peep(r, 1.15), 0.62, 1);
    return inPlace(far(out, 120), 'valley', -13);
  });
  def('train_whistle_02', { ch: 1, lufs: -22, about: 'A deep three-note whistle, long and lonely: the big engines of the Americas and the steppe.' }, (r) => inPlace(far(chime(r, 2.1), 160), 'valley', -12));
  def('rail_ride', { ch: 1, about: 'An army is sent by rail: the guard\'s whistle, carriage doors slammed down the platform, the engine\'s answer and the first beats away. Played when the order to ride is given: a train, never boots.' }, (r) => {
    const dur = 5.4, out = mono(dur + 0.6);
    // The guard's whistle has a pea in it: a high note that flutters.
    const n = sec(0.75), pea = mono(0.75);
    const flutter = sine(n, 23);
    add(pea, mul(svf(white(n, r), 'bp', 2850, 30), flutter.map((v) => 0.6 + 0.4 * v)), 0, 3);
    add(pea, mul(sine(n, 2850), flutter.map((v) => 0.25 + 0.2 * v)), 0, 1);
    add(out, mul(pea, line(n, [[0, 0], [0.03, 1], [0.6, 0.9], [0.75, 0]])), 0.02, 0.3);
    for (const t of [0.75, 1.02, 1.21, 1.52]) {
      // A carriage door: a flat wooden slam, the thud of the frame, the click of the catch.
      const n = sec(0.25), door = mono(0.25);
      add(door, mul(lp(white(n, r), r.range(600, 900)), decay(n, 0.03, 0.001)), 0, 1.6);
      add(door, mul(sine(n, glide(n, 105, 58, 0.02)), decay(n, 0.045, 0.001)), 0, 1);
      add(door, tick(r, { tone: 2600, body: 1500, tau: 0.003 }), 0.012, 0.5);
      add(out, door, t, r.range(0.6, 0.9));
    }
    add(out, peep(r, 0.5), 1.95, 0.45);
    const beats = line(sec(dur), [[0, 0], [2.6, 0], [2.62, 1.5], [dur, 5.6]], 0.9);
    add(out, steamTrain(r, dur, beats, { wagons: 3 }), 0, 1);
    return inPlace(fade(out, 0.005, 1.3), 'city', -17);
  });
  def('rail_cut', { ch: 1, lufs: -16, about: 'A railway is cut: the charge or the bomb, rails wrenched and ringing, ballast coming down.' }, (r) => {
    const out = mono(6);
    add(out, bigBang(r.fork(), 0.85), 0, 1);
    for (const t of [0.28, 0.55, 0.95]) add(out, clang(r, { f0: r.range(300, 520), dur: 1.6, tau: 0.45, partials: 9, bright: 0.78, click: 0.6 }), t, r.range(0.12, 0.2));
    return inPlace(out, 'valley', -13);
  });
  def('rail_mended', { ch: 1, lufs: -22, about: 'A line is open again: the last spikes driven home and an engine\'s whistle to try the road.' }, (r) => {
    const out = mono(3.6);
    for (const t of [0.02, 0.36, 0.7, 1.04]) { add(out, clang(r, { f0: 640 * r.range(0.98, 1.03), dur: 0.7, tau: 0.13, partials: 8, bright: 0.74, click: 1 }), t, 0.8); add(out, mul(sine(sec(0.1), glide(sec(0.1), 130, 70, 0.02)), decay(sec(0.1), 0.03, 0.001)), t, 0.5); }
    add(out, far(peep(r, 0.7), 150), 1.7, 0.5);
    return inPlace(out, 'valley', -15);
  });
}
