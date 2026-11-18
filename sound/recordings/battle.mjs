// The battle pack: the sounds of the real-time battles, cut from the same real recordings as recipes.mjs and mixed
// for the animated units. Nothing here is synthesised. Where the battle code already names a sound (battle_loop,
// machine_gun, far_guns_01, ship_guns, dive_bomber, bomb_stick_01, sea_battle_loop) the real one takes that name,
// so that nothing a builder holds has to change; the rest are new names for the animation's moments.
//
// Every cut was chosen with tools/bursts.mjs (where each shot or burst starts and ends, and whether a voice is near)
// and tools/draw.mjs. Shouted orders on the ranges fall before most bursts, so cuts start at the burst itself.

import { fade, rng, seedOf, around, add, db, loudness, isStereo } from '../kit/dsp.mjs';
import { clean, weight, punch, glue, grit, away, tuned, tone, layers, even, loopFrom, ring, bend, shorten, tailOff } from '../kit/film.mjs';

const eased = (x, fadeIn, fadeOut) => fade(x, fadeIn, fadeOut);

export default function battle(def) {
  // ---- The rifleman -------------------------------------------------------------------------------------
  // Single shots on a firing range, each with nothing else for the best part of a second after it.
  const SINGLES = [[80.225, 80.77], [105.535, 106.1], [104.035, 104.9], [81.11, 81.65]];
  const single = (take, [a, b]) => eased(take('rifle_mg_range_singapore', { from: a, to: b }), 0.001, 0.3);
  const rifleman = (x) => grit(weight(punch(clean(x, { low: 60 }), 0.3), { hz: 110, lift: 2.5, harmonics: -14 }), { amount: 2, under: -18, low: 300, high: 5000 });
  SINGLES.forEach((span, i) => def(`rifle_shot_0${i + 1}`, {
    // A single shot's blast lasts only a few hundredths of a second, so it cannot reach the guns' loudness without
    // clipping: all four are set to one lower level instead (they sat 3 dB apart), and serve as the farther rifles.
    family: 'guns', lufs: -21, title: `A rifleman fires (${i + 1} of 4)`, finish: { fadeOut: 0.2, squash: 6 }, battle: 'A soldier fires, standing, kneeling or lying down',
    about: 'One real rifle shot on a firing range, with its crack and the slap back off the range. Four takes, so that a line of riflemen never fires the same shot twice running.',
  }, (take) => rifleman(single(take, span))));

  // ---- The machine gunner -------------------------------------------------------------------------------
  const gunner = (x) => grit(weight(punch(clean(x, { low: 50 }), 0.25), { hz: 100, lift: 3, harmonics: -14 }), { amount: 2, under: -18, low: 250, high: 5000 });
  const mg = (take, a, b) => take('m240g_prime_cuts', { from: a, to: b });
  def('lmg_burst_01', {
    family: 'guns', title: 'A machine gunner fires a short burst', finish: { fadeOut: 0.3 }, battle: 'The machine gunner fires from his bipod',
    about: 'A real belt-fed machine gun on a range: one burst of three quarters of a second and the echo after it.',
  }, (take) => gunner(eased(mg(take, 43.17, 45.0), 0.001, 0.5)));
  def('lmg_burst_02', {
    family: 'guns', title: 'A machine gunner fires a short burst (second take)', finish: { fadeOut: 0.3 }, battle: 'The machine gunner fires from his bipod',
    about: 'Another real burst from the same range, a shade shorter, so that the gunner does not fire the same burst twice running.',
  }, (take) => gunner(eased(mg(take, 81.06, 82.3), 0.001, 0.4)));
  def('lmg_burst_03', {
    family: 'guns', title: 'A machine gunner fires a short burst (third take)', finish: { fadeOut: 0.3 }, battle: 'The machine gunner fires from his bipod',
    about: 'A longer real burst let go sooner: its first rounds, then its own last round and echo, joined between two rounds.',
  }, (take) => gunner(eased(shorten(mg(take, 166.41, 168.02), 0.62, 1.22), 0.001, 0.4)));
  def('machine_gun', {
    family: 'guns', title: 'A machine gun opens up from the flank', finish: { fadeOut: 0.4 }, battle: 'A unit is caught from the flank',
    about: 'A long real burst of two and a half seconds and its echo down the range: the sound of being caught from the side.',
  }, (take) => gunner(eased(mg(take, 39.345, 43.1), 0.001, 0.8)));

  // ---- Explosions near the men --------------------------------------------------------------------------
  def('grenade_blast', {
    family: 'guns', title: 'A grenade goes off', finish: { fadeOut: 0.3 }, battle: 'A grenade lands',
    about: 'A real explosion, cut short and close: the bang and the first of its roll, before the next blast in the recording.',
  }, (take) => weight(punch(clean(eased(take('explosions_ls100152', { from: 0.6, to: 1.52 }), 0.001, 0.45), { low: 40 }), 0.3), { hz: 90, lift: 3, harmonics: -12 }));
  def('shell_impact_near', {
    family: 'guns', title: 'A shell lands close by', finish: { fadeOut: 0.8 }, battle: 'A shell lands near a unit',
    about: 'A real explosion heard from close in, with its full roll as the ground and the air carry it away.',
  }, (take) => weight(punch(clean(eased(take('explosions_ls100152', { from: 3.82, to: 7.1 }), 0.001, 1.6), { low: 30 }), 0.3), { hz: 70, lift: 4, harmonics: -12 }));
  def('shell_impact_far', {
    family: 'guns', lufs: -21, title: 'A shell lands a long way off', finish: { fadeOut: 0.8 }, battle: 'A shell lands elsewhere on the field',
    about: 'The same kind of real explosion set back a kilometre: the crack is gone and the crump rolls.',
  }, (take) => weight(away(clean(eased(take('explosions_ls100152', { from: 3.82, to: 7.1 }), 0.001, 1.6), { low: 30 }), 1000), { hz: 70, lift: 3, harmonics: -14 }));
  def('shells_landing', {
    family: 'guns', lufs: -19, title: 'A salvo of shells lands', finish: { fadeOut: 0.8 }, battle: 'A barrage falls on a position',
    about: 'Five real explosions one after another, a little way off, as a battery\'s salvo lands across a position.',
  }, (take) => weight(away(clean(eased(take('explosions_ls100152', { from: 0.55, to: 7.1 }), 0.002, 1.8), { low: 30 }), 250), { hz: 75, lift: 3, harmonics: -13 }));
  def('bomb_stick_01', {
    family: 'guns', lufs: -19, title: 'A stick of bombs walks across the target', finish: { fadeOut: 0.8 }, battle: 'Bombs fall on ships or on the ground',
    about: 'Six real explosions in a row and the roll under them, heard from a few hundred metres, as a stick of bombs falls.',
  }, (take) => weight(away(clean(eased(take('explosions_ls100155', { from: 0.2, to: 9.6 }), 0.002, 2), { low: 30 }), 300), { hz: 75, lift: 3, harmonics: -13 }));

  // ---- The tank -----------------------------------------------------------------------------------------
  def('tank_idle_loop', {
    family: 'ground', loop: true, title: 'A tank stands with its engine running', battle: 'A tank stands still',
    about: 'The diesel of a real wartime T-34, kept running by a museum in Estonia, ticking over: its slow swells evened out so that it can go round.',
  }, (take) => {
    const run = even(clean(take('t34_estonia_museum', { from: 0.5, to: 14.5 }), { low: 30 }), { window: 1.5, strength: 0.85 });
    // Every filter runs round the loop (around), so that the end flows into the start with no click.
    return around(loopFrom(run, { from: 1, seconds: 8, overlap: 2.5 }), (y) => weight(y, { hz: 80, lift: 3, harmonics: -9 }));
  });
  def('tank_gun_01', {
    family: 'guns', title: 'A tank fires its main gun', finish: { fadeOut: 0.6 }, battle: 'A tank fires its main gun',
    about: 'A real tank gun fired on a gunnery range and heard from beside the tank: the blast and the long rush of air after it.',
  }, (take) => weight(punch(clean(eased(take('abrams_gunnery_2_37', { from: 10.17, to: 12.9 }), 0.001, 1.5), { low: 25 }), 0.3), { hz: 60, lift: 4, harmonics: -12 }));
  def('tank_mg_burst', {
    family: 'guns', title: 'A tank\'s machine gun fires', finish: { fadeOut: 0.3 }, battle: 'A tank fires its machine gun',
    about: 'A real burst from the kind of machine gun mounted beside a tank\'s gun, a second long, and its echo.',
  }, (take) => gunner(eased(shorten(mg(take, 146.2, 148.9), 0.95, 1.6), 0.001, 0.6)));
  def('tank_knocked_out', {
    family: 'guns', lufs: -17.5, title: 'A tank is knocked out', finish: { fadeOut: 0.6 }, battle: 'A tank is knocked out',
    about: 'Three real explosions laid together: the hit, a smaller one as the hatch blows, and the ammunition going up in a roll behind them.',
  }, (take) => {
    const hit = punch(eased(take('explosions_ls100152', { from: 2.72, to: 3.33 }), 0.001, 0.25), 0.4);
    const hatch = tuned(eased(take('explosions_ls100155', { from: 2.925, to: 4.08 }), 0.001, 0.4), 1.25);
    const roll = eased(take('explosions_ls100152', { from: 3.82, to: 7.1 }), 0.02, 1.6);
    return weight(clean(layers([{ x: hit }, { x: hatch, at: 0.4, db: -5 }, { x: roll, at: 0.85, db: -5 }]), { low: 30 }), { hz: 70, lift: 4, harmonics: -12 });
  });

  // ---- The field: beds that play under a fight ----------------------------------------------------------
  // Each is a ring: real shots scattered over a stretch of time, whatever runs off the end coming round to the
  // start, so the bed loops with no join. Distances are set with away(); the scatter is fixed by the sound's name.
  const BATTERY = [44.095, 59.07, 75.58, 110.765, 120.26, 187.625, 221.57];
  const gunShot = (take, t, len = 1.3) => eased(take('barrage_usrok', { from: t - 0.01, to: t + len }), 0.001, 0.6);
  // For the beds: each cut dies away on its own, so the hiss under it never stops dead.
  const farShot = (take, t) => tailOff(take('barrage_usrok', { from: t - 0.01, to: t + 3.5 }), 0.25, 0.7);
  const BURSTS = [[43.17, 0.78], [81.06, 0.72], [146.2, 1.75], [137.635, 1.2]], LONG = [[39.345, 2.6], [97.835, 5.1], [278.545, 2.8]];
  const burstIn = (take, [a, len]) => gunner(tailOff(mg(take, a, a + len + 1.5), len, 0.3));
  const shotIn = (take, s) => rifleman(tailOff(single(take, s), 0.06, 0.15));
  // Guns in action: single far salvos, one dropped in every 6 to 20 seconds by the game, rather than a loop whose
  // level swung 21 dB from one second to the next.
  [[59.07, 1400, 0], [110.765, 1800, -2], [187.625, 1100, -1]].forEach(([t, metres, dbs], i) => def(`far_guns_0${i + 1}`, {
    family: 'guns', lufs: -22 + dbs, title: `A battery fires a long way off (salvo ${i + 1} of 3)`, finish: { fadeOut: 0.8 }, battle: 'Guns in action: one salvo every 6 to 20 seconds',
    about: 'A real heavy gun heard from more than a kilometre, its report rolling away. Three, so the game can drop one in at the uneven pace of a battery.',
  }, (take) => weight(clean(away(farShot(take, t), metres), { low: 25 }), { hz: 65, lift: 3, harmonics: -14 })));
  def('battle_loop', {
    family: 'guns', lufs: -22, loop: true, ch: 2, title: 'A firefight', battle: 'Any unit in a fight',
    about: 'Real rifle shots and machine-gun bursts scattered across thirty seconds, near and far and left and right, with now and then a heavy gun far behind, over a real distant firefight that never quite stops: a company in action.',
  }, (take) => {
    const r = rng(seedOf('battle_loop')), L = 30, list = [];
    const shots = SINGLES.map((s) => shotIn(take, s)), bursts = BURSTS.map((b) => burstIn(take, b));
    for (let i = 0; i < 56; i++) list.push({ x: away(r.pick(shots), r.range(60, 450)), at: r.range(0, L), db: -r.range(0, 7), pan: r.range(-0.8, 0.8) });
    for (let i = 0; i < 11; i++) list.push({ x: away(r.pick(bursts), r.range(150, 600)), at: i * L / 11 + r.range(0, 1.5), db: -r.range(2, 8), pan: r.range(-0.7, 0.7) });
    // Behind it, the rest of the fight: rifles and guns further off, so that the field is never quite still.
    for (let i = 0; i < 34; i++) list.push({ x: away(r.pick(shots), r.range(700, 1200)), at: r.range(0, L), db: -r.range(6, 12), pan: r.range(-0.8, 0.8) });
    for (let i = 0; i < 6; i++) list.push({ x: away(farShot(take, BATTERY[(i + 2) % BATTERY.length]), 1800), at: i * L / 6 + r.range(0, 3), db: -6, pan: r.range(-0.4, 0.4) });
    const fight = ring(list, L, { stereo: true });
    // Under it all, a real far-off firefight (a re-enactment heard from a wood), so the field never falls silent
    // between shots: 14 dB under the shots.
    const under = loopFrom(take('fs_170478', { from: 32.5, to: 32.5 + L + 2.5 }), { from: 0, seconds: L, overlap: 2.5 });
    const g = db(loudness(fight).integrated - loudness(under).integrated - 14);
    add(fight.L, isStereo(under) ? under.L : under, 0, g); add(fight.R, isStereo(under) ? under.R : under, 0, g);
    return around(fight, (y) => weight(clean(y, { low: 30 }), { hz: 90, lift: 2, harmonics: -15 }));
  });
  def('battle_far_loop', {
    family: 'guns', lufs: -27, loop: true, ch: 2, title: 'A battle somewhere ahead', battle: 'A fight further off, before contact or behind the line',
    about: 'The same real rifles, machine guns and heavy guns set back to a kilometre and more, across thirty seconds: a fight heard over the next rise.',
  }, (take) => {
    const r = rng(seedOf('battle_far_loop')), L = 30, list = [];
    const shots = SINGLES.map((s) => tailOff(single(take, s), 0.06, 0.15));
    for (let i = 0; i < 44; i++) list.push({ x: away(r.pick(shots), r.range(700, 1300)), at: r.range(0, L), db: -r.range(0, 8), pan: r.range(-0.6, 0.6) });
    for (let i = 0; i < 8; i++) list.push({ x: away(burstIn(take, r.pick(LONG)), r.range(900, 1500)), at: i * L / 8 + r.range(0, 2), db: -r.range(0, 5), pan: r.range(-0.6, 0.6) });
    for (let i = 0; i < 11; i++) list.push({ x: away(farShot(take, BATTERY[i % BATTERY.length]), r.range(2000, 3500)), at: i * L / 11 + r.range(0, 1.5), db: -r.range(0, 4), pan: r.range(-0.5, 0.5) });
    // Under it, the real far-off firefight in a wood set back further still, so the bed never drops into a hole.
    const fight = ring(list, L, { stereo: true });
    const under = away(loopFrom(take('fs_170478', { from: 2, to: 2 + L + 2.5 }), { from: 0, seconds: L, overlap: 2.5 }), 600);
    const g = db(loudness(fight).integrated - loudness(under).integrated - 10);
    add(fight.L, isStereo(under) ? under.L : under, 0, g); add(fight.R, isStereo(under) ? under.R : under, 0, g);
    return around(fight, (y) => weight(clean(y, { low: 25 }), { hz: 70, lift: 3, harmonics: -15 }));
  });

  // ---- The air ------------------------------------------------------------------------------------------
  def('fighter_pass', {
    family: 'air', title: 'A fighter flashes past', finish: { fadeIn: 0.01, fadeOut: 0.4 }, battle: 'A fighter crosses the field',
    about: 'A real pass of a P-38 Lightning and a B-25 at an air display, played a quarter faster: a smaller, quicker plane comes on and is gone.',
  }, (take) => weight(eased(tuned(clean(take('b25_p38_flyingbulls', { from: 51.6, to: 62.4 }), { low: 50 }), 1.25), 1.2, 2.5), { hz: 90, lift: 2, harmonics: -15 }));
  def('plane_guns', {
    family: 'guns', lufs: -18, title: 'A fighter fires its guns', finish: { fadeOut: 0.3 }, battle: 'A fighter strafes, or fires on a bomber',
    about: 'Three real bursts from a heavy machine gun of the kind wartime fighters carried six and eight of, laid a few hundredths of a second apart as the guns in the wings fire together.',
  }, (take) => {
    const m2 = (a, b) => eased(take('m2_browning_range', { from: a - 0.005, to: b + 0.5 }), 0.001, 0.4);
    return weight(away(punch(clean(layers([{ x: m2(62.845, 63.995) }, { x: m2(69.34, 70.485), at: 0.021, db: -2 }, { x: m2(103.72, 104.875), at: 0.047, db: -3 }]), { low: 50 }), 0.25), 120), { hz: 100, lift: 3, harmonics: -14 });
  });
  def('gunner_burst', {
    family: 'guns', lufs: -18, title: 'A bomber\'s gunner fires', finish: { fadeOut: 0.3 }, battle: 'A bomber\'s turret gunner fires at a fighter',
    about: 'One real burst from a heavy machine gun of the kind in a bomber\'s turrets, heard from a little way off.',
  }, (take) => weight(away(punch(clean(eased(take('m2_browning_range', { from: 75.505, to: 77.4 }), 0.001, 0.4), { low: 50 }), 0.25), 150), { hz: 100, lift: 3, harmonics: -14 }));
  def('plane_down', {
    family: 'air', title: 'A plane goes down', finish: { fadeIn: 0.02, fadeOut: 1 }, battle: 'A plane spins or burns down',
    about: 'A real pass of a P-38 and a B-25, its engines winding up higher and higher as the plane falls, and a real explosion far off as it hits the ground.',
  }, (take) => {
    // The engines wind up as it dives and fall away as it goes down far off; the crash comes as they are lost.
    const fall = eased(bend(clean(take('b25_p38_flyingbulls', { from: 55.6, to: 62.4 }), { low: 50 }), (t) => 0.95 + 0.55 * t * t), 0.6, 2.2);
    const n = fall.length / 48000;
    const crash = away(eased(take('explosions_ls100152', { from: 3.82, to: 7.1 }), 0.001, 1.6), 1500);
    return weight(layers([{ x: fall }, { x: crash, at: n - 0.9, db: 3 }]), { hz: 80, lift: 3, harmonics: -14 });
  });
  def('dive_bomber', {
    family: 'air', title: 'A dive bomber screams down', finish: { fadeIn: 0.02, fadeOut: 0.6 }, battle: 'A dive attack on ships or on land',
    about: 'A real P-38 and B-25 coming on at an air display, the engines wound up as the plane noses over and dives.',
  }, (take) => weight(eased(bend(clean(take('b25_p38_hahnweide', { from: 63.6, to: 72.4 }), { low: 50 }), (t) => 1.0 + 0.6 * t), 0.8, 0.8), { hz: 90, lift: 2, harmonics: -15 }));
  def('plane_explode', {
    family: 'flak', title: 'A plane blows up in the air', finish: { fadeOut: 0.8 }, battle: 'A plane blows up in the air',
    about: 'A real explosion, lowered a little and set back to a few hundred metres up: a plane\'s fuel going up overhead.',
  }, (take) => weight(away(tuned(clean(eased(take('explosions_ls100155', { from: 2.88, to: 4.0 }), 0.001, 0.5), { low: 30 }), 0.85), 500), { hz: 80, lift: 3, harmonics: -13 }));
  def('flak_light_fire', {
    family: 'flak', title: 'A light anti-aircraft gun fires', finish: { fadeOut: 0.6 }, battle: 'Light flak fires at low level',
    about: 'A real 35 mm anti-aircraft cannon firing a short burst and the roll after it. It stands in for the wartime 20 and 40 mm guns.',
  }, (take) => weight(punch(clean(eased(take('oerlikon_35mm_broll', { from: 26.44, to: 30.0 }), 0.001, 1.5), { low: 40 }), 0.2), { hz: 90, lift: 3, harmonics: -14 }));

  // ---- The sea ------------------------------------------------------------------------------------------
  def('ship_guns', {
    family: 'guns', title: 'A warship\'s guns fire', finish: { fadeOut: 0.6 }, battle: 'Ships open fire',
    about: 'A real 130 mm gun and a real heavy field gun fired a twentieth of a second apart, as a twin turret fires.',
  }, (take) => {
    const one = eased(take('cannon_130mm_finnish', { from: 2.86, to: 5.5 }), 0.001, 1.2);
    const two = tuned(eased(take('barrage_usrok', { from: 94.825, to: 96.75 }), 0.001, 1.0), 0.9);   // the next gun of that battery fires at 96.8
    return weight(away(clean(layers([{ x: one }, { x: two, at: 0.05, db: -2 }]), { low: 28 }), 150), { hz: 65, lift: 4, harmonics: -12 });
  });
  def('ship_gun_medium', {
    family: 'guns', title: 'A cruiser\'s guns fire', finish: { fadeOut: 0.7 }, battle: 'A cruiser fires',
    about: 'A real heavy field gun lowered a little, with a real 130 mm gun laid a few hundredths of a second behind it: a cruiser\'s twin turret.',
  }, (take) => {
    const a = tuned(eased(take('barrage_usrok', { from: 120.25, to: 122.3 }), 0.001, 1.2), 0.85);
    const b = eased(take('cannon_130mm_finnish', { from: 2.86, to: 5.5 }), 0.001, 1.2);
    return weight(away(clean(layers([{ x: a }, { x: b, at: 0.035, db: -3 }]), { low: 25 }), 180), { hz: 58, lift: 4, harmonics: -12 });
  });
  def('magazine_explosion', {
    family: 'guns', title: 'A ship\'s magazine blows up', finish: { fadeOut: 1.2 }, battle: 'A ship\'s magazine explodes',
    about: 'The biggest real explosion in the recordings, lowered to make it heavier, with a string of real blasts rolling on under it as the ship\'s shells go up.',
  }, (take) => {
    const big = tuned(eased(take('explosions_ls100152', { from: 3.33, to: 7.1 }), 0.001, 1.8), 0.7);
    const after = tuned(eased(take('explosions_ls100155', { from: 1.2, to: 9.6 }), 0.05, 2.5), 0.85);
    return weight(away(clean(layers([{ x: big }, { x: after, at: 0.6, db: -7 }]), { low: 22 }), 400), { hz: 55, lift: 5, harmonics: -12 });
  });
  def('ship_gun_heavy', {
    family: 'guns', title: 'A battleship fires a broadside', finish: { fadeOut: 0.8 }, battle: 'A battleship fires',
    about: 'Three real heavy guns laid a few hundredths of a second apart and lowered, as the three guns of a big turret fire together.',
  }, (take) => {
    const g = (t) => tuned(eased(take('barrage_usrok', { from: t - 0.01, to: t + 2.2 }), 0.001, 1.4), 0.72);
    return weight(away(clean(layers([{ x: g(110.765) }, { x: g(120.26), at: 0.04, db: -1 }, { x: g(187.625), at: 0.09, db: -2 }]), { low: 22 }), 200), { hz: 50, lift: 5, harmonics: -12 });
  });
  def('sea_battle_loop', {
    family: 'guns', lufs: -26, loop: true, ch: 2, title: 'A fleet action over the horizon', battle: 'A fleet in a fight',
    about: 'Real heavy guns and light anti-aircraft fire set back by kilometres over open water, falling at the uneven pace of ships in action, with the sea under them; thirty seconds.',
  }, (take) => {
    const r = rng(seedOf('sea_battle_loop')), L = 30, list = [];
    for (let i = 0; i < 13; i++) list.push({ x: away(tuned(farShot(take, BATTERY[i % BATTERY.length]), r.range(0.72, 0.9)), r.range(2500, 5000)), at: i * L / 13 + r.range(0, 1.5), db: -r.range(0, 5), pan: r.range(-0.6, 0.6) });
    for (let i = 0; i < 6; i++) list.push({ x: away(tailOff(take('oerlikon_35mm_broll', { from: [26.44, 33.1, 43.6][i % 3], to: [26.44, 33.1, 43.6][i % 3] + 4 }), 1.0, 0.6), r.range(1500, 2500)), at: i * L / 6 + r.range(0, 3), db: -r.range(3, 8), pan: r.range(-0.6, 0.6) });
    // Under it, the open sea from a ship's bow, so the water never stops between the guns.
    const sea = loopFrom(take('fs_438837', { from: 3, to: 3 + L + 2.5 }), { from: 0, seconds: L, overlap: 2.5 });
    const fight = ring(list, L, { stereo: true }), g = db(loudness(fight).integrated - loudness(sea).integrated - 12);
    add(fight.L, isStereo(sea) ? sea.L : sea, 0, g); add(fight.R, isStereo(sea) ? sea.R : sea, 0, g);
    return around(fight, (y) => weight(clean(y, { low: 22 }), { hz: 60, lift: 3, harmonics: -15 }));
  });
}
