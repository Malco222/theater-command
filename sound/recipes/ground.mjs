// The army on the ground: tanks, lorries, boots and hooves, and men at work with shovels and hammers.

import {
  SR, TAU, sec, db, clamp, lerp, smooth, rng, mono, stereo, add, place, gain, mul, line, decay, fade, wander,
  white, pink, brown, sine, buzz, svf, lp, hp, bp, lp4, eq, comb, drive, inPlace, far, loopOf, toMono,
} from '../kit/dsp.mjs';
import { blast, debris, clang, knock, engine, tracks, glide, swish, hiss, tick } from '../kit/parts.mjs';

const tankEngine = (r, dur, rev, rpm = 1500) => engine(r, dur, { cyl: 12, rpm, rev, rough: 0.42, sharp: 2.6, pipes: [[rpm / 60 * 6 * 0.5, 1.5, 1.0], [210, 1.2, 0.9], [520, 1.0, 0.45], [1300, 0.8, 0.15]], tone: 1500, breath: 0.5, low: 0.9 });
const lorryEngine = (r, dur, rev, rpm = 2100) => engine(r, dur, { cyl: 6, rpm, rev, rough: 0.3, sharp: 3.0, pipes: [[rpm / 60 * 3, 1.5, 1.0], [320, 1.1, 0.7], [800, 0.9, 0.4], [1900, 0.8, 0.15]], tone: 2400, breath: 0.4, low: 0.6 });

/** One footfall of a column: many boots landing almost together on a metalled road. */
function footfall(r, men, left) {
  const out = mono(0.5);
  for (let m = 0; m < men; m++) {
    const at = 0.06 + r.gauss() * 0.014;
    const n = sec(0.2);
    const boot = mono(0.2);
    add(boot, mul(sine(n, glide(n, r.range(120, 170), r.range(70, 95), 0.015)), decay(n, 0.028, 0.001)), 0, 0.9);
    add(boot, mul(bp(white(n, r), r.range(left ? 1300 : 1600, left ? 2300 : 2800), 1.2), decay(n, r.range(0.012, 0.03), 0.0006)), 0, 0.8);
    add(boot, mul(hp(white(n, r), 3800), decay(n, 0.045, 0.004)), 0.012, 0.15); // the scrape of grit
    add(out, boot, Math.max(0, at), r.range(0.4, 1) / Math.sqrt(men));
  }
  return out;
}
function column(r, steps, pace = 0.5, men = 26) {
  const out = mono(steps * pace + 0.6);
  for (let s = 0; s < steps; s++) {
    add(out, footfall(r, men, s % 2 === 0), s * pace, s % 2 === 0 ? 1 : 0.88);
    for (let k = 0; k < 3; k++) { // the rattle of kit: mess tins, buckles, slings
      const j = clang(r, { f0: r.range(2600, 5200), dur: 0.08, tau: 0.012, partials: 4, bright: 0.7, click: 0.3 });
      add(out, j, s * pace + r.range(0.03, 0.45), r.range(0.02, 0.06));
    }
  }
  return out;
}
function hoof(r, hard) {
  const n = sec(0.14), out = mono(0.14);
  add(out, mul(sine(n, glide(n, r.range(820, 1150), r.range(380, 520), 0.006)), decay(n, 0.014, 0.0005)), 0, hard ? 0.8 : 0.3);
  add(out, mul(bp(white(n, r), r.range(700, 1400), 2.2), decay(n, 0.016, 0.0006)), 0, 0.8);
  add(out, mul(sine(n, glide(n, 130, 70, 0.02)), decay(n, 0.03, 0.001)), 0, 0.7);
  return out;
}
function riders(r, dur, horses = 5) {
  const out = mono(dur + 0.4);
  for (let h = 0; h < horses; h++) {
    const stride = r.range(0.4, 0.46), start = r.range(0, stride);
    for (let t = start; t < dur; t += stride * r.range(0.97, 1.03)) for (const beat of [0, 0.105, 0.2]) add(out, hoof(r, false), t + beat + r.range(-0.008, 0.008), r.range(0.5, 1) / Math.sqrt(horses) * (beat === 0.105 ? 0.8 : 1));
    for (let t = r.range(0.1, 0.8); t < dur; t += r.range(0.5, 1.3)) add(out, clang(r, { f0: r.range(2800, 4600), dur: 0.1, tau: 0.02, partials: 4, bright: 0.7, click: 0.2 }), t, 0.03); // bits and stirrups
  }
  return out;
}

export default function (def) {
  // ---- Armour and motors -------------------------------------------------------------------------------------
  def('tank_engine', { ch: 1, about: 'An armoured corps sets off: the engines take up the load and the tracks begin to clatter. Played when armour is picked up or ordered to move.' }, (r) => {
    const dur = 4.4, n = sec(dur);
    const rev = line(n, [[0, 0.62], [0.5, 0.66], [1.5, 1.0], [dur, 1.06]], 1), go = line(n, [[0, 0], [0.9, 0], [2.2, 1], [dur, 1.1]], 1);
    const out = tankEngine(r, dur, rev);
    add(out, tankEngine(r.fork(), dur, rev.map((v) => v * 0.97), 1440), 0.1, 0.6);
    add(out, tracks(r, dur, { links: 12, weight: 1, squeal: 0.5, speed: go }), 0, 0.5);
    return inPlace(fade(drive(out, 1.4), 0.15, 0.9), 'field', -18);
  });
  def('tank_loop', { loop: true, ch: 1, lufs: -22, about: 'Tanks on the move: engines and tracks, going round for ever. Carried by an armoured army while it marches across the map.' }, (r) => {
    const out = tankEngine(r, 9.4, null, 1560);
    add(out, tankEngine(r.fork(), 9.4, null, 1505), 0, 0.7);
    add(out, tracks(r, 9.4, { links: 13, weight: 1, squeal: 0.6 }), 0, 0.5);
    return loopOf(drive(out, 1.3), 1.4);
  });
  def('truck_engine', { ch: 1, about: 'Lorries pull out: a motorised corps sets off, the gears whining as they pick up speed.' }, (r) => {
    const dur = 3.8, n = sec(dur);
    const rev = line(n, [[0, 0.5], [0.4, 0.55], [1.5, 1.05], [1.75, 0.78], [3.0, 1.0], [dur, 1.03]], 1);
    const out = lorryEngine(r, dur, rev);
    add(out, lorryEngine(r.fork(), dur, rev.map((v) => v * 0.96), 2000), 0.25, 0.7);
    const gears = sine(n, rev.map((v) => 520 * v));
    add(out, mul(gears, rev.map((v) => 0.03 * v)), 0, 1);
    return inPlace(fade(out, 0.12, 0.9), 'field', -18);
  });
  def('halftrack', { ch: 1, about: 'Half-tracks pull out: a lorry\'s engine with tracks under it. For German and American motorised corps.' }, (r) => {
    const dur = 4.0, n = sec(dur);
    const rev = line(n, [[0, 0.55], [0.5, 0.6], [1.7, 1.0], [dur, 1.05]], 1), go = line(n, [[0, 0], [0.7, 0], [1.9, 1], [dur, 1.1]], 1);
    const out = lorryEngine(r, dur, rev, 1900);
    add(out, tracks(r, dur, { links: 16, weight: 0.6, squeal: 0.3, speed: go }), 0, 0.4);
    return inPlace(fade(out, 0.12, 0.9), 'field', -18);
  });
  def('convoy_loop', { loop: true, ch: 1, lufs: -23, about: 'A column of lorries on the road. Carried by a motorised army while it marches.' }, (r) => {
    const out = mono(9.4);
    for (let k = 0; k < 3; k++) add(out, mul(lorryEngine(r.fork(), 9.4, null, 1950 + k * 90), wander(sec(9.4), 0.3, r, 0.5)), 0, [1, 0.7, 0.6][k]);
    return loopOf(out, 1.4);
  });

  // ---- Men and horses ------------------------------------------------------------------------------------------
  def('marching', { ch: 1, about: 'Infantry steps off: eight paces of a column on a road, boots together and the rattle of kit. Played when infantry is picked up or ordered to march.' }, (r) => inPlace(fade(column(r, 8), 0.02, 0.5), 'field', -18));
  def('marching_loop', { loop: true, ch: 1, lufs: -25, about: 'A column marching, going round for ever at the same pace. Carried by an infantry army while it marches across the map.' }, (r) => {
    // Sixteen paces, then folded: the pace is exact, so the fold lands on a footfall.
    const c = column(r, 18, 0.5, 30);
    return loopOf(c.slice(0, sec(9)), 1.0);
  });
  def('horses', { ch: 1, about: 'Cavalry moves off at a canter: hooves on turf and the jingle of harness.' }, (r) => inPlace(fade(riders(r, 3.2, 6), 0.05, 0.7), 'field', -18));
  def('horses_loop', { loop: true, ch: 1, lufs: -23, about: 'Cavalry on the move, going round for ever. Carried by a cavalry army while it marches.' }, (r) => loopOf(riders(r, 8.6, 7).slice(0, sec(8.6)), 1.2));

  // ---- Work ----------------------------------------------------------------------------------------------------
  def('dig_in', { ch: 1, lufs: -22, about: 'An army digs in: shovels going into earth, a pick, spoil thrown on the parapet. For the order to fortify or to hold.' }, (r) => {
    const out = mono(3.4);
    for (let k = 0; k < 6; k++) {
      const t = 0.05 + k * 0.5 + r.range(-0.06, 0.06);
      add(out, mul(swish(r, 0.22, { lo: 900, hi: 3200, rough: 0.8, grain: 300 }), decay(sec(0.22), 0.09, 0.004)), t, 0.9); // the blade in
      add(out, mul(sine(sec(0.1), glide(sec(0.1), 140, 80, 0.02)), decay(sec(0.1), 0.03, 0.001)), t, 0.6);
      add(out, debris(r, 0.6, { count: 9, from: 0.02, to: 0.4, lo: 700, hi: 2600, size: 0.008, level: 0.5 }), t + 0.26, 0.5); // the spoil landing
    }
    for (const t of [0.32, 1.9]) add(out, clang(r, { f0: 1500, dur: 0.3, tau: 0.05, partials: 5, bright: 0.6, click: 0.8 }), t, 0.25); // a pick on a stone
    return inPlace(out, 'field', -20);
  });
  def('hammer', { ch: 1, lufs: -22, about: 'Something is being built: three blows of a hammer on iron in a yard. Played when works are ordered from the arsenal or begun in a town.' }, (r) => {
    const out = mono(1.9);
    [0.02, 0.4, 0.78].forEach((t, k) => add(out, clang(r, { f0: 780 * r.range(0.98, 1.02), dur: 0.9, tau: 0.16, partials: 8, bright: 0.72, click: 1 }), t, 1 - k * 0.12));
    return inPlace(out, 'city', -16);
  });
  def('build_works', { ch: 1, lufs: -22, about: 'A building going up: a saw, hammering on timber, a winch\'s ratchet. For a town building ordered or finished.' }, (r) => {
    const out = mono(3.6);
    for (let k = 0; k < 4; k++) add(out, swish(r, 0.3, { lo: 1400, hi: 3800, rough: 0.9, grain: 420, move: k % 2 ? 0 : 1 }), 0.05 + k * 0.33, 0.6); // the saw, there and back
    for (const t of [1.5, 1.78, 2.06, 2.34]) add(out, knock(r, { f: [240, 410, 730], tau: 0.03, weight: 1 }), t, 0.9);
    for (let k = 0; k < 9; k++) add(out, tick(r, { tone: 2400, body: 1300, tau: 0.004 }), 2.7 + k * 0.075, 0.5);
    return inPlace(out, 'city', -16);
  });
  def('factory_loop', { loop: true, ch: 1, lufs: -30, about: 'A factory town at work: the thud of presses, belts running, a hiss of steam. Very quiet; heard only when the eye is close to a town whose works are busy.' }, (r) => {
    const dur = 9.6, out = mono(dur);
    for (let t = 0.1; t < dur; t += 0.8) add(out, far(blast(r, { dur: 0.6, crack: 0.2, body: 0.6, bodyFrom: 1200, bodyTo: 150, bodyTime: 0.05, thump: 1, thumpFrom: 90, thumpTo: 50, thumpTime: 0.09, rumble: 0 }), 200), t, 0.8);
    add(out, lp(buzz(sec(dur), 48.5, 'saw'), 420), 0, 0.2);
    add(out, mul(bp(pink(sec(dur), r), 900, 0.8), wander(sec(dur), 0.6, r, 0.4)), 0, 1.2);
    for (let t = 2.1; t < dur; t += 4.8) add(out, hiss(r, 1.2, { tone: 4200, tau: 0.35, attack: 0.05 }), t, 0.25);
    return loopOf(toMono(inPlace(out, 'city', -14)).slice(0, sec(dur)), 1.6);
  });
  def('parachutes', { ch: 1, lufs: -22, about: 'Airborne troops go out over their target: a transport overhead and the canopies cracking open one after another.' }, (r) => {
    const out = mono(5);
    const drone = engine(r, 5, { cyl: 9, rpm: 1850, rough: 0.3, pipes: [[140, 1.6, 1], [330, 1.1, 0.7], [820, 0.9, 0.35]], prop: 0.7, tone: 1400 });
    add(out, mul(drone, line(sec(5), [[0, 0], [1.2, 1], [3, 0.7], [5, 0]])), 0, 0.7);
    for (let k = 0; k < 9; k++) {
      const t = 0.9 + k * 0.33 + r.range(-0.05, 0.05);
      add(out, mul(swish(r, 0.35, { lo: 500, hi: 2600, rough: 0.5, grain: 90 }), decay(sec(0.35), 0.1, 0.03)), t, 0.6);
      add(out, mul(sine(sec(0.12), glide(sec(0.12), 190, 90, 0.02)), decay(sec(0.12), 0.035, 0.002)), t + 0.05, 0.5);
    }
    return inPlace(out, 'sky', -14);
  });
}
