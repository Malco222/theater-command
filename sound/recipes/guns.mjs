// Guns of every size, the flak and the searchlights, and the noise of a battle.

import {
  SR, TAU, sec, db, clamp, lerp, smooth, rng, mono, stereo, add, place, gain, mul, line, decay, fade, wander,
  white, pink, brown, sine, buzz, svf, lp, hp, bp, lp4, eq, comb, drive, inPlace, far, loopOf, toMono,
} from '../kit/dsp.mjs';
import { blast, debris, whistle, clang, engine, glide, hiss, knock } from '../kit/parts.mjs';

// ---- The guns themselves ---------------------------------------------------------------------------------

export const rifle = (r) => blast(r, { dur: 0.9, crack: 1.3, crackTone: 2600, crackTime: 0.003, body: 1, bodyFrom: 5200 * r.range(0.85, 1.15), bodyTo: 480, bodyTime: 0.028, grit: 3, thump: 0.5, thumpFrom: 210, thumpTo: 105, thumpTime: 0.04, rumble: 0 });
export const mgRound = (r, heavy) => blast(r, { dur: 0.4, crack: 1.1, crackTone: 2300, crackTime: 0.0025, body: 1, bodyFrom: heavy ? 3600 : 4800, bodyTo: 420, bodyTime: heavy ? 0.03 : 0.018, grit: 3, thump: heavy ? 0.7 : 0.4, thumpFrom: 190, thumpTo: 100, thumpTime: 0.035, rumble: 0 });
export const fieldGun = (r, size = 1) => {
  const g = blast(r, { dur: 4, crack: 1.0, crackTone: 2000, body: 1, bodyFrom: 4500 * r.range(0.85, 1.15), bodyTo: 170, bodyTime: 0.12 * size, grit: 3, thump: 1.1, thumpFrom: 112 * r.range(0.9, 1.1), thumpTo: 36, thumpTime: 0.32 * size, rumble: 0.7, rumbleTone: 220, rumbleTime: 1.3 * size });
  // The shell on its way: a short tearing of the air just after the report.
  const away = svf(white(sec(0.5), r), 'bp', glide(sec(0.5), 500, 1900, 0.25), 2.2);
  add(g, mul(away, line(away.length, [[0, 0], [0.04, 1], [0.5, 0]], 2)), 0.03, 0.12);
  return g;
};
export const heavyFlak = (r) => {
  const g = blast(r, { dur: 2.4, crack: 1.25, crackTone: 2300, crackTime: 0.0035, body: 1, bodyFrom: 6200 * r.range(0.9, 1.1), bodyTo: 240, bodyTime: 0.065, grit: 3.2, thump: 1.0, thumpFrom: 152 * r.range(0.92, 1.08), thumpTo: 54, thumpTime: 0.14, rumble: 0.35, rumbleTone: 300, rumbleTime: 0.6 });
  add(g, clang(r, { f0: r.range(620, 780), dur: 0.5, tau: 0.07, partials: 6, bright: 0.65, click: 0.3 }), 0.11, 0.05); // the breech thrown open
  return g;
};
export const airBurst = (r, v = 1) => blast(r, { dur: 2.2, crack: 0.25, crackTone: 1500, body: 1, bodyFrom: 2600 * v, bodyTo: 140, bodyTime: 0.09, grit: 2.2, thump: 0.75, thumpFrom: 108 * v, thumpTo: 48, thumpTime: 0.17, rumble: 0.45, rumbleTone: 260, rumbleTime: 0.7 });
export const lightFlak = (r) => blast(r, { dur: 0.7, crack: 1.0, crackTone: 2200, body: 1, bodyFrom: 4200, bodyTo: 300, bodyTime: 0.04, grit: 3, thump: 0.9, thumpFrom: 185 * r.range(0.95, 1.05), thumpTo: 88, thumpTime: 0.06, rumble: 0.1, rumbleTime: 0.2 });
export const cannon20 = (r) => blast(r, { dur: 0.3, crack: 1.1, crackTone: 2500, crackTime: 0.002, body: 1, bodyFrom: 4600, bodyTo: 520, bodyTime: 0.017, grit: 3, thump: 0.6, thumpFrom: 230 * r.range(0.96, 1.04), thumpTo: 120, thumpTime: 0.022, rumble: 0 });
export const crump = (r, size = 1) => {
  const c = blast(r, { dur: 4, crack: 0.2, crackTone: 1400, body: 1, bodyFrom: 2300 * r.range(0.8, 1.2), bodyTo: 110, bodyTime: 0.2 * size, grit: 2.6, thump: 1.2, thumpFrom: 92 * r.range(0.85, 1.1), thumpTo: 31, thumpTime: 0.4 * size, rumble: 0.9, rumbleTone: 190, rumbleTime: 1.4 * size });
  add(c, debris(r, 3, { count: 22, from: 0.25, to: 2.2, lo: 500, hi: 3000, level: 0.18 }), 0, 1);
  return c;
};
export const bigBang = (r, size = 1) => {
  const b = blast(r, { dur: 5, crack: 0.8, crackTone: 1900, body: 1, bodyFrom: 5200 * r.range(0.85, 1.15), bodyTo: 120, bodyTime: 0.22 * size, grit: 3.2, thump: 1.3, thumpFrom: 110 * r.range(0.9, 1.1), thumpTo: 30, thumpTime: 0.42 * size, rumble: 0.95, rumbleTone: 210, rumbleTime: 1.6 * size });
  add(b, debris(r, 3.6, { count: 40, from: 0.2, to: 2.8, lo: 500, hi: 4200, level: 0.25 }), 0, 1);
  return b;
};
/** A burst of automatic fire: rounds a second, for so long, a little uneven as every gun is. */
export function burst(r, seconds, rate, round) {
  const out = mono(seconds + 1);
  for (let t = 0; t < seconds; t += (1 / rate) * r.range(0.94, 1.06)) add(out, round(r), t, r.range(0.78, 1));
  return out;
}

export default function (def) {
  // ---- Flak ------------------------------------------------------------------------------------------------
  for (let v = 1; v <= 3; v++) def(`flak_heavy_fire_0${v}`, { fam: 'flak', ch: 1, about: 'A heavy anti-aircraft gun fires from the edge of the town: a hard crack and a thump, the breech clanging open after it. One of three voices; pick at random.' }, (r) => inPlace(far(heavyFlak(r), 140), 'city', -13));
  for (let v = 1; v <= 4; v++) def(`flak_burst_0${v}`, { fam: 'flak', ch: 1, lufs: -21, about: 'A heavy shell bursts high in the air among the bombers: a dull "crump" with no crack to it, and the ground answering a moment later. One of four voices, each a little higher or lower.' }, (r) => inPlace(far(airBurst(r, [1, 0.86, 1.17, 0.94][v - 1]), 650), 'sky', -11));
  for (let v = 1; v <= 2; v++) def(`flak_light_0${v}`, { fam: 'flak', ch: 1, lufs: -20, about: 'Light anti-aircraft guns: the steady "pom, pom, pom" of an automatic gun, two rounds a second, and its small shells bursting far up.' }, (r) => {
    const out = mono(5.5), every = v === 1 ? 0.5 : 0.43;
    for (let k = 0; k < (v === 1 ? 6 : 7); k++) {
      add(out, far(lightFlak(r), 120), 0.02 + k * every * r.range(0.97, 1.03), r.range(0.8, 1));
      add(out, far(blast(r, { dur: 0.6, crack: 0.3, body: 1, bodyFrom: 2400, bodyTo: 400, bodyTime: 0.03, thump: 0.3, thumpFrom: 160, thumpTo: 90, thumpTime: 0.04, rumble: 0 }), 900), 0.02 + k * every + r.range(0.85, 1.25), 0.3);
    }
    return inPlace(out, 'city', -14);
  });
  def('flak_barrage_loop', { fam: 'flak', loop: true, ch: 1, lufs: -22, about: 'A whole town\'s guns firing at once, heard as one continuous barrage: for a raid on a place with heavy guns, under the single shots.' }, (r) => {
    const dur = 13.5, out = mono(dur + 3);
    for (let t = 0; t < dur; t += r.range(0.18, 0.75)) add(out, far(heavyFlak(r.fork()), r.range(350, 900)), t, r.range(0.35, 0.9));
    for (let t = 0; t < dur; t += r.range(0.15, 0.6)) add(out, far(airBurst(r.fork(), r.range(0.8, 1.2)), r.range(700, 1300)), t, r.range(0.3, 0.75));
    const wet = toMono(inPlace(out.slice(0, sec(dur)), 'city', -12));
    return loopOf(wet.slice(0, sec(dur)), 1.5);
  });
  for (let v = 1; v <= 2; v++) def(`flak_wagon_fire_0${v}`, { fam: 'flak', ch: 1, lufs: -19, about: 'The flak wagon on a guarded train opens up: four light barrels firing together in short tearing bursts, the wagon\'s iron floor ringing under them.' }, (r) => {
    const out = mono(3.2);
    const bursts = v === 1 ? [[0.02, 0.9], [1.25, 0.6]] : [[0.02, 0.5], [0.8, 0.45], [1.55, 0.8]];
    for (const [t, len] of bursts) add(out, burst(r, len, 23, cannon20), t, 1);
    const ring = comb(out, 1 / 310, 0.82, 0.35, 1);
    add(out, lp(ring, 2600), 0, 0.12);
    return inPlace(far(out, 90), 'field', -14);
  });

  // ---- Searchlights ------------------------------------------------------------------------------------------
  const arc = (r, dur) => {
    // A carbon arc: a mains buzz with a fierce hiss on it, and the little generator engine that feeds it.
    const n = sec(dur), w = wander(n, 7, r, 1);
    const hum = lp(buzz(n, 100, 'pulse', 0.18), 2400);
    const fizz = bp(white(n, r), 5200, 0.8);
    const out = mono(dur);
    for (let i = 0; i < n; i++) out[i] = hum[i] * 0.22 + fizz[i] * (0.16 + 0.2 * w[i]) * (1 + 0.5 * hum[i]);
    add(out, engine(r, dur, { cyl: 1, rpm: 1500, rough: 0.5, pipes: [[95, 1.2, 1], [300, 1, 0.6], [900, 0.8, 0.25]], tone: 1500 }), 0, 0.35);
    return out;
  };
  def('searchlight_on', { fam: 'flak', ch: 1, lufs: -24, about: 'A searchlight is struck: the heavy switch goes over and the arc catches with a spit and settles to a buzz. Played at each lamp as its beam comes up.' }, (r) => {
    const out = mono(1.8);
    add(out, knock(r, { f: [120, 260, 610], tau: 0.05, weight: 1.2 }), 0.01, 0.9);
    add(out, clang(r, { f0: 880, dur: 0.3, tau: 0.04, partials: 5, bright: 0.6 }), 0.012, 0.25);
    const strike = mul(hp(white(sec(0.35), r), 3000), line(sec(0.35), [[0, 0], [0.01, 1], [0.08, 0.35], [0.35, 0.15]]));
    add(out, strike, 0.09, 0.8);
    add(out, mul(arc(r, 1.7), line(sec(1.7), [[0, 0], [0.1, 0], [0.3, 1], [1.2, 0.6], [1.7, 0]])), 0.05, 0.9);
    return inPlace(out, 'field', -20);
  });
  def('searchlight_loop', { fam: 'flak', loop: true, ch: 1, lufs: -32, about: 'A searchlight burning: the buzz and hiss of the arc and the putter of its generator. Very quiet; heard only close to. Carried by each lamp while it is lit.' }, (r) => loopOf(arc(r, 7.2), 1.0));
  def('searchlight_off', { fam: 'flak', ch: 1, lufs: -26, about: 'A searchlight is doused: the arc fizzles down and the switch drops.' }, (r) => {
    const out = mono(1.3);
    add(out, mul(arc(r, 1.0), line(sec(1.0), [[0, 0.8], [0.25, 0.5], [0.7, 0]], 1.5)), 0, 1);
    add(out, knock(r, { f: [130, 280, 640], tau: 0.04 }), 0.62, 0.6);
    return inPlace(out, 'field', -20);
  });
  def('searchlight_caught', { fam: 'flak', ch: 1, lufs: -25, about: 'A beam has found a bomber and holds it: a thin bright shimmer, rising. A signal more than a real sound; played once when a searchlight takes hold.' }, (r) => {
    const n = sec(1.3), out = mono(1.3);
    for (const [m, g] of [[1, 1], [1.5, 0.5], [2.01, 0.35], [3.02, 0.15]]) add(out, mul(sine(n, glide(n, 880 * m, 1320 * m, 0.25)), line(n, [[0, 0], [0.06, 1], [0.5, 0.5], [1.3, 0]], 1.5)), 0, g * 0.4);
    add(out, mul(hp(white(n, r), 6000), line(n, [[0, 0], [0.05, 0.25], [0.6, 0]])), 0, 0.3);
    return inPlace(out, 'sky', -9);
  });

  // ---- Artillery ---------------------------------------------------------------------------------------------
  for (let v = 1; v <= 3; v++) def(`artillery_fire_0${v}`, { ch: 1, about: 'A field gun in the gun line fires: the bark, the thump in the ground, the shell tearing away, and the report rolling round the country. One of three voices.' }, (r) => inPlace(far(fieldGun(r, [1, 1.12, 0.92][v - 1]), 90), 'valley', -11));
  def('field_gun', { ch: 1, same: 'artillery_fire_01', about: 'A field gun, under the name the 3D game already calls.' }, (r) => inPlace(far(fieldGun(r, 1.05), 90), 'valley', -11));
  for (let v = 1; v <= 2; v++) def(`artillery_salvo_0${v}`, { ch: 1, lufs: -15, about: 'The whole gun line fires, gun after gun down the line in a second or two. For a bombardment ordered, or a battle opened by an army with artillery.' }, (r) => {
    const out = mono(8), guns = v === 1 ? 5 : 7;
    let t = 0.02;
    for (let k = 0; k < guns; k++) { add(out, far(fieldGun(r.fork(), r.range(0.9, 1.15)), r.range(70, 260)), t, r.range(0.7, 1)); t += r.range(0.16, 0.42); }
    return inPlace(drive(out, 1.3), 'valley', -10);
  });
  for (let v = 1; v <= 2; v++) def(`shell_incoming_0${v}`, { ch: 1, about: 'A shell coming in: its short falling whine and the burst, with earth thrown up.' }, (r) => {
    const out = mono(6), len = v === 1 ? 1.15 : 0.8;
    const w = whistle(r, len, { from: v === 1 ? 1900 : 2400, to: 760, q: 8, breath: 0.5, curve: 1.5 });
    add(out, mul(w, line(w.length, [[0, 0], [len * 0.5, 0.4], [len * 0.97, 1], [len, 0]])), 0, 0.2);
    add(out, crump(r, 1), len - 0.02, 1);
    return inPlace(out, 'field', -13);
  });
  for (let v = 1; v <= 3; v++) def(`shell_impact_0${v}`, { ch: 1, lufs: -18, about: 'A shell bursts some way off: a heavy "crump" and what it threw up coming down. One of three voices, for the far end of a bombardment.' }, (r) => inPlace(far(crump(r, [1, 0.85, 1.15][v - 1]), [380, 520, 300][v - 1]), 'valley', -12));
  def('barrage_loop', { loop: true, ch: 1, lufs: -21, about: 'Drumfire: a bombardment kept up, guns and bursts without a pause. Carried by a province while it is being shelled.' }, (r) => {
    const dur = 13.5, out = mono(dur + 4);
    for (let t = 0; t < dur; t += r.range(0.12, 0.55)) add(out, far(fieldGun(r.fork(), r.range(0.85, 1.2)), r.range(300, 900)), t, r.range(0.3, 0.9));
    for (let t = 0; t < dur; t += r.range(0.2, 0.7)) add(out, far(crump(r.fork(), r.range(0.8, 1.2)), r.range(500, 1200)), t, r.range(0.3, 0.8));
    const bed = mul(lp4(brown(sec(dur), r), 110), wander(sec(dur), 2.5, r, 0.6));
    add(out, bed, 0, 2.5);
    const wet = toMono(inPlace(out.slice(0, sec(dur)), 'valley', -12));
    return loopOf(wet.slice(0, sec(dur)), 1.5);
  });

  // ---- Small arms and the tank's gun ---------------------------------------------------------------------------
  const volley = (r, shots, over) => { const out = mono(over + 1.5); for (let k = 0; k < shots; k++) add(out, far(rifle(r), r.range(60, 300)), r.range(0.02, over), r.range(0.5, 1)); return out; };
  def('rifles', { ch: 1, same: 'rifles_01', about: 'Rifle fire: a ragged dozen shots along a line.' }, (r) => inPlace(volley(r, 12, 1.5), 'field', -12));
  def('rifles_02', { ch: 1, about: 'Rifle fire, thinner: skirmishers, a patrol, a garrison holding on.' }, (r) => inPlace(volley(r, 6, 1.8), 'field', -12));
  def('machine_gun', { ch: 1, same: 'machine_gun_01', about: 'A heavy machine gun: two deliberate bursts, slow and hard.' }, (r) => {
    const out = mono(3);
    add(out, burst(r, 1.15, 9.2, (q) => mgRound(q, true)), 0.02, 1); add(out, burst(r, 0.65, 9.2, (q) => mgRound(q, true)), 1.5, 1);
    return inPlace(far(out, 110), 'field', -13);
  });
  def('machine_gun_02', { ch: 1, about: 'A light machine gun: quick tearing bursts.' }, (r) => {
    const out = mono(3);
    add(out, burst(r, 0.55, 19, (q) => mgRound(q, false)), 0.02, 1); add(out, burst(r, 0.4, 19, (q) => mgRound(q, false)), 0.85, 1); add(out, burst(r, 0.7, 19, (q) => mgRound(q, false)), 1.5, 1);
    return inPlace(far(out, 110), 'field', -13);
  });
  const tankShot = (r) => {
    const g = blast(r, { dur: 3.5, crack: 1.4, crackTone: 2400, crackTime: 0.003, body: 1, bodyFrom: 6500, bodyTo: 220, bodyTime: 0.085, grit: 3.4, thump: 1.1, thumpFrom: 135, thumpTo: 44, thumpTime: 0.2, rumble: 0.5, rumbleTime: 0.9 });
    add(g, clang(r, { f0: r.range(1500, 1900), dur: 0.5, tau: 0.09, partials: 6, bright: 0.7, click: 0.6 }), 0.85, 0.035); // the empty case on the turret floor
    return g;
  };
  def('tank_gun', { ch: 1, same: 'tank_gun_01', about: 'A tank fires: a flat, hard crack with a boom under it, and the empty case ringing on the turret floor.' }, (r) => inPlace(far(tankShot(r), 70), 'field', -12));
  def('tank_gun_02', { ch: 1, about: 'A tank fires, a second voice.' }, (r) => inPlace(far(tankShot(r), 110), 'valley', -13));
  for (const [name, same, about, make] of [
    ['explosion', 'explosion_01', 'A big explosion with earth and wreckage coming down: an assault going in, a bridge blown, a dump hit.', (r) => inPlace(bigBang(r, 1), 'field', -12)],
    ['explosion_02', null, 'An explosion in a street: the walls throw it back.', (r) => inPlace(bigBang(r, 0.9), 'city', -12)],
    ['explosion_03', null, 'A store of fuel or shells goes up: one burst, then more inside it, and a long roar.', (r) => {
      const out = mono(7);
      add(out, bigBang(r.fork(), 1.15), 0, 1);
      for (let k = 0; k < 5; k++) add(out, crump(r.fork(), r.range(0.5, 0.8)), r.range(0.35, 1.9), r.range(0.3, 0.6));
      const roar = mul(bp(pink(sec(3.5), r), 420, 0.7), line(sec(3.5), [[0, 0], [0.3, 1], [3.5, 0]], 1.6));
      add(out, roar, 0.1, 1.4);
      return inPlace(out, 'valley', -12);
    }],
  ]) def(name, { ch: 1, lufs: -15, same, about }, make);
  def('bridge_blown', { ch: 1, lufs: -15, about: 'A bridge is blown: the charges go, the span groans and drops into the river.' }, (r) => {
    const out = mono(7);
    add(out, bigBang(r.fork(), 0.8), 0, 1); add(out, bigBang(r.fork(), 0.7), 0.16, 0.8);
    const groan = svf(buzz(sec(1.6), glide(sec(1.6), 170, 62, 0.7), 'saw'), 'bp', glide(sec(1.6), 900, 300, 0.6), 4);
    add(out, mul(groan, line(sec(1.6), [[0, 0], [0.3, 1], [1.6, 0]])), 0.5, 0.25);
    const splash = mul(svf(white(sec(2.4), r), 'bp', glide(sec(2.4), 3200, 700, 0.8), 0.7), line(sec(2.4), [[0, 0], [0.12, 1], [2.4, 0]], 1.8));
    add(out, splash, 1.7, 0.9);
    add(out, blast(r, { dur: 2, crack: 0, body: 0.5, bodyFrom: 900, bodyTo: 90, bodyTime: 0.25, thump: 1, thumpFrom: 70, thumpTo: 30, thumpTime: 0.4, rumble: 0.5 }), 1.68, 0.8);
    return inPlace(out, 'valley', -13);
  });

  // ---- A battle, heard from the table ----------------------------------------------------------------------
  const fight = (r, dur, thick) => {
    const out = stereo(dur + 5);
    for (let t = 0.05; t < dur; t += r.range(0.05, 0.32) / thick) place(out, far(rifle(r), r.range(150, 600)), t, r.range(-0.9, 0.9), r.range(0.25, 0.7));
    for (let t = r.range(0.2, 0.8); t < dur; t += r.range(0.9, 2.2) / thick) place(out, far(burst(r, r.range(0.4, 1.1), r.chance(0.5) ? 9.2 : 19, (q) => mgRound(q, true)), r.range(200, 500)), t, r.range(-0.8, 0.8), 0.5);
    for (let t = r.range(0.2, 0.6); t < dur; t += r.range(0.9, 1.9) / thick) place(out, far(crump(r.fork(), r.range(0.8, 1.15)), r.range(250, 700)), t, r.range(-0.7, 0.7), r.range(0.6, 0.95));
    for (let t = r.range(0.8, 1.6); t < dur; t += r.range(2.4, 4) / thick) place(out, far(tankShot(r.fork()), r.range(200, 450)), t, r.range(-0.6, 0.6), 0.7);
    return out;
  };
  def('battle', { ch: 2, lufs: -17, about: 'A fight heard from a little way off, five seconds of it: rifles, machine guns, shells bursting, a tank gun. Played once when a battle is resolved.' }, (r) => {
    const f = fight(r, 4.2, 1);
    return inPlace({ L: f.L.slice(0, sec(6)), R: f.R.slice(0, sec(6)) }, 'valley', -12);
  });
  def('battle_loop', { loop: true, ch: 2, lufs: -21, about: 'The same fight going on and on: the bed under the face-off card and under any battle that is being watched.' }, (r) => {
    const dur = 18, f = fight(r, dur, 0.85);
    const wet = inPlace({ L: f.L.slice(0, sec(dur)), R: f.R.slice(0, sec(dur)) }, 'valley', -12);
    return loopOf({ L: wet.L.slice(0, sec(dur)), R: wet.R.slice(0, sec(dur)) }, 2);
  });
}
