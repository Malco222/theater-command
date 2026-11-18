// The air war: bombers and fighters, bombs, the siren.

import {
  SR, TAU, sec, db, clamp, lerp, smooth, rng, mono, stereo, add, place, gain, mul, line, decay, fade, wander,
  white, pink, brown, sine, buzz, svf, lp, hp, bp, lp4, eq, comb, warp, drive, inPlace, far, loopOf, toMono, widen,
} from '../kit/dsp.mjs';
import { blast, debris, whistle, engine, glide, clang } from '../kit/parts.mjs';

/** A radial engine, as most bombers and many fighters had. */
const radial = (r, dur, rpm, o = {}) => engine(r, dur, {
  cyl: 9, rpm, rough: 0.34, sharp: 3.0, pipes: [[rpm / 60 * 4.5, 1.6, 1.0], [330, 1.1, 0.75], [820, 0.9, 0.4], [1900, 0.8, 0.14]],
  prop: 0.75, blades: 3, gearing: 0.56, tone: 3000, breath: 0.4, ...o,
});
/** A liquid-cooled twelve in a line: smoother, higher, with the whine of its blower. */
const inline = (r, dur, rpm, o = {}) => engine(r, dur, {
  cyl: 12, rpm, rough: 0.2, sharp: 3.6, pipes: [[rpm / 60 * 6, 1.8, 1.0], [520, 1.0, 0.7], [1250, 0.9, 0.45], [2600, 0.8, 0.2]],
  prop: 0.85, blades: 3, gearing: 0.48, whine: 1, whineAt: 3900, tone: 4200, breath: 0.3, ...o,
});

/** Several engines that never quite agree: the throb of a bomber is the beating between them. */
function manyEngines(r, dur, { kind = radial, count = 4, rpm = 1950, apart = 0.012, tone = 1700 } = {}) {
  const out = mono(dur);
  for (let k = 0; k < count; k++) {
    const e = kind(r.fork(), dur, rpm * (1 + (k - (count - 1) / 2) * apart + r.bip() * apart * 0.3), { tone });
    add(out, mul(e, wander(e.length, 0.25, r, 0.25)), 0, 1 / Math.sqrt(count));
  }
  return out;
}

/**
 * Something flying past the listener: the pitch falls as it goes by, it is loudest when nearest, the air takes the
 * top off it with distance, and its echo off the ground sweeps through it (the "whoosh" of every real fly-past).
 */
function flyPast(src, { dur, speed = 110, near = 60, height = 80, tNear = null, ear = 1.7, bounce = 0.5, stereoOut = true } = {}) {
  const n = sec(dur), when = tNear ?? dur * 0.5;
  const d1 = new Float32Array(n), d2 = new Float32Array(n), a = new Float32Array(n), top = new Float32Array(n), pan = new Float32Array(n);
  const closest = Math.hypot(near, height - ear);
  let far0 = 0;
  for (let i = 0; i < n; i++) {
    const x = speed * (i / SR - when), flat = Math.hypot(x, near);
    const r1 = Math.hypot(flat, height - ear), r2 = Math.hypot(flat, height + ear);
    d1[i] = r1 / 343; d2[i] = r2 / 343; far0 = Math.max(far0, d2[i]);
    a[i] = closest / r1; top[i] = 700 + 11000 * Math.pow(closest / r1, 1.3); pan[i] = 0.9 * x / flat;
  }
  // The sound heard now left the aircraft a little while ago: read the engine through that changing delay.
  const offset = far0 + 0.01;
  const read = (d) => { const o = new Float32Array(n); for (let i = 0; i < n; i++) { const p = i + (offset - d[i]) * SR, j = Math.floor(p), u = p - j; if (j >= 0 && j < src.length - 1) o[i] = src[j] + (src[j + 1] - src[j]) * u; } return o; };
  const direct = read(d1), echo = lp(read(d2), 3200);
  const mixd = new Float32Array(n);
  for (let i = 0; i < n; i++) mixd[i] = (direct[i] + echo[i] * bounce) * a[i];
  const shaped = svf(mixd, 'lp', top, 0.6);
  if (!stereoOut) return shaped;
  const out = stereo(dur);
  for (let i = 0; i < n; i++) { const g = (pan[i] + 1) * Math.PI / 4; out.L[i] = shaped[i] * Math.cos(g); out.R[i] = shaped[i] * Math.sin(g); }
  return out;
}

/** A burst from an aircraft's guns, heard from the ground: thin, fast, torn. */
function airGuns(r, seconds, rate = 17, heavy = false) {
  const out = mono(seconds + 0.4);
  for (let t = 0; t < seconds; t += (1 / rate) * r.range(0.93, 1.07)) {
    const shot = blast(r, { dur: 0.25, crack: 0.7, crackTone: 1800, body: 1, bodyFrom: heavy ? 2400 : 3600, bodyTo: 300, bodyTime: heavy ? 0.035 : 0.018, thump: heavy ? 0.7 : 0.25, thumpFrom: 170, thumpTo: 80, thumpTime: 0.03, rumble: 0 });
    add(out, shot, t, r.range(0.7, 1));
  }
  return out;
}

export default function (def) {
  // ---- Engines that go round for ever, to be carried by the aircraft on the map ---------------------------
  def('bomber_heavy_loop', { loop: true, ch: 1, about: 'Four-engined bombers in formation, heard from below: the deep, slowly beating drone. Carried by a raid of heavy bombers for as long as it is in the air.' }, (r) => {
    const planes = mono(13);
    for (let p = 0; p < 3; p++) add(planes, manyEngines(r.fork(), 13, { count: 4, rpm: 1900 + p * 37, apart: 0.011, tone: 1500 }), 0, [1, 0.75, 0.6][p]);
    return loopOf(eq(eq(planes, 'low', 160, 4), 'high', 2500, -6), 1.6);
  });
  def('bomber_medium_loop', { loop: true, ch: 1, about: 'Twin-engined bombers. Their two engines run a little out of step, which gives the slow "wom-wom-wom" that people on the ground remembered.' }, (r) => {
    const planes = mono(13);
    for (let p = 0; p < 3; p++) add(planes, manyEngines(r.fork(), 13, { count: 2, rpm: 2050 + p * 52, apart: 0.021, tone: 1700 }), 0, [1, 0.7, 0.55][p]);
    return loopOf(eq(eq(planes, 'low', 180, 3), 'high', 2800, -5), 1.6);
  });
  def('fighter_loop', { loop: true, ch: 1, about: 'A single-engined fighter with an in-line engine at cruising speed: a smooth tearing note with a blower\'s whine on top.' }, (r) => loopOf(inline(r, 9.5, 2650), 1.2));
  def('fighter_radial_loop', { loop: true, ch: 1, about: 'A single-engined fighter with a radial engine: rounder and rougher than the in-line one.' }, (r) => loopOf(engine(r, 9.5, { cyl: 14, rpm: 2400, rough: 0.28, sharp: 3.2, pipes: [[280, 1.7, 1.0], [460, 1.1, 0.7], [1000, 0.9, 0.42], [2200, 0.8, 0.16]], prop: 0.8, tone: 3600 }), 1.2));

  // ---- Fly-pasts, played once ------------------------------------------------------------------------------
  def('fighter_pass_01', { ch: 2, same: 'fighter_pass', about: 'A fighter goes by low and fast, left to right; the note drops as it passes.' }, (r) => {
    const src = inline(r, 7.5, 2950);
    return inPlace(fade(flyPast(src, { dur: 4.4, speed: 135, near: 45, height: 60, tNear: 2.0 }), 0.5, 1.0), 'field', -20);
  });
  def('fighter_pass_02', { ch: 2, about: 'A radial-engined fighter goes by, right to left, a little further off.' }, (r) => {
    const src = engine(r, 7.5, { cyl: 14, rpm: 2550, rough: 0.28, pipes: [[290, 1.7, 1.0], [470, 1.1, 0.7], [1000, 0.9, 0.42], [2200, 0.8, 0.16]], prop: 0.85, tone: 3800 });
    const p = fade(flyPast(src, { dur: 4.6, speed: 120, near: 80, height: 90, tNear: 2.2 }), 0.5, 1.0);
    return inPlace({ L: p.R, R: p.L }, 'field', -20);
  });
  def('fighter_pass', { ch: 2, same: 'fighter_pass_01', about: 'The same as fighter_pass_01, under the name the 3D game already calls.' }, (r) => {
    const src = inline(rng(1 + r.int(9)), 7.5, 2950);
    return inPlace(fade(flyPast(src, { dur: 4.4, speed: 135, near: 45, height: 60, tNear: 2.0 }), 0.5, 1.0), 'field', -20);
  });
  def('bomber_drone', { ch: 2, about: 'A formation of heavy bombers passes high overhead and goes on: six seconds, swelling and fading. Played once where no loop can be carried.' }, (r) => {
    const src = mono(11);
    for (let p = 0; p < 3; p++) add(src, manyEngines(r.fork(), 11, { count: 4, rpm: 1920 + p * 41, apart: 0.011, tone: 1600 }), 0, [1, 0.75, 0.6][p]);
    const p = flyPast(src, { dur: 6.5, speed: 85, near: 150, height: 900, tNear: 3.0, bounce: 0.35 });
    return inPlace(fade(p, 1.6, 2.2), 'field', -22);
  });
  def('dive_bomber', { ch: 1, about: 'A dive bomber comes down on its target: the engine winds up and a wind-driven siren on its undercarriage screams, then it pulls out. The bomb is a separate sound (bomb_single).' }, (r) => {
    const dur = 5.0, n = sec(dur + 2);
    const rev = line(n, [[0, 0.8], [3.3, 1.28], [3.9, 1.22], [dur + 2, 0.95]], 1);
    const eng = inline(r, dur + 2, 2500, { rev, whine: 2 });
    const pitch = line(n, [[0, 380], [3.3, 1080], [3.8, 1000], [dur + 2, 500]], 1);
    const air = white(n, r), howl = mono(dur + 2);
    for (const m of [1, 1.19]) add(howl, svf(buzz(n, pitch.map((f) => f * m), 'pulse', 0.3), 'bp', pitch.map((f) => f * m * 1.5), 2.5), 0, 0.5);
    add(howl, svf(air, 'bp', pitch.map((f) => f * 2.2), 5), 0, 0.5);
    const env = line(n, [[0, 0], [0.9, 0.5], [3.3, 1], [3.9, 0.6], [dur, 0]], 1);
    for (let i = 0; i < n; i++) eng[i] = eng[i] * 0.9 + howl[i] * 0.55 * env[i] * env[i];
    return inPlace(fade(flyPast(eng, { dur, speed: 125, near: 70, height: 260, tNear: 3.5, stereoOut: false }), 0.6, 0.9), 'field', -18);
  });
  def('aircraft_takeoff', { ch: 1, about: 'Aircraft run up their engines on the airfield and go: played at the airfield as a raid or a patrol sets off.' }, (r) => {
    const dur = 5.4, n = sec(dur + 2);
    const rev = line(n, [[0, 0.42], [1.5, 0.5], [2.6, 1.0], [dur + 2, 1.04]], 1);
    const a = radial(r.fork(), dur + 2, 2300, { rev }), b = radial(r.fork(), dur + 2, 2260, { rev: rev.map((v) => v * 0.99) });
    add(a, b, 0.35, 0.8);
    const roll = flyPast(a, { dur, speed: 46, near: 70, height: 3, tNear: 2.4, stereoOut: false, bounce: 0.2 });
    return inPlace(fade(roll, 0.3, 1.6), 'field', -20);
  });
  def('dogfight', { ch: 2, about: 'Fighters among the bombers: two engines weaving, short bursts of their guns. Played over a raid while the defender\'s fighters are engaged.' }, (r) => {
    const dur = 7.5, n = sec(dur + 3), out = stereo(dur);
    for (let k = 0; k < 2; k++) {
      const q = r.fork(), w = wander(n, 0.45, q, 1);
      const rev = w.map((v) => 0.85 + 0.35 * v);
      const e = k ? engine(q, dur + 3, { cyl: 14, rpm: 2450, rev, pipes: [[285, 1.7, 1.0], [460, 1.1, 0.7], [1000, 0.9, 0.42]], prop: 0.8, tone: 3400 }) : inline(q, dur + 3, 2800, { rev });
      const p = flyPast(e, { dur, speed: k ? -70 : 85, near: 110, height: 320, tNear: k ? 4.6 : 2.6, bounce: 0.3 });
      place(out, p, 0, 0, 0.9);
    }
    for (const [t, len, pn] of [[1.2, 0.5, -0.4], [2.1, 0.8, -0.1], [3.9, 0.45, 0.35], [4.7, 1.0, 0.5], [6.0, 0.4, -0.2]]) {
      const g = far(airGuns(r, len, r.chance(0.3) ? 9 : 17, r.chance(0.3)), 420);
      place(out, g, t, pn, 0.55);
    }
    return inPlace(fade(out, 0.8, 1.4), 'sky', -16);
  });
  def('plane_down', { ch: 1, about: 'An aircraft falls out of the fight: its engine falters and its note slides down and away, and it hits far off. Played once for a raid that loses aircraft.' }, (r) => {
    const dur = 5.2, n = sec(dur + 2);
    const rev = line(n, [[0, 1.0], [0.8, 1.1], [3.8, 1.55], [dur + 2, 1.6]], 1.4);
    const e = radial(r, dur + 2, 1900, { rev, rough: 0.6 });
    const cough = wander(n, 9, r, 1);
    for (let i = 0; i < n; i++) e[i] *= cough[i] > 0.38 ? 1 : 0.25;
    const scream = whistle(r, dur, { from: 900, to: 2300, q: 9, breath: 0.2, curve: 1.2 });
    const fall = flyPast(e, { dur, speed: 150, near: 500, height: 700, tNear: 5.4, stereoOut: false, bounce: 0.2 });
    add(fall, mul(scream, line(scream.length, [[0, 0], [1.5, 0.25], [3.9, 0.5], [4.2, 0]])), 0, 0.3);
    mul(fall, line(fall.length, [[0, 0], [0.5, 1], [4.05, 1], [4.2, 0], [dur, 0]]));
    const out = mono(dur + 3);
    add(out, fall, 0, 1);
    add(out, far(blast(r, { dur: 3, crack: 0, body: 0.8, bodyFrom: 1500, bodyTo: 120, bodyTime: 0.22, thump: 1, thumpFrom: 80, thumpTo: 34, thumpTime: 0.45, rumble: 0.9, rumbleTime: 1.3 }), 900), 4.35, 1.3);
    return inPlace(out, 'field', -16);
  });

  // ---- Bombs -----------------------------------------------------------------------------------------------
  const fallOfBombs = (r, count, dur) => {
    const out = mono(dur + 0.2);
    for (let k = 0; k < count; k++) {
      const len = dur * r.range(0.72, 1), start = (dur - len) * r.f();
      const w = whistle(r, len, { from: r.range(2500, 3300), to: r.range(650, 950), q: r.range(10, 18), breath: 0.3, curve: r.range(1.3, 1.9) });
      add(out, mul(w, line(w.length, [[0, 0], [len * 0.35, 0.35], [len * 0.93, 1], [len, 0]], 1)), start, r.range(0.5, 1));
    }
    return out;
  };
  def('bombs_falling_01', { ch: 1, about: 'A stick of bombs on its way down: several whistles together, sliding down in pitch and swelling. Ends just as the first bomb would land; follow it with a bomb_stick.' }, (r) => inPlace(far(fallOfBombs(r, 6, 2.7), 120), 'sky', -22));
  def('bombs_falling_02', { ch: 1, about: 'The same, a shorter and thinner fall: a lighter raid.' }, (r) => inPlace(far(fallOfBombs(r, 3, 2.1), 160), 'sky', -22));
  const bomb = (r, size = 1) => {
    const b = blast(r, {
      dur: 4.5, crack: 0.5, crackTone: 1800, body: 1, bodyFrom: 3800 * r.range(0.8, 1.2), bodyTo: 110, bodyTime: 0.24 * size, grit: 3,
      thump: 1.3, thumpFrom: 105 * r.range(0.85, 1.1), thumpTo: 30, thumpTime: 0.42 * size, rumble: 0.9, rumbleTone: 200, rumbleTime: 1.5 * size, rumbleRoll: 4,
    });
    add(b, debris(r, 3.5, { count: 34, from: 0.25, to: 2.6, lo: 500, hi: 3600, level: 0.22 }), 0, 1);
    return b;
  };
  const stick = (r, count, gap) => {
    const out = mono(count * gap + 5.5);
    let t = 0.02;
    for (let k = 0; k < count; k++) {
      add(out, far(bomb(r.fork(), r.range(0.8, 1.15)), r.range(60, 420)), t, r.range(0.6, 1));
      t += gap * r.range(0.65, 1.4);
    }
    return out;
  };
  for (const [name, count, gap, about] of [
    ['bomb_stick_01', 6, 0.27, 'A stick of six bombs walks across the target, each a heavy thump with earth and masonry coming down after it.'],
    ['bomb_stick_02', 8, 0.2, 'A heavier stick of eight, closer together: a hard hit.'],
    ['bomb_stick_03', 4, 0.36, 'Four bombs, well spaced: a light raid or a scattered one.'],
  ]) def(name, { fam: 'guns', ch: 1, lufs: -15, about }, (r) => inPlace(drive(stick(r, count, gap), 1.3), 'city', -15));
  def('bomb_single_01', { fam: 'guns', ch: 1, lufs: -15, about: 'One heavy bomb: for a dive bomber\'s strike, a bridge, a single hit.' }, (r) => inPlace(bomb(r, 1.1), 'field', -14));
  def('bomb_single_02', { fam: 'guns', ch: 1, lufs: -15, about: 'One bomb, a second voice so that two in a row do not sound the same.' }, (r) => inPlace(bomb(r, 0.9), 'city', -15));

  // ---- The warning -----------------------------------------------------------------------------------------
  const sirenVoice = (r, dur, pitch) => {
    const n = sec(dur), out = mono(dur);
    // Two rotors chopping the air at pitches a minor third apart: the uneasy chord of every town siren.
    for (const m of [1, 1.2]) {
      const p = pitch.map((f) => f * m);
      add(out, svf(buzz(n, p, 'pulse', 0.32, r.f()), 'bp', p.map((f) => f * 2.1), 1.3), 0, 0.6);
    }
    return eq(eq(out, 'peak', 950, 5, 1.0), 'high', 3800, -8);
  };
  def('air_raid_siren', { fam: 'calls', ch: 1, lufs: -21, same: 'siren', about: 'The air raid warning over a town: the wail winds up, wavers twice and runs down. Played from the place as a raid comes within sight of it.' }, (r) => {
    const dur = 9.5, n = sec(dur);
    const pitch = line(n, [[0, 120], [2.2, 470], [3.6, 330], [5.3, 470], [6.6, 345], [dur, 105]], 1).map((f, i) => f * (1 + 0.004 * Math.sin(TAU * 0.7 * i / SR)));
    const v = mul(sirenVoice(r, dur, pitch), line(n, [[0, 0], [0.9, 0.7], [2.2, 1], [6.6, 0.9], [dur, 0]], 1));
    return inPlace(far(v, 380), 'city', -9);
  });
  def('siren', { fam: 'calls', ch: 1, lufs: -21, same: 'air_raid_siren', about: 'The same warning, shorter, under the name the 3D game already calls.' }, (r) => {
    const dur = 6.0, n = sec(dur);
    const pitch = line(n, [[0, 130], [1.9, 470], [3.1, 340], [4.2, 470], [dur, 110]], 1);
    const v = mul(sirenVoice(r, dur, pitch), line(n, [[0, 0], [0.8, 0.7], [1.9, 1], [4.2, 0.9], [dur, 0]], 1));
    return inPlace(far(v, 380), 'city', -9);
  });
  def('siren_all_clear', { fam: 'calls', ch: 1, lufs: -22, about: 'The all clear: one steady note held and let run down. Played from the place when the last raid of the night has gone.' }, (r) => {
    const dur = 6.5, n = sec(dur);
    const pitch = line(n, [[0, 130], [1.8, 455], [4.6, 455], [dur, 110]], 1);
    const v = mul(sirenVoice(r, dur, pitch), line(n, [[0, 0], [0.8, 0.7], [1.8, 1], [4.6, 0.95], [dur, 0]], 1));
    return inPlace(far(v, 380), 'city', -9);
  });
}
