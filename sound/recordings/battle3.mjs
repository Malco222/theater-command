// The battle pack, part three: the cue review's edit list (18 November 2026, /mnt/project-files/sound/cue-review/).
// More takes of the sounds that had only one, and new cuts from recordings already held: the Freesound finds
// (recordings/freesound) and the archive recordings. Nothing here is synthesised.
//
// Each cut was measured before it was made (tools/bursts.mjs, tools/envelope.mjs). Recordings the review proposed
// but left out here, and why:
//   338240 (rifle duel in a wood) and 184382 (mortar shots): every shot is clipped at source (kurtosis 1.6 to 2.4 in
//     the shots, decoded peaks up to +5 dB).
//   832849 (footsteps on earth): clipped at source (55 to 366 samples at the peak in most steps).
//   croatian_artillery at 20.8 s and m119_4_319_lfx: clipped (kurtosis 1.2 and 2.4 to 2.9).
//   523403: a .22 shot with a ricochet after it, not small arms striking armour.

import { fade, around } from '../kit/dsp.mjs';
import { clean, weight, punch, away, tuned, layers, even, loopFrom, shorten, tailOff } from '../kit/film.mjs';

const eased = (x, fadeIn, fadeOut) => fade(x, fadeIn, fadeOut);

export default function battle3(def) {
  const fs = (take, id, from, to, o = {}) => take('fs_' + id, { from, to, ...o });
  const cleanLoop = (x, low) => around(x, (y) => clean(y, { low }));
  const VOICES = 'Voices', GUNS = 'Wartime guns, one for each army', KIT = 'The soldier: feet, kit and bolt', EXPL = 'Explosions',
    TANK = 'The tank', SEA = 'The sea', AIR = 'The air', UNDER = 'Under a fight';

  // ---- More takes of single sounds ----------------------------------------------------------------------
  const blast = (x, low = 30) => weight(punch(clean(x, { low }), 0.3), { hz: 75, lift: 3, harmonics: -12 });
  def('grenade_blast_02', {
    family: 'guns', group: EXPL, title: 'A grenade goes off (second take)', finish: { fadeOut: 0.3 }, battle: 'A grenade lands',
    about: 'A real close explosion with stones and earth in it, cut short as a grenade\'s bang is.',
  }, (take) => blast(tailOff(fs(take, 108641, 0.08, 1.4), 0.5, 0.25)));
  def('grenade_blast_03', {
    family: 'guns', group: EXPL, title: 'A grenade goes off (third take)', finish: { fadeOut: 0.3 }, battle: 'A grenade lands',
    about: 'The second blast of a real demolition recorded in Poland (julujanus), cut short.',
  }, (take) => blast(tailOff(fs(take, 768370, 11.0, 12.2), 0.45, 0.25)));
  def('shell_impact_near_02', {
    family: 'guns', group: EXPL, title: 'A shell lands close by (second take)', finish: { fadeOut: 0.8 }, battle: 'A shell lands near a unit',
    about: 'The first blast of the same real demolition, with its roll and the debris coming down.',
  }, (take) => blast(eased(fs(take, 768370, 5.1, 10.6), 0.001, 2), 28));
  def('shell_impact_near_03', {
    family: 'guns', group: EXPL, title: 'A shell lands close by (third take)', finish: { fadeOut: 0.8 }, battle: 'A shell lands near a unit',
    about: 'Its second blast, a little further off, with the roll after it.',
  }, (take) => blast(eased(fs(take, 768370, 11.0, 15.5), 0.001, 1.8), 28));
  def('tank_knocked_out_02', {
    family: 'guns', group: TANK, lufs: -17.5, title: 'A tank is knocked out (second take)', finish: { fadeOut: 0.6 }, battle: 'A tank is knocked out',
    about: 'Three real explosions laid together: a sharp hit, the hatch blowing, and a blast with debris rolling on behind.',
  }, (take) => {
    const hit = punch(eased(fs(take, 768370, 11.0, 11.7), 0.001, 0.25), 0.4);
    const hatch = tuned(eased(take('explosions_ls100155', { from: 2.925, to: 4.08 }), 0.001, 0.4), 1.25);
    const roll = eased(fs(take, 108641, 0.3, 4.0), 0.03, 1.5);
    return weight(clean(layers([{ x: hit }, { x: hatch, at: 0.4, db: -5 }, { x: roll, at: 0.8, db: -4 }]), { low: 30 }), { hz: 70, lift: 4, harmonics: -12 });
  });
  def('tank_knocked_out_03', {
    family: 'guns', group: TANK, lufs: -17.5, title: 'A tank is knocked out (third take)', finish: { fadeOut: 0.6 }, battle: 'A tank is knocked out',
    about: 'A close blast with stones in it, a smaller one as the hatch goes, and a demolition\'s long roll as the ammunition burns off.',
  }, (take) => {
    const hit = punch(eased(fs(take, 108641, 0.08, 0.75), 0.001, 0.25), 0.4);
    const hatch = tuned(eased(take('explosions_ls100155', { from: 2.925, to: 4.08 }), 0.001, 0.4), 1.1);
    const roll = eased(fs(take, 768370, 5.4, 10.6), 0.05, 2);
    return weight(clean(layers([{ x: hit }, { x: hatch, at: 0.45, db: -6 }, { x: roll, at: 0.9, db: -5 }]), { low: 30 }), { hz: 70, lift: 4, harmonics: -12 });
  });
  def('tank_gun_02', {
    family: 'guns', group: TANK, title: 'A tank fires its main gun (second take)', finish: { fadeOut: 0.6 }, battle: 'A tank fires its main gun',
    about: 'A real tank firing live on a range in Bulgaria, heard from a little way off, and the report rolling away.',
  }, (take) => weight(punch(clean(eased(take('abrams_livefire_bulgaria', { from: 30.68, to: 33.6 }), 0.001, 1.2), { low: 25 }), 0.3), { hz: 60, lift: 4, harmonics: -12 }));
  def('field_gun_fire_02', {
    family: 'guns', group: EXPL, title: 'A field gun fires (second take)', finish: { fadeOut: 0.5 }, battle: 'A field gun fires',
    about: 'A real 105 mm field gun firing on a live-fire exercise, and its echo.',
  }, (take) => weight(clean(eased(take('m119_10mtn_lfx', { from: 30.45, to: 33.7 }), 0.001, 1.2), { low: 30 }), { hz: 70, lift: 2, harmonics: -16 }));

  // ---- Voices -------------------------------------------------------------------------------------------
  def('soviet_cheer', {
    family: 'calls', group: VOICES, lufs: -21, title: 'Soviet soldiers cheer', finish: { fadeOut: 1 }, battle: 'A Soviet victory',
    about: 'Men cheering in Russian, recorded by Lubini on location for his own film about the end of the war.',
  }, (take) => clean(eased(fs(take, 338321, 0.5, 23.8), 0.1, 2), { low: 80 }));
  def('liberation_cheer', {
    family: 'calls', group: VOICES, lufs: -22, title: 'Soldiers cheer and sing with a band', finish: { fadeOut: 1.5 }, battle: 'A capital is freed',
    about: 'Soldiers cheering and singing to a band, from an old effects library given away free (craigsmith). The old tape stops near 10 kHz.',
  }, (take) => clean(eased(fs(take, 482791, 0.4, 24), 0.3, 3), { low: 60 }));

  // ---- The soldier: feet, kit and bolt ----------------------------------------------------------------------
  [2.1, 3.3, 6.33, 9.39].forEach((t, i) => def(`footstep_0${i + 5}`, {
    family: 'ground', group: KIT, lufs: -28, title: `A boot on gritty ground (step ${i + 5} of 8)`, finish: { fadeOut: 0.05 }, battle: 'A soldier takes a step (walk, run)',
    about: 'One step of a man in boots on gritty gravel: a second set, so a squad\'s feet do not all sound alike.',
  }, (take) => clean(tailOff(eased(fs(take, 530589, t - 0.04, t + 0.55), 0.004, 0.1), 0.2, 0.08), { low: 60 })));
  def('marching_loop_02', {
    family: 'ground', group: KIT, lufs: -26, loop: true, title: 'A second column marches', battle: 'A column on the march (loop), the second of two so they never beat together',
    about: 'Another real squad marching, at a slightly slower step than the first; cut on the beat, sixteen steps that go round.',
  }, (take) => cleanLoop(loopFrom(fs(take, 200322, 0, 10), { from: 0.49, seconds: 8.175, overlap: 0.04 }), 60));
  def('body_fall_05', {
    family: 'ground', group: KIT, lufs: -24, title: 'A man falls to the ground (fifth take)', finish: { fadeOut: 0.2 }, battle: 'A soldier is hit and falls',
    about: 'A real body fall: one heavy thud.',
  }, (take) => clean(eased(fs(take, 346694, 0.3, 1.45), 0.003, 0.3), { low: 40 }));
  def('body_fall_rubble', {
    family: 'ground', group: KIT, lufs: -24, title: 'A man falls into rubble', finish: { fadeOut: 0.4 }, battle: 'A soldier falls in a town',
    about: 'A body falling into debris, with bricks and stones sliding after it.',
  }, (take) => clean(eased(fs(take, 461697, 0.1, 4.6), 0.003, 1), { low: 60 }));
  def('kit_rattle_03', {
    family: 'ground', group: KIT, lufs: -28, title: 'A canteen rattles on a running man', finish: { fadeOut: 0.1 }, battle: 'A soldier gets up or starts to run',
    about: 'A real metal canteen rattling on a belt.',
  }, (take) => clean(eased(fs(take, 204047, 1.7, 2.7), 0.01, 0.2), { low: 120 }));
  def('bolt_enfield_02', {
    family: 'guns', group: KIT, lufs: -24, title: 'A British rifleman works his bolt (second take)', finish: { fadeOut: 0.1 }, battle: 'A rifleman works his bolt between shots',
    about: 'Another real rifle bolt opened and closed, the wait between the strokes taken out.',
  }, (take) => clean(shorten(eased(fs(take, 362346, 0, 2.6), 0.003, 0.1), 0.75, 1.65), { low: 100 }));
  def('grenade_land_stone', {
    family: 'ground', group: KIT, lufs: -24, title: 'A grenade lands on stone', finish: { fadeOut: 0.15 }, battle: 'A grenade lands in a town (before the blast)',
    about: 'A real grenade tossed onto cement: it lands, bounces and rolls.',
  }, (take) => clean(eased(fs(take, 93839, 0.55, 1.75), 0.003, 0.3), { low: 80 }));
  def('garand_reload', {
    family: 'guns', group: GUNS, lufs: -24, title: 'An M1 Garand is reloaded', finish: { fadeOut: 0.2 }, battle: 'An American rifleman reloads (after the ping)',
    about: 'A real M1 Garand reloaded while empty: the clip pressed in and the bolt slamming home.',
  }, (take) => clean(eased(fs(take, 725399, 3.2, 6.5), 0.003, 0.3), { low: 80 }));

  // ---- Guns ---------------------------------------------------------------------------------------------
  def('ricochet_01', {
    family: 'guns', group: GUNS, lufs: -22, title: 'A round ricochets', finish: { fadeOut: 0.1 }, battle: 'A round glances off armour, stone or a plane',
    about: 'One real ricochet whining away.',
  }, (take) => clean(eased(fs(take, 78092, 0, 0.93), 0.002, 0.2), { low: 150 }));
  [[0.1, 3.2], [5.4, 8.8], [11.5, 15.0]].forEach(([a, b], i) => def(`bren_far_0${i + 1}`, {
    family: 'guns', group: GUNS, lufs: -24, title: `A British machine gun far off (Bren, take ${i + 1})`, finish: { fadeOut: 0.4 }, battle: 'A British machine gun fires somewhere along the line',
    about: 'A real Bren gun heard from a distance, from an old effects library given away free (craigsmith). The old tape stops near 9 kHz.',
  }, (take) => clean(eased(fs(take, 675591, a, b), 0.005, 0.8), { low: 50 })));
  [[1.05, 3.3], [6.5, 8.05], [3.3, 4.6]].forEach(([a, b], i) => def(`mg_town_0${i + 1}`, {
    family: 'guns', group: UNDER, lufs: -22, title: `Machine guns among the houses (take ${i + 1})`, finish: { fadeOut: 0.3 }, battle: 'A battle in a town: dropped in now and then',
    about: 'Heavy machine guns firing around the listener, recorded by Lubini on location. One burst each, to drop in at random rather than loop.',
  }, (take) => clean(eased(fs(take, 338241, a, b), 0.003, 0.4), { low: 50 })));
  def('street_battle', {
    family: 'guns', group: UNDER, lufs: -22, title: 'A street battle', finish: { fadeOut: 1 }, battle: 'A battle in a town',
    about: 'Rifles and machine guns in a street, from a First World War re-enactment in Lviv recorded by Lubini: the guns are close to the Second World War\'s.',
  }, (take) => clean(eased(fs(take, 337773, 0.2, 13.5), 0.05, 1.5), { low: 50 }));

  // ---- Explosions and what follows ------------------------------------------------------------------------
  def('bridge_blown', {
    family: 'guns', group: EXPL, title: 'A bridge is blown', finish: { fadeOut: 0.8 }, battle: 'Engineers blow a bridge',
    about: 'A real demolition charge going off, and the debris coming down after it.',
  }, (take) => blast(eased(fs(take, 768370, 4.9, 10.9), 0.001, 2), 25));
  def('building_collapse', {
    family: 'guns', group: EXPL, lufs: -19, title: 'A building comes down', finish: { fadeOut: 1 }, battle: 'A building collapses in a town fight',
    about: 'The rumble of a real building demolition after its charge (the charge itself is left out: it is clipped).',
  }, (take) => clean(eased(fs(take, 125937, 10.55, 22), 0.1, 3), { low: 25 }));

  // ---- The tank -----------------------------------------------------------------------------------------
  def('tank_sherman_drive_loop', {
    family: 'ground', group: TANK, loop: true, title: 'A Sherman drives on', battle: 'An American tank drives (loop; faster or slower with speed)',
    about: 'The same real Sherman at its closest, engine and tracks evened out so that it can go round.',
  }, (take) => cleanLoop(loopFrom(even(fs(take, 368671, 44.5, 53.5), { window: 1.2, strength: 0.8 }), { from: 0.5, seconds: 6.5, overlap: 1.5 }), 25));
  def('light_tank_loop', {
    family: 'ground', group: TANK, loop: true, title: 'A light tank on the move', battle: 'A light tank drives (Japan, Italy, early war; loop)',
    about: 'A real T-26 light tank moving. The recording is very loud; its peaks are rounded here.',
  }, (take) => cleanLoop(loopFrom(even(fs(take, 329800, 0.5, 13.8), { window: 1.2, strength: 0.7 }), { from: 0.5, seconds: 10.5, overlap: 2 }), 25));

  // ---- Fire ---------------------------------------------------------------------------------------------
  def('fire_town_loop', {
    family: 'fire', group: UNDER, loop: true, ch: 2, title: 'A town burns', battle: 'A town or a big building burns (loop)',
    about: 'A real building on fire, from an old effects library given away free (craigsmith).',
  }, (take) => cleanLoop(loopFrom(fs(take, 483313, 5, 37), { from: 0, seconds: 28, overlap: 2.5 }), 40));
  def('fire_small_loop', {
    family: 'fire', group: UNDER, loop: true, ch: 2, title: 'A small fire burns', battle: 'A lorry, a hut or a wreck burns (loop)',
    about: 'A real small fire burning steadily.',
  }, (take) => cleanLoop(loopFrom(fs(take, 249418, 0.5, 16), { from: 0, seconds: 13, overlap: 2 }), 40));
  def('fire_start', {
    family: 'fire', group: UNDER, title: 'Something catches fire', finish: { fadeOut: 1.5 }, battle: 'A hit vehicle or house catches fire',
    about: 'Real wood and metal catching and crackling as a fire takes hold, from the same old effects library.',
  }, (take) => clean(eased(fs(take, 483323, 3.0, 15.0), 0.2, 2.5), { low: 50 }));

  // ---- The sea ------------------------------------------------------------------------------------------
  def('ship_steam_engine_loop', {
    family: 'sea', group: SEA, loop: true, title: 'A steamship\'s engine runs', battle: 'A transport or merchant ship under way (loop)',
    about: 'A real ship\'s steam engine running.',
  }, (take) => cleanLoop(loopFrom(fs(take, 587171, 0.3, 9.4), { from: 0, seconds: 7.5, overlap: 1.5 }), 25));
  def('destroyer_wash_loop', {
    family: 'sea', group: SEA, loop: true, ch: 2, title: 'Choppy water on a small ship\'s bow', battle: 'A destroyer or escort under way (loop)',
    about: 'Real choppy waves slapping against a ship\'s bow.',
  }, (take) => cleanLoop(loopFrom(fs(take, 551436, 0.3, 21.4), { from: 0, seconds: 18.5, overlap: 2.5 }), 30));
  def('sea_day_loop', {
    family: 'sea', group: SEA, loop: true, ch: 2, title: 'The open sea', battle: 'Open water, no engine near (loop)',
    about: 'A real bow wave and the open sea round it, from the steadiest stretch of a long recording (kirkrcarter).',
  }, (take) => cleanLoop(loopFrom(fs(take, 744955, 20, 50.5), { from: 0, seconds: 28, overlap: 2.5 }), 30));
  def('ship_bells_two', {
    family: 'sea', group: SEA, lufs: -22, title: 'Two bells', finish: { fadeOut: 0.6 }, battle: 'A fleet action ends',
    about: 'A real ship\'s bell struck twice, as the watch is kept.',
  }, (take) => clean(eased(fs(take, 77699, 0, 6.06), 0.002, 1.5), { low: 60 }));
  def('depth_charge_02', {
    family: 'sea', group: SEA, title: 'A depth charge goes off (second take)', finish: { fadeOut: 0.8 }, battle: 'A destroyer drops depth charges',
    about: 'A real depth charge: the splash, the fizz as it sinks, and the blast, from an old effects library given away free (craigsmith).',
  }, (take) => weight(clean(eased(fs(take, 438713, 11.3, 17.0), 0.01, 1.5), { low: 25 }), { hz: 60, lift: 3, harmonics: -14 }));
  def('torpedo_hit', {
    family: 'sea', group: SEA, lufs: -19, title: 'A torpedo strikes', finish: { fadeOut: 0.8 }, battle: 'A torpedo or a mine hits a ship',
    about: 'A real underwater explosion throwing up a column of water (Robinhood76, non-commercial licence: fine for a game that is not sold). The recording is very hot; its level is brought down.',
  }, (take) => weight(clean(eased(fs(take, 565947, 0, 5.9), 0.001, 1.5), { low: 25 }), { hz: 60, lift: 3, harmonics: -14 }));

  // ---- The air ------------------------------------------------------------------------------------------
  def('p51_pass', {
    family: 'air', group: AIR, title: 'A P-51 Mustang flies past', finish: { fadeOut: 0.8 }, battle: 'An American fighter crosses the field',
    about: 'A real P-51 with its supercharger howling; its deep rumble is taken out.',
  }, (take) => clean(eased(fs(take, 110736, 0, 9.5), 0.3, 1.5), { low: 60 }));
  def('fighter_engine_loop', {
    family: 'air', group: AIR, loop: true, title: 'A fighter\'s engine in flight', battle: 'Fighters in the air (loop)',
    about: 'A real Spitfire manoeuvring a way off at an air show, its swells evened out so that it can go round.',
  }, (take) => cleanLoop(loopFrom(even(fs(take, 170477, 14, 34), { window: 1.5, strength: 0.8 }), { from: 1, seconds: 14, overlap: 2.5 }), 40));
  def('dogfight', {
    family: 'air', group: AIR, title: 'Fighters tangle overhead', finish: { fadeOut: 1 }, battle: 'A dogfight above the field',
    about: 'A real Zero chasing a P-51 at an air show (Fight2FlyPhoto, non-commercial licence: fine for a game that is not sold).',
  }, (take) => clean(eased(fs(take, 161120, 11, 21), 0.8, 2), { low: 40 }));
  def('b24_drone_loop', {
    family: 'air', group: AIR, loop: true, title: 'A B-24\'s four engines drone', battle: 'A second stream of heavy bombers (loop)',
    about: 'A real B-24 Liberator\'s four engines, from the steadiest part of its pass.',
  }, (take) => cleanLoop(loopFrom(even(fs(take, 391516, 9, 20), { window: 1.5, strength: 0.8 }), { from: 0.5, seconds: 8, overlap: 2 }), 30));
  def('bomber_medium_loop', {
    family: 'air', group: AIR, loop: true, title: 'Twin-engined bombers overhead', battle: 'Medium bombers overhead (loop)',
    about: 'A real B-25 Mitchell at its loudest as it passes, evened out so that it can go round.',
  }, (take) => cleanLoop(loopFrom(even(fs(take, 142887, 18.5, 27), { window: 1.2, strength: 0.85 }), { from: 0.5, seconds: 6, overlap: 1.5 }), 30));
  def('aircraft_takeoff', {
    family: 'air', group: AIR, title: 'Aircraft take off', finish: { fadeOut: 1 }, battle: 'Aircraft scramble or take off',
    about: 'A real B-25 running up and rolling for take-off, cut before its loudest seconds (they are clipped).',
  // Most of it sits low, so the low end's own harmonics are laid in for small speakers.
  }, (take) => weight(clean(eased(fs(take, 329616, 0, 12.5), 0.1, 2), { low: 30 }), { hz: 80, lift: 0, harmonics: -7 }));
  def('aero_engine_idle_loop', {
    family: 'air', group: AIR, loop: true, lufs: -24, title: 'Aircraft engines ticking over', battle: 'Planes on an airfield, engines running (loop)',
    about: 'The same real B-25 ticking over before it rolls.',
  }, (take) => around(loopFrom(fs(take, 329616, 0.5, 8.5), { from: 0, seconds: 6, overlap: 1.5 }), (y) => weight(clean(y, { low: 30 }), { hz: 80, lift: 0, harmonics: -7 })));
  def('air_raid_siren_short', {
    family: 'air', group: AIR, lufs: -22, title: 'An air raid siren (short)', finish: { fadeOut: 1.5 }, battle: 'The map\'s raid warning',
    about: 'The first twelve seconds of the real siren in Devonport: it winds up and wails once.',
  }, (take) => clean(eased(fs(take, 336640, 0.5, 12.5), 0.3, 2.5), { low: 40 }));
  def('air_raid_all_clear', {
    family: 'air', group: AIR, lufs: -22, title: 'The all-clear sounds', finish: { fadeOut: 1.5 }, battle: 'The raid is over',
    about: 'The same real siren holding one steady note: the all-clear.',
  }, (take) => clean(eased(fs(take, 336640, 37.5, 62), 0.5, 3), { low: 40 }));
  def('air_raid_siren_02', {
    family: 'air', group: AIR, lufs: -22, title: 'An air raid siren (another town)', finish: { fadeOut: 1.5 }, battle: 'A raid warning by day',
    about: 'Another real air raid siren, rising and falling.',
  }, (take) => clean(eased(fs(take, 157463, 0, 15), 0.3, 2.5), { low: 40 }));
}
