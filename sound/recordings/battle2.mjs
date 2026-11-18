// The battle pack, part two: cut from the Freesound finds (recordings/freesound, found.json, CREDITS.md). It fills
// what the first cut could not: voices, the soldier's feet and kit, the rifle bolt, fire, ships' engines and bells,
// bugles and whistles, and real wartime guns and aircraft (a Mauser 98k, a Lee-Enfield, an M1 Garand, a PPSh-41, an
// MP 38, an MG 42, a Maxim; a Spitfire, a Focke-Wulf 190, a P-47, a B-17 and a Stuka). Nothing here is synthesised,
// and nothing comes from a film, a series or a game.
//
// Every cut was chosen from measurements (tools/bursts.mjs and a level trace every 5 ms), not by ear: where each
// shot, step or line starts and ends, and whether the loud parts are clipped. Each def says which group of the
// listening board it sits in.

import { fade, around } from '../kit/dsp.mjs';
import { clean, weight, punch, tuned, tone, layers, even, loopFrom, shorten, tailOff } from '../kit/film.mjs';

const eased = (x, fadeIn, fadeOut) => fade(x, fadeIn, fadeOut);

export default function battle2(def) {
  const fs = (take, id, from, to, o = {}) => take('fs_' + id, { from, to, ...o });
  // A filter on a finished loop runs round it (around), so the loop's end flows into its start with no click.
  const cleanLoop = (x, low) => around(x, (y) => clean(y, { low }));

  // ---- Voices -------------------------------------------------------------------------------------------
  // Real people shouting, cut line by line. The words of the English reel (95520) are not known here, since they are
  // chosen without ears: the board is where they are heard.
  const voice = (x) => clean(x, { low: 90 });
  const VOICES = 'Voices';
  def('order_go', {
    family: 'calls', group: VOICES, title: '"Go, go, go!" (a British voice)', finish: { fadeOut: 0.15 }, battle: 'An officer sends his men forward',
    about: 'A man with a northern British voice shouting "Go, go, go!", recorded for free use by theuncertainman.',
  }, (take) => voice(eased(fs(take, 402643, 0.2, 1.55), 0.01, 0.2)));
  def('order_move', {
    family: 'calls', group: VOICES, title: '"Move!" (a British voice)', finish: { fadeOut: 0.15 }, battle: 'A unit is ordered to move',
    about: 'The same kind of British voice shouting one word: "Move!".',
  }, (take) => voice(eased(fs(take, 458073, 0.18, 1.1), 0.01, 0.2)));
  def('order_charge', {
    family: 'calls', group: VOICES, title: '"Charge!" (a British voice)', finish: { fadeOut: 0.2 }, battle: 'A unit charges',
    about: 'A British voice shouting "Charge!".',
  }, (take) => voice(eased(fs(take, 458068, 0.35, 1.7), 0.01, 0.25)));
  // All seventeen lines of the reel, in its order: each from just before its first word to just before the next line.
  const LINE_AT = [0.555, 2.155, 3.58, 5.445, 7.53, 9.07, 11.19, 14.405, 16.285, 17.705, 19.85, 21.75, 24.275, 27.015, 29.49, 31.07, 33.44];
  const LINE_END = [1.39, 2.77, 4.27, 6.385, 8.31, 10.425, 13.19, 15.37, 17.04, 19.04, 21.055, 23.425, 25.52, 28.45, 30.21, 32.275, 35.06];
  LINE_AT.forEach((a, i) => def(`order_english_${String(i + 1).padStart(2, '0')}`, {
    family: 'calls', group: VOICES, title: `A battlefield order in English (line ${i + 1} of 17)`, finish: { fadeOut: 0.15 }, battle: 'Orders shouted along a line (which line goes with which moment, once heard)',
    about: 'One line from a reel of battlefield shouts and orders by one voice actor, put up by shawshank73. The words are for your ear to judge.',
  }, (take) => voice(eased(fs(take, 95520, a - 0.05, Math.min(LINE_END[i] + 0.4, (LINE_AT[i + 1] ?? 36.4) - 0.08)), 0.01, 0.2))));
  def('german_schnell', {
    family: 'calls', group: VOICES, title: '"Schnell, schnell!" (German)', finish: { fadeOut: 0.15 }, battle: 'A German unit is hurried forward',
    about: 'A German speaker shouting "Schnell, schnell!" (quick, quick!).',
  }, (take) => voice(eased(fs(take, 237294, 0.22, 1.75), 0.01, 0.2)));
  def('german_los', {
    family: 'calls', group: VOICES, title: '"Los, los, los!" (German)', finish: { fadeOut: 0.15 }, battle: 'A German unit is sent off (go, go, go)',
    about: 'A German speaker shouting "Los, los, los!" (go, go, go!).',
  }, (take) => voice(eased(fs(take, 237290, 1.42, 2.7), 0.01, 0.2)));
  def('german_drop_weapon', {
    family: 'calls', group: VOICES, title: '"Drop the gun!" (German)', finish: { fadeOut: 0.1 }, battle: 'Men are taken prisoner',
    about: 'A German command to drop a weapon, one male voice: for the moment a unit surrenders.',
  }, (take) => voice(eased(fs(take, 443444, 0, 1.04), 0.005, 0.1)));
  const RUSSIAN = [[1.5, 2.6, 'probably "Ogon! Ogon!" (fire!)'], [3.05, 4.2, 'probably "Strelyat!" (open fire!)'], [4.63, 6.0, 'probably "Pobeda!" (victory!)'], [6.2, 8.4, 'probably "Davai! Davai!" (come on!)']];
  RUSSIAN.forEach(([a, b, words], i) => def(`russian_order_0${i + 1}`, {
    family: 'calls', group: VOICES, title: `A Soviet order (${i + 1} of 4): ${words}`, finish: { fadeOut: 0.15 }, battle: 'Soviet orders along a line',
    about: `One line of a Russian speaker's shouted orders; the recording lists them in this order, so this is ${words}.`,
  }, (take) => voice(eased(fs(take, 212429, a, b), 0.01, 0.2))));
  def('charge_cry', {
    family: 'calls', group: VOICES, title: 'A crowd of men scream as they charge', finish: { fadeOut: 1 }, battle: 'A unit charges home',
    about: 'A group of people screaming as they charge, eight seconds of it.',
  }, (take) => voice(eased(fs(take, 563011, 0.25, 8.6), 0.05, 1.5)));
  def('victory_cry', {
    family: 'calls', group: VOICES, title: 'Ten men raise a victory cry', finish: { fadeOut: 0.5 }, battle: 'A position is taken',
    about: 'Ten men shouting a victory cry together.',
  }, (take) => voice(eased(fs(take, 165492, 0.05, 3.9), 0.02, 0.8)));
  def('chinese_soldiers_shout', {
    family: 'calls', group: VOICES, lufs: -22, title: 'Chinese soldiers shout as they run', finish: { fadeOut: 0.8 }, battle: 'A Chinese unit attacks (the China line)',
    about: 'Chinese soldiers shouting and running, from an old effects library given away free (craigsmith).',
  }, (take) => voice(eased(fs(take, 479580, 10.5, 24.4), 0.3, 1.2)));

  // ---- Bugles and whistles ------------------------------------------------------------------------------
  const CALLS = 'Bugles and whistles';
  // bugle_charge: cut by Brian on the listening board, 18 Nov 14:49Z (file kept in battle-pack/cut-by-brian/)
  // bugle_call: cut by Brian on the listening board, 18 Nov 14:49Z (file kept in battle-pack/cut-by-brian/)
  def('whistle_long', {
    family: 'calls', group: CALLS, lufs: -22, title: 'An officer blows one long blast', finish: { fadeOut: 0.1 }, battle: 'Over the top: an attack starts',
    about: 'An old army whistle of the kind officers blew in the First World War: one long blast.',
  }, (take) => clean(eased(fs(take, 324539, 0.2, 2.1), 0.01, 0.15), { low: 300 }));
  def('whistle_short', {
    family: 'calls', group: CALLS, lufs: -22, title: 'An officer blows two short blasts', finish: { fadeOut: 0.1 }, battle: 'A unit is called back or told to halt',
    about: 'The same whistle, two short blasts.',
  }, (take) => clean(eased(fs(take, 324539, 2.7, 4.75), 0.01, 0.15), { low: 300 }));

  // ---- Wartime guns, one for each army --------------------------------------------------------------------
  // Light handling: these are already the real thing, so they get only a clean low end and a little body.
  const GUNS = 'Wartime guns, one for each army';
  const gun = (x) => weight(punch(clean(x, { low: 50 }), 0.2), { hz: 100, lift: 2, harmonics: -16 });
  def('kar98_shot', {
    family: 'guns', group: GUNS, title: 'A German rifleman fires (Mauser 98k)', finish: { fadeOut: 0.4 }, battle: 'A German soldier fires',
    about: 'A real Mauser 98k, the German army\'s rifle, firing a blank at a historical re-enactment, and its echo.',
  }, (take) => gun(eased(fs(take, 802675, 0.02, 2.0), 0.001, 0.6)));
  def('enfield_shot', {
    family: 'guns', group: GUNS, title: 'A British rifleman fires (Lee-Enfield)', finish: { fadeOut: 0.15 }, battle: 'A British soldier fires',
    about: 'A real Lee-Enfield .303, the British army\'s rifle: one shot, short and dry.',
  }, (take) => gun(eased(fs(take, 450852, 0.15, 0.79), 0.001, 0.2)));
  def('garand_shot', {
    family: 'guns', group: GUNS, title: 'An American rifleman fires (M1 Garand)', finish: { fadeOut: 0.4 }, battle: 'An American soldier fires',
    about: 'An M1 Garand shot and its tail (the uploader does not say where it was recorded).',
  }, (take) => gun(eased(fs(take, 386842, 0, 2.6), 0.001, 0.8)));
  def('garand_clip_ping', {
    family: 'guns', group: GUNS, lufs: -24, title: 'The M1 Garand\'s empty clip pings out', finish: { fadeOut: 0.2 }, battle: 'An American rifleman fires his eighth round',
    about: 'A real M1 Garand ejecting its empty clip, with the ping every American soldier knew.',
  }, (take) => clean(eased(fs(take, 725398, 0, 1.19), 0.001, 0.3), { low: 80 }));
  [[1.5, 2.75], [4.45, 5.75]].forEach(([a, b], i) => def(`ppsh_burst_0${i + 1}`, {
    family: 'guns', group: GUNS, title: `A Soviet submachine gun fires (PPSh-41, take ${i + 1})`, finish: { fadeOut: 0.3 }, battle: 'A Soviet soldier fires his submachine gun',
    about: 'A real PPSh-41 firing in the Carpathian mountains, recorded on location by Lubini for his own documentary.',
  }, (take) => gun(eased(fs(take, 338323, a, b), 0.001, 0.4))));
  [[2.05, 4.05], [4.07, 5.1]].forEach(([a, b], i) => def(`mp40_burst_0${i + 1}`, {
    family: 'guns', group: GUNS, title: `A German submachine gun fires (MP 38, take ${i + 1})`, finish: { fadeOut: 0.3 }, battle: 'A German soldier fires his submachine gun',
    about: 'A real MP 38, the MP 40\'s elder brother, recorded on location by Lubini.',
  }, (take) => gun(eased(fs(take, 338046, a, b), 0.001, 0.4))));
  [[0.15, 2.6], [2.65, 5.15]].forEach(([a, b], i) => def(`mg42_burst_0${i + 1}`, {
    family: 'guns', group: GUNS, title: `A German machine gun rips (MG 42, take ${i + 1})`, finish: { fadeOut: 0.4 }, battle: 'A German machine gunner fires',
    about: 'A real MG 42, the German machine gun that fired so fast it tore like cloth. The uploader says he added a few layers to the recording.',
  }, (take) => gun(eased(fs(take, 338242, a, b), 0.001, 0.6))));
  def('maxim_burst', {
    family: 'guns', group: GUNS, title: 'A Soviet heavy machine gun fires (Maxim)', finish: { fadeOut: 0.4 }, battle: 'A Soviet machine gunner fires',
    about: 'A real Maxim machine gun heard from about 40 metres: one long burst of two and a half seconds.',
  }, (take) => gun(eased(fs(take, 160773, 0.7, 4.35), 0.001, 0.6)));
  def('bullet_crack', {
    family: 'guns', group: GUNS, lufs: -22, title: 'A bullet cracks past', finish: { fadeOut: 0.02 }, battle: 'A unit is under fire',
    about: 'A real bullet passing close, the sharp crack it makes going faster than sound, taken from a US Army training film (public domain).',
  }, (take) => clean(fs(take, 855248, 0, 0.08), { low: 200 }));
  [[0.3, 2.0], [2.0, 4.3]].forEach(([a, b], i) => def(`bullets_incoming_0${i + 1}`, {
    family: 'guns', group: GUNS, lufs: -22, title: `Bullets whine and ricochet round a unit (take ${i + 1})`, finish: { fadeOut: 0.2 }, battle: 'A unit is under fire',
    about: 'Real incoming ricochets, recorded on a minidisc: the whine of rounds glancing off the ground nearby.',
  }, (take) => clean(eased(fs(take, 109958, a, b), 0.01, 0.2), { low: 150 })));

  // ---- The soldier: feet, kit and bolt ----------------------------------------------------------------------
  const KIT = 'The soldier: feet, kit and bolt';
  const foley = (x, low = 60) => clean(x, { low });
  [0.47, 1.58, 2.54, 3.685].forEach((t, i) => def(`footstep_0${i + 1}`, {
    family: 'ground', group: KIT, lufs: -28, title: `A boot on gravel (step ${i + 1} of 4)`, finish: { fadeOut: 0.05 }, battle: 'A soldier takes a step (walk, run)',
    about: 'One step of a man walking in mountain boots on gravel. Four, so that a walk never repeats the same step twice running.',
  }, (take) => foley(tailOff(eased(fs(take, 556002, t - 0.03, t + 0.45), 0.004, 0.1), 0.18, 0.08))));
  def('marching_loop', {
    family: 'ground', group: KIT, lufs: -26, loop: true, title: 'A column marches', battle: 'A column on the march (loop)',
    about: 'Real men marching in boots, cut on the beat: twenty steps that go round without a stumble.',
  }, (take) => cleanLoop(loopFrom(fs(take, 172369, 0, 14), { from: 2.2, seconds: 9.22, overlap: 0.04 }), 60));
  def('body_fall_01', {
    family: 'ground', group: KIT, lufs: -22, title: 'A man falls heavily on dirt', finish: { fadeOut: 0.2 }, battle: 'A soldier is hit and falls',
    about: 'A real body fall on dirt, very heavy (its first thud is a little clipped).',
  }, (take) => foley(eased(fs(take, 504626, 0.3, 1.63), 0.003, 0.3), 40));
  [[0.93, 1.9], [11.95, 13.0]].forEach(([a, b], i) => def(`body_fall_0${i + 2}`, {
    family: 'ground', group: KIT, lufs: -24, title: `A man falls to the ground (take ${i + 2})`, finish: { fadeOut: 0.2 }, battle: 'A soldier is hit and falls',
    about: 'A body falling to the ground, leaves and earth under it.',
  }, (take) => foley(eased(fs(take, 853591, a, b), 0.003, 0.3), 40)));
  def('body_fall_04', {
    family: 'ground', group: KIT, lufs: -24, title: 'A man falls in grass, close', finish: { fadeOut: 0.2 }, battle: 'A soldier is hit and falls',
    about: 'A body falling in grass, recorded close.',
  }, (take) => foley(eased(fs(take, 73583, 1.62, 2.65), 0.003, 0.3), 40));
  def('knee_down', {
    family: 'ground', group: KIT, lufs: -28, title: 'A soldier drops to one knee', finish: { fadeOut: 0.1 }, battle: 'A soldier kneels to fire',
    about: 'A man going down on one knee: cloth, kit and the knee on the ground.',
  }, (take) => foley(eased(fs(take, 393021, 0.15, 1.22), 0.01, 0.2)));
  [[0.4, 1.4], [3.6, 4.55]].forEach(([a, b], i) => def(`kit_rattle_0${i + 1}`, {
    family: 'ground', group: KIT, lufs: -28, title: `A soldier's kit rattles (take ${i + 1})`, finish: { fadeOut: 0.1 }, battle: 'A soldier gets up or starts to run',
    about: 'Real harness straps and buckles shaken, as webbing and kit move on a running man.',
  }, (take) => foley(eased(fs(take, 805470, a, b), 0.01, 0.2), 120)));
  def('bolt_k98', {
    family: 'guns', group: KIT, lufs: -24, title: 'A German rifleman works his bolt (Mauser 98k)', finish: { fadeOut: 0.1 }, battle: 'A rifleman works his bolt between shots',
    about: 'The bolt of a real Mauser 98k drawn back and run home.',
  }, (take) => foley(eased(fs(take, 802673, 0.3, 1.6), 0.005, 0.2), 100));
  def('bolt_mosin', {
    family: 'guns', group: KIT, lufs: -24, title: 'A Soviet rifleman works his bolt (Mosin-Nagant)', finish: { fadeOut: 0.1 }, battle: 'A rifleman works his bolt between shots',
    about: 'A real Mosin-Nagant bolt, one full cycle: up, back, forward, down.',
  }, (take) => foley(eased(fs(take, 370345, 0.05, 1.55), 0.005, 0.2), 100));
  def('bolt_enfield', {
    family: 'guns', group: KIT, lufs: -24, title: 'A British rifleman works his bolt (Lee-Enfield)', finish: { fadeOut: 0.1 }, battle: 'A rifleman works his bolt between shots',
    about: 'A real Lee-Enfield bolt opened and closed. The recording is slow, so the wait between the two strokes is taken out.',
  }, (take) => foley(shorten(eased(fs(take, 162153, 0.35, 3.48), 0.005, 0.05), 0.55, 2.2), 100));
  def('grenade_throw', {
    family: 'ground', group: KIT, lufs: -24, title: 'A pin is pulled and a grenade thrown', finish: { fadeOut: 0.1 }, battle: 'A soldier throws a grenade (before the blast)',
    about: 'A real grenade\'s pin pulled, then the swish of an arm and sleeve as it is thrown.',
  }, (take) => foley(layers([{ x: eased(fs(take, 876008, 0.05, 0.6), 0.003, 0.1) }, { x: eased(fs(take, 611613, 0, 1.28), 0.003, 0.15), at: 0.45 }]), 100));

  // ---- Explosions ---------------------------------------------------------------------------------------
  const EXPL = 'Explosions';
  def('explosion_debris', {
    family: 'guns', group: EXPL, title: 'A shell bursts close, with earth and stones falling', finish: { fadeOut: 0.6 }, battle: 'A shell lands right on a unit',
    about: 'A real explosion recorded close, with the rocks and debris raining down after it.',
  }, (take) => weight(clean(eased(fs(take, 108641, 0.08, 4.06), 0.001, 1.2), { low: 30 }), { hz: 70, lift: 3, harmonics: -14 }));
  [[13.5, 20.5], [5.5, 13.0]].forEach(([a, b], i) => def(`explosion_far_0${i + 1}`, {
    family: 'guns', group: EXPL, lufs: -21, title: `An explosion a long way off (take ${i + 1})`, finish: { fadeOut: 0.8 }, battle: 'Shells land elsewhere on the field',
    about: 'Real distant explosions rolling across open country (cut from the blasts that are not clipped).',
  // All of this recording sits under 940 Hz, so a laptop lost 12 dB of it: the 200 to 800 Hz band is lifted and the
  // low end's own harmonics laid in, which a small speaker can play.
  }, (take) => weight(tone(clean(eased(fs(take, 320788, a, b), 0.01, 2), { low: 25 }), 'peak', 420, 11, 0.5), { hz: 70, lift: 0, harmonics: -4, from: 140 })));
  def('mortar_fire', {
    family: 'guns', group: EXPL, title: 'A mortar fires', finish: { fadeOut: 0.3 }, battle: 'A mortar team fires',
    about: 'A real 120 mm mortar firing, taken from a US Army training film (public domain).',
  }, (take) => weight(clean(eased(fs(take, 854478, 0, 1.8), 0.001, 0.6), { low: 30 }), { hz: 70, lift: 2, harmonics: -16 }));
  def('field_gun_fire', {
    family: 'guns', group: EXPL, title: 'A field gun fires', finish: { fadeOut: 0.4 }, battle: 'A field gun fires',
    about: 'A real field artillery gun firing, and the report rolling away.',
  }, (take) => weight(clean(eased(fs(take, 702245, 0.03, 2.13), 0.001, 1), { low: 30 }), { hz: 70, lift: 2, harmonics: -16 }));

  // ---- The tank -----------------------------------------------------------------------------------------
  const TANK = 'The tank';
  def('tank_start', {
    family: 'ground', group: TANK, title: 'A Sherman tank\'s engine is started', finish: { fadeOut: 0.6 }, battle: 'A tank starts up',
    about: 'A real Sherman M4A3E8 ("Easy Eight") cranked and started: the starter, the catch, and the engine settling.',
  }, (take) => clean(eased(fs(take, 368670, 0, 10), 0.01, 1), { low: 25 }));
  def('tank_idle_sherman_loop', {
    family: 'ground', group: TANK, loop: true, title: 'A Sherman stands with its engine running', battle: 'An American tank stands still (loop)',
    about: 'The same Sherman ticking over after it has started, evened out so that it can go round.',
  }, (take) => cleanLoop(loopFrom(even(fs(take, 368670, 12, 40), { window: 1.5, strength: 0.7 }), { from: 2, seconds: 12, overlap: 2 }), 25));
  def('tank_drive_past', {
    family: 'ground', group: TANK, title: 'A Sherman drives past', finish: { fadeOut: 1 }, battle: 'A tank drives past the camera',
    about: 'A real Sherman driving up, past and away: engine, tracks and running gear.',
  }, (take) => clean(eased(fs(take, 368671, 38, 62), 1, 3), { low: 25 }));
  def('tank_shell_case', {
    family: 'ground', group: TANK, lufs: -24, title: 'An empty shell case clangs to the floor', finish: { fadeOut: 0.2 }, battle: 'A tank fires its main gun (a moment after)',
    about: 'Real brass cases dropped and bouncing; one of the larger, played a little slower so that it rings like a tank shell case.',
  }, (take) => clean(tuned(eased(fs(take, 318964, 23.2, 25.2), 0.002, 0.4), 0.8), { low: 60 }));

  // ---- The sea ------------------------------------------------------------------------------------------
  const SEA = 'The sea';
  def('ship_engine_loop', {
    family: 'sea', group: SEA, loop: true, title: 'A big ship\'s engine runs', battle: 'A ship under way (loop, under the wash)',
    about: 'The engine of a large ship, recorded in port, idling: the deep beat that comes up through the deck.',
  }, (take) => cleanLoop(loopFrom(fs(take, 715171, 0.5, 18), { from: 0.5, seconds: 12, overlap: 2.5 }), 25));
  def('ship_wash_loop', {
    family: 'sea', group: SEA, loop: true, ch: 2, title: 'The bow wave of a ship under way', battle: 'A ship under way (loop)',
    about: 'The heavy bow wave of a ship, from an old effects library given away free (craigsmith). Cut to one swell, so that it goes round with the sea.',
  }, (take) => cleanLoop(loopFrom(fs(take, 438837, 0, 40), { from: 18, seconds: 13.3, overlap: 2.5 }), 30));
  // ship_horn: cut by Brian on the listening board, 18 Nov 14:49Z (file kept in battle-pack/cut-by-brian/)
  def('ship_bell', {
    family: 'sea', group: SEA, lufs: -22, title: 'A ship\'s bell is struck', finish: { fadeOut: 0.6 }, battle: 'Action stations, or a watch changes',
    about: 'A real ship\'s bell cast in 1936, struck once and left to ring.',
  }, (take) => clean(eased(fs(take, 519080, 0.3, 5.95), 0.002, 1.5), { low: 60 }));
  def('shell_splash', {
    family: 'sea', group: SEA, title: 'A shell falls in the sea close by', finish: { fadeOut: 0.4 }, battle: 'A shell misses a ship (a splash beside it)',
    about: 'A big splash of water, mixed by qubodup from free real splashes.',
  }, (take) => clean(eased(fs(take, 442773, 0, 2.17), 0.002, 0.6), { low: 40 }));
  def('depth_charge', {
    family: 'sea', group: SEA, title: 'A depth charge goes off', finish: { fadeOut: 0.8 }, battle: 'A destroyer drops depth charges',
    about: 'A real depth charge going off under the sea, and the water thrown up and falling back, from an old effects library given away free (craigsmith).',
  }, (take) => weight(clean(eased(fs(take, 674886, 0.45, 5.7), 0.002, 1.5), { low: 25 }), { hz: 60, lift: 3, harmonics: -14 }));

  // ---- The air ------------------------------------------------------------------------------------------
  const AIR = 'The air';
  def('spitfire_pass', {
    family: 'air', group: AIR, title: 'A Spitfire flies past', finish: { fadeOut: 0.8 }, battle: 'A British fighter crosses the field',
    about: 'A real Spitfire at a military air show: its second pass, which is clean, with the Merlin engine\'s roar.',
  }, (take) => clean(eased(fs(take, 170477, 34, 47.5), 1.5, 1.5), { low: 30 }));
  def('fw190_pass', {
    family: 'air', group: AIR, title: 'A Focke-Wulf 190 flies past low', finish: { fadeOut: 0.8 }, battle: 'A German fighter crosses the field',
    about: 'A real Focke-Wulf 190A-5 making a low pass at an air show, with its radial engine\'s growl.',
  }, (take) => clean(eased(fs(take, 143562, 0, 8.3), 0.05, 1.5), { low: 30 }));
  def('p47_pass', {
    family: 'air', group: AIR, title: 'A P-47 Thunderbolt flies overhead', finish: { fadeOut: 0.8 }, battle: 'An American fighter crosses the field',
    about: 'A real P-47D Thunderbolt making a low pass overhead.',
  }, (take) => clean(eased(fs(take, 143560, 2.5, 11.5), 1, 1.5), { low: 30 }));
  def('b17_drone_loop', {
    family: 'air', group: AIR, loop: true, title: 'A B-17\'s four engines drone', battle: 'Heavy bombers overhead (loop)',
    about: 'A B-17\'s four engines in steady flight, from an old effects library given away free (craigsmith). It starts at 0:07, where Brian asked for it to start: before that the drone drops out twice.',
  }, (take) => cleanLoop(loopFrom(fs(take, 437862, 7, 34), { from: 0, seconds: 12, overlap: 2.5 }), 30));
  def('stuka_drone_loop', {
    family: 'air', group: AIR, loop: true, title: 'A Stuka\'s engine drones', battle: 'German dive bombers overhead (loop)',
    about: 'A Ju 87 Stuka\'s engine in steady flight, from the same old effects library (craigsmith).',
  }, (take) => cleanLoop(loopFrom(fs(take, 437771, 17, 35), { from: 1, seconds: 12, overlap: 2.5 }), 30));
  def('air_raid_siren', {
    family: 'air', group: AIR, lufs: -22, title: 'An air raid siren wails', finish: { fadeOut: 2, floor: -40 }, battle: 'A raid is coming (a city under the bombers)',
    about: 'A real air raid siren tested in Devonport, England: it winds up and wails.',
  }, (take) => clean(eased(fs(take, 336640, 0.5, 37.4), 0.3, 3), { low: 40 }));

  // ---- Under a fight ------------------------------------------------------------------------------------
  const UNDER = 'Under a fight';
  def('rifles_far_loop', {
    family: 'beds', group: UNDER, loop: true, lufs: -27, ch: 2, title: 'Rifles crackle in the distance', battle: 'A fight further off (loop): rifles in a wood',
    about: 'A Second World War re-enactment battle heard from inside a wood: rifles and the odd machine gun, far off, twenty-eight seconds of it.',
  }, (take) => cleanLoop(loopFrom(fs(take, 170478, 32.5, 63), { from: 0, seconds: 28, overlap: 2 }), 40));
  def('fire_loop', {
    family: 'fire', group: UNDER, loop: true, ch: 2, title: 'A fire roars and crackles', battle: 'A tank, a ship or a building burns (loop)',
    about: 'A large fire roaring and hissing, with the crack of what it is burning.',
  }, (take) => cleanLoop(loopFrom(fs(take, 698209, 30, 46), { from: 1, seconds: 12, overlap: 2 }), 40));
}
