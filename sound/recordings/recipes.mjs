// The recipes that turn real recordings into the game's sounds. Each names the recordings it takes from (by their
// id in sources.json) and says how they are cut and mixed. Nothing here is synthesised.
//
// The cuts were chosen by reading each recording with tools/timeline.mjs and tools/draw.mjs: stretches with voices,
// a public-address system, a clipped microphone or a second sound over the wanted one were passed over.

import { fade, around } from '../kit/dsp.mjs';
import { clean, weight, punch, glue, grit, away, tuned, tone, layers, even, sustain, beats, steady, loopFrom } from '../kit/film.mjs';

/** A cut with its ends eased, so that it neither clicks in nor stops dead. */
const eased = (x, fadeIn, fadeOut) => fade(x, fadeIn, fadeOut);

export default function recipes(def) {
  // ---- The air --------------------------------------------------------------------------------------------
  // Two clean passes of a B-25 Mitchell and a P-38 Lightning at an air display (no commentary over these seconds).
  // The B-17 recordings fetched were made with a microphone driven past its limit, and are not used.
  const pass = (take) => ({
    a: take('b25_p38_flyingbulls', { from: 55.6, to: 62.4 }),  // going away, steady note
    b: take('b25_p38_hahnweide', { from: 63.6, to: 72.4 }),    // coming on, steady note
    c: take('b25_p38_flyingbulls', { from: 49.7, to: 53.2 }),  // coming on, higher note
  });

  def('bomber_heavy_loop', {
    family: 'air', loop: true, title: 'Heavy bombers overhead',
    about: 'The drone of a formation, built from two real passes of a B-25 Mitchell and a P-38 Lightning: six layers of the same engines, tuned a hair apart so that they beat slowly against each other as a formation does.',
  }, (take) => {
    const p = pass(take), L = 11.4, ov = 3;
    // Each pass has one strong note (its propellers): 138, 121 to 127 and 151 Hz. Every layer is played at the speed
    // that brings its note to about 125 Hz, a hair apart from the others, so the layers beat slowly against each
    // other instead of blurring into a roar. The top is rolled off, as height does to a formation.
    const voice = (x, ratio, fold) => sustain(even(tuned(clean(x, { low: 40, high: 2200 }), ratio), { window: 1.2, strength: 0.9 }), L + ov + 3, fold);
    const mix = layers([
      { x: voice(p.a, 0.905, 1.6), db: 0 },
      { x: voice(p.b, 1.0, 2.2), db: -1, at: 0.7 },
      { x: voice(p.c, 0.826, 1.2), db: -2, at: 1.1 },
      { x: voice(p.a, 0.918, 1.9), db: -2, at: 1.9 },
      { x: voice(p.b, 1.015, 1.4), db: -3, at: 0.2 },
      { x: voice(p.c, 0.836, 1.0), db: -4, at: 2.4 },
    ]);
    return loopFrom(weight(glue(tone(mix, 'high', 700, -5), { threshold: -20, ratio: 1.6 }), { hz: 75, lift: 3, harmonics: -15 }), { from: 2.6, seconds: L, overlap: ov });
  });

  def('bomber_drone', {
    family: 'air', ch: 2, title: 'Bombers pass overhead', finish: { fadeIn: 0.01, fadeOut: 0.3, squash: 3 },
    about: 'One real pass: a B-25 Mitchell and a P-38 Lightning come over and go on, with a second pass laid a second behind it on the other side.',
  }, (take) => {
    const one = eased(clean(take('b25_p38_flyingbulls', { from: 49.6, to: 62.6 }), { low: 40 }), 2.2, 4);
    const two = eased(tuned(clean(take('b25_p38_hahnweide', { from: 62.6, to: 72.6 }), { low: 40 }), 0.9), 2.5, 4);
    return weight(layers([{ x: one, pan: -0.35 }, { x: two, at: 1.0, db: -3, pan: 0.45 }], { stereo: true }), { hz: 80, lift: 3, harmonics: -15 });
  });

  // ---- Guns -----------------------------------------------------------------------------------------------
  def('artillery_fire_01', {
    family: 'guns', title: 'A field gun fires', finish: { fadeOut: 0.5 },
    about: 'A real field gun at a training range: one shot for the bark and the thump, and the long roll of another shot from the same battery laid under it as it dies away.',
  }, (take) => {
    const shot = eased(take('croatian_artillery', { from: 32.56, to: 34.22 }), 0.001, 0.4);
    const roll = eased(take('croatian_artillery', { from: 22.25, to: 25.15 }), 0.25, 0.9);
    const mix = layers([{ x: punch(shot, 0.35) }, { x: roll, at: 0.45, db: -2 }]);
    return grit(weight(clean(mix, { low: 28 }), { hz: 70, lift: 4, harmonics: -12 }), { amount: 2.5, under: -17, low: 150, high: 3500 });
  });

  def('rifles', {
    family: 'guns', title: 'Rifle fire along a line', finish: { fadeOut: 0.3 },
    about: 'Real rifles on a firing range: eleven shots from several riflemen in three seconds, as they fell.',
  }, (take) => {
    const line = eased(take('rifle_mg_range_singapore', { from: 101.62, to: 104.9 }), 0.002, 0.45);
    return grit(weight(punch(clean(line, { low: 60 }), 0.3), { hz: 110, lift: 2.5, harmonics: -14 }), { amount: 2, under: -18, low: 300, high: 5000 });
  });

  // ---- Flak -----------------------------------------------------------------------------------------------
  def('flak_heavy_fire_01', {
    family: 'flak', title: 'A heavy anti-aircraft gun fires', finish: { fadeOut: 0.6 },
    about: 'A real 130 mm coast gun firing, heard from a little way off. It stands in for the heavy flak gun, which is the same kind of long, hard-hitting gun; no free recording of an 88 was found.',
  }, (take) => {
    const shot = eased(take('cannon_130mm_finnish', { from: 14.57, to: 18.2 }), 0.001, 0.9);
    return weight(away(clean(shot, { low: 30 }), 160), { hz: 80, lift: 4, harmonics: -12 });
  });

  def('flak_burst_01', {
    family: 'flak', title: 'A shell bursts high among the bombers', finish: { fadeOut: 0.8 },
    about: 'A real explosion in the open air with its echo, set back to the distance of a burst high overhead: the crump and the roll, with the crack taken off by the air.',
  }, (take) => {
    const burst = eased(take('explosions_ls100155', { from: 4.94, to: 9.4 }), 0.002, 1.6);
    return weight(away(clean(burst, { low: 30 }), 450), { hz: 85, lift: 3, harmonics: -13 });
  });

  // ---- Rail -----------------------------------------------------------------------------------------------
  def('train_loop', {
    family: 'rail', loop: true, title: 'A steam train running',
    about: 'The exhaust beat of a real American Berkshire, engine 765, built in 1944, as it comes by under load. The engine was gathering speed; each beat is set a shade earlier or later so that it runs at one speed and can go round.',
  }, (take) => {
    const run = take('steam_berkshire_765', { from: 44.2, to: 59 });
    const at = beats(run, { period: 0.45 });
    const period = 0.43, n = 24;                      // six turns of the wheels, four beats a turn
    const even4 = steady(run, at.slice(0, n + 4 + 1), period);
    // The filters run round the finished loop (around), so its end flows into its start with no click.
    return around(loopFrom(even(even4, { window: 2.5, strength: 0.8 }), { from: 0, seconds: n * period, overlap: 4 * period - 0.01 }), (y) => weight(clean(y, { low: 38 }), { hz: 90, lift: 2, harmonics: -16 }));
  });

  // ---- The ground -----------------------------------------------------------------------------------------
  def('tank_loop', {
    family: 'ground', loop: true, title: 'Tanks on the move',
    about: 'Two real T-34s. Underneath, the diesel of a wartime T-34 kept running by a museum in Estonia, coming on slowly with its tracks clanking; over it, the track clatter of a Finnish T-34-85 advancing at a display.',
  }, (take) => {
    const L = 8, ov = 2.5;
    const engine = even(clean(take('t34_estonia_museum', { from: 31.5, to: 42.5 }), { low: 32 }), { window: 2, strength: 0.85 });
    const tracks = sustain(even(clean(take('t34_85_finnish_flagday', { from: 1.2, to: 9.6 }), { low: 140 }), { window: 1.5, strength: 0.85 }), L + ov + 0.4, 1.2);
    const mix = layers([{ x: engine }, { x: tracks, db: -5 }]);
    return loopFrom(grit(weight(glue(mix, { threshold: -18, ratio: 1.8 }), { hz: 85, lift: 3, harmonics: -13 }), { amount: 2, under: -18, low: 150, high: 3000 }), { from: 0, seconds: L, overlap: ov });
  });
}
