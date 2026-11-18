// The battle pack, part five (18 November 2026): the 3D game's last stand-in sounds (the desk, the newsreel, the drums,
// the horses and the two stingers) and the ship and aircraft cues that had no recording at all. The 3D builder and
// the ship and aircraft models asked for them; the names are the ones the games already call. Nothing here is
// synthesised: each is cut from real recordings, now and then two laid together, and the board says which.
//
// Where they come from:
// - craigsmith's tapes (Freesound, CC0): first-generation copies of 1930s and 1940s optical film sound effects, so
//   they are of the war's own time. G45 is a submarine tape, G33 a war tape, G26 and R29 crash tapes. Most tracks
//   open with a spoken slate (a man reading the track's name); every cut here starts after it.
// - Wikimedia Commons: military bands and buglers (public domain), Work With Sounds' museum machines (CC BY 4.0),
//   a 35 mm cinema projector, a real carrier's aircraft lift, a US Navy submarine's dive klaxon.
// - Freesound field recordings: a skydiver's canopy opening, a hand winch, a steel ramp, a boat hull on shingle,
//   a rolling steel drum, chess pieces, a moored steel boat groaning against its fenders.
//
// Left silent on purpose: bomb_doors. No real recording of a bomber's bay doors was found (only space-ship doors
// and bus doors), and from outside the aircraft the doors are lost under the engines anyway.
//
// Fetched and measured but not used: the JS Hyuga lift (clipped), the Ryou-Un Maru sinking (US Coast Guard film: the
// cutter and the wind, not the ship), scrap steel, a coal crane and a chain (the hand winch and the steel ramp fit
// better), a parachute filmed by its jumper (wind on the microphone), the USS Curtis Wilbur torpedo (a modern air
// launch; the 1940s tape is of the war's own time), the Last Post, Reveille (talking on it), two more drum takes, a
// signal bell, Static B and Radio Oranje's 1941 tune (the amateur's Morse has its own static).

import { fade } from '../kit/dsp.mjs';
import { clean, even, loopFrom, layers, weight } from '../kit/film.mjs';
import { around } from '../kit/dsp.mjs';

export default function battle5(def) {
  const fs = (take, id, from, to, o = {}) => take('fs_' + id, { from, to, ...o });
  const cleanLoop = (x, low) => around(x, (y) => clean(y, { low }));
  const DESK = 'New: the desk, the newsreel, drums, horses and the stingers', SEA = 'New: submarines, torpedoes and depth charges', AIR = 'New: aircraft, parachutes and landing craft';

  // ---- Submarines, torpedoes and depth charges ---------------------------------------------------------------
  def('torpedo_launch', {
    family: 'sea', group: SEA, title: 'A torpedo leaves its tube',
    battle: 'A submarine or destroyer fires a torpedo',
    about: 'A torpedo fired from a submarine, from a 1940s film studio\'s submarine tape (craigsmith, given away free): the rush of air as it leaves the tube.',
  }, (take) => clean(fade(fs(take, 438715, 1.45, 3.95), 0.005, 0.25), { low: 35 }));

  def('torpedo_run_loop', {
    family: 'sea', group: SEA, loop: true, title: 'A torpedo running (loop)',
    battle: 'A torpedo runs towards its target (loop)',
    about: 'A submarine\'s motor heard under water, from the same 1940s submarine tape: a steady whine, the sound a running torpedo makes on a hydrophone. Evened out so that it can go round.',
  }, (take) => cleanLoop(loopFrom(even(fs(take, 438722, 3, 33), { window: 1.5, strength: 0.7 }), { from: 0.5, seconds: 9, overlap: 1.5 }), 40));

  def('ship_sinking', {
    family: 'sea', group: SEA, title: 'A ship goes down',
    battle: 'A ship sinks',
    about: 'Two real recordings laid together: a moored steel boat groaning and clanking against its fenders (nebulousflynn), and under it a deep underwater explosion from a 1940s submarine tape (craigsmith), as her boilers go.',
  }, (take) => {
    const groan = fade(fs(take, 269061, 146.0, 155.0), 0.3, 1.5);
    const deep = fade(fs(take, 438737, 5.45, 11.7), 0.01, 2.0);
    return weight(clean(layers([{ x: groan }, { x: deep, at: 1.6, db: -5 }]), { low: 28 }), { hz: 60, lift: 3, harmonics: -14 });
  });

  def('depth_charge_roll', {
    family: 'sea', group: SEA, title: 'A depth charge rolls off the stern rack',
    battle: 'A destroyer rolls a depth charge off its stern',
    about: 'A steel drum rolling on a hard floor (a depth charge is a steel drum of explosive, rolled off rails at the stern). CC0, Mateusz_Chenc.',
  }, (take) => clean(fade(fs(take, 512215, 0.0, 2.6), 0.02, 0.4), { low: 40 }));

  def('depth_charge_thrower', {
    family: 'sea', group: SEA, title: 'A depth charge thrower fires',
    battle: 'A destroyer\'s depth charge thrower (K-gun) fires',
    about: 'A mortar firing, from a 1940s film studio\'s war tape (craigsmith). A K-gun throws its charge with a powder charge just as a mortar does; no recording of a K-gun itself was found.',
  }, (take) => clean(fade(fs(take, 438507, 14.58, 15.5), 0.003, 0.2), { low: 35 }));

  def('dive_alarm', {
    family: 'sea', group: SEA, title: 'A submarine\'s dive klaxon',
    battle: 'A submarine dives',
    about: 'A real WWII US submarine\'s diving klaxon, recorded by the United States Navy (public domain): two blasts, the signal to dive.',
  }, (take) => clean(fade(take('sub_dive_klaxon_wwii', { from: 0.92, to: 2.83 }), 0.005, 0.15), { low: 80 }));

  def('surfacing', {
    family: 'sea', group: SEA, title: 'A submarine surfaces: three blasts',
    battle: 'A submarine surfaces',
    about: 'The same US Navy diving klaxon sounded three times, the signal to surface aboard American submarines. No recording of a submarine breaking the surface was found, so this is what the crew heard rather than the sea.',
  }, (take) => clean(fade(take('sub_dive_klaxon_wwii', { from: 0.92, to: 3.77 }), 0.005, 0.15), { low: 80 }));

  def('lift_motor', {
    family: 'sea', group: AIR, title: 'A carrier\'s aircraft lift runs',
    battle: 'A carrier\'s deck lift moves an aircraft',
    about: 'A real aircraft carrier\'s deck lift going down and stopping (JS Ise, FLYWITHMETOTHEMOON, CC BY 3.0).',
  }, (take) => clean(fade(take('carrier_lift_ise', { from: 48.0, to: 57.5 }), 0.5, 0.8), { low: 40 }));

  // ---- Aircraft, parachutes and landing craft ----------------------------------------------------------------
  def('aircraft_break_up', {
    family: 'air', group: AIR, title: 'An aircraft breaks up',
    battle: 'A shot-down aircraft comes apart',
    about: 'Large metal vehicles crashing and tearing, from a vintage crash tape (craigsmith, CC0): the steel coming apart.',
  }, (take) => weight(clean(fade(fs(take, 487143, 7.6, 11.8), 0.005, 0.8), { low: 35 }), { hz: 70, lift: 2, harmonics: -14 }));

  def('bomb_release', {
    family: 'air', group: AIR, title: 'Bombs fall away',
    battle: 'Bombs are released and fall',
    about: 'A falling bomb\'s rush and whistle, from a 1940s film studio\'s war tape (craigsmith, CC0), cut before it lands so that the bomb\'s own hit plays apart.',
  }, (take) => clean(fade(fs(take, 438514, 3.95, 5.72), 0.05, 0.15), { low: 40 }));

  def('parachute_open', {
    family: 'air', group: AIR, title: 'A parachute opens',
    battle: 'A paratrooper\'s canopy opens',
    about: 'A real parachute canopy opening, from a skydiver\'s recordings (Huggy13ear, CC BY 4.0).',
  }, (take) => clean(fade(fs(take, 138976, 0.6, 2.7), 0.05, 0.4), { low: 40 }));

  def('ramp_winch', {
    family: 'sea', group: AIR, title: 'A landing craft\'s ramp winch lets go',
    battle: 'A landing craft\'s ramp is let go: the winch runs out',
    about: 'A hand winch\'s ratchet run out and its quick release (Benboncan, CC BY 4.0): the ramp\'s cable paying out.',
  }, (take) => clean(fade(fs(take, 69177, 1.3, 2.95), 0.005, 0.15), { low: 40 }));

  def('ramp_drop', {
    family: 'sea', group: AIR, title: 'A landing craft\'s ramp drops',
    battle: 'A landing craft\'s ramp comes down',
    about: 'A steel ramp let down, clanking on its chains (Gingie, CC0).',
  }, (take) => weight(clean(fade(fs(take, 346856, 0.12, 3.6), 0.003, 0.6), { low: 35 }), { hz: 80, lift: 2, harmonics: -14 }));

  def('craft_grounds', {
    family: 'sea', group: AIR, title: 'A landing craft runs aground',
    battle: 'A landing craft grounds on the beach',
    about: 'A boat\'s hull dragged up a shingle shore (brunoboselli, CC BY 4.0): the keel grinding onto the beach.',
  }, (take) => weight(clean(fade(fs(take, 488335, 13.2, 15.8), 0.02, 0.4), { low: 35 }), { hz: 70, lift: 3, harmonics: -14 }));

  // ---- The desk, the newsreel, drums, horses and the stingers -------------------------------------------------

  def('click', {
    family: 'desk', bus: 'interface', group: DESK, title: 'A key struck',
    battle: 'Any button pressed',
    about: 'One key of a real typewriter struck (Work With Sounds, a European museum project; CC BY 4.0).',
  }, (take) => clean(fade(take('desk_typewriter_wws', { from: 0.545, to: 0.665 }), 0.001, 0.04), { low: 90 }));

  def('stamp', {
    family: 'desk', bus: 'interface', group: DESK, title: 'A rubber stamp comes down',
    battle: 'An order confirmed; the WON stamp (fire it as the stamp lands)',
    about: 'A rubber stamp brought down on its ink pad (Subhashish Panigrahi, CC BY 3.0).',
  }, (take) => clean(fade(take('desk_rubber_stamp', { from: 12.5, to: 12.9 }), 0.001, 0.12), { low: 60 }));

  def('tick', {
    family: 'desk', bus: 'interface', group: DESK, title: 'A clock ticks once',
    battle: 'A day or a turn ticks by',
    about: 'One tick of a real clock (public domain).',
  }, (take) => clean(fade(take('desk_clock_ticking', { from: 7.09, to: 7.17 }), 0.001, 0.03), { low: 120 }));

  def('coins', {
    family: 'desk', bus: 'interface', group: DESK, title: 'Coins counted down',
    battle: 'Money spent',
    about: 'Coins dropped into a wooden money box (public domain).',
  }, (take) => clean(fade(take('desk_coins_wooden_box', { from: 0.26, to: 1.3 }), 0.002, 0.2), { low: 90 }));

  def('hammer', {
    family: 'desk', bus: 'interface', group: DESK, title: 'A hammer drives a nail',
    battle: 'Something built: a factory, a fort, a ship laid down',
    about: 'A hammer driving a nail, a tap and then the blow, the nail ringing (Antti Luode, CC BY 3.0).',
  }, (take) => clean(fade(take('desk_hammer_nails', { from: 4.95, to: 6.4 }), 0.002, 0.3), { low: 60 }));

  def('bell', {
    family: 'desk', bus: 'interface', group: DESK, title: 'A tower clock strikes',
    battle: 'The month\'s end',
    about: 'Three strokes of a real tower clock\'s bell (Work With Sounds, CC BY 4.0).',
  }, (take) => clean(fade(take('desk_tower_clock', { from: 8.3, to: 11.55 }), 0.002, 0.8), { low: 60 }));

  def('projector', {
    family: 'desk', bus: 'interface', group: DESK, title: 'A cinema projector runs',
    battle: 'The newsreel starts: the projector',
    about: 'A real 35 mm cinema projector starting and running (Carlosjvives, CC BY 4.0).',
  }, (take) => clean(fade(take('newsreel_projector_proyecson', { from: 1.0, to: 7.0 }), 0.05, 0.8), { low: 50 }));

  def('newsreel', {
    ch: 2, family: 'desk', bus: 'interface', group: DESK, title: 'The newsreel\'s fanfare',
    battle: 'The newsreel\'s opening',
    about: 'The opening of a ceremonial fanfare played by a United States Air Force band (public domain).',
  }, (take) => clean(fade(take('newsreel_fanfare_ceremonial', { from: 2.75, to: 6.4 }), 0.005, 0.6), { low: 50 }));

  def('radio', {
    family: 'desk', bus: 'interface', group: DESK, title: 'A wireless message in Morse',
    battle: 'A wireless message: intelligence, spies, a ship\'s report',
    about: 'Morse code taken off the air by a radio amateur, with the band\'s own hiss and static crashes (HamRadioOperator73, CC0).',
  }, (take) => clean(fade(take('radio_cw_psalm118', { from: 0.0, to: 2.6 }), 0.05, 0.3), { low: 120 }));

  def('drums', {
    ch: 2, family: 'calls', bus: 'calls', group: DESK, title: 'The drums: two sides drawn up',
    battle: 'Two sides drawn up on the attack card, before a battle is joined',
    about: 'A military side drum and bass drum playing a cadence (public domain).',
  }, (take) => clean(fade(take('drums_cadence_a', { from: 0.35, to: 2.45 }), 0.002, 0.35), { low: 40 }));

  def('horses', {
    family: 'ground', bus: 'world', group: DESK, title: 'Cavalry galloping',
    battle: 'Cavalry taken in hand',
    about: 'Six real horses galloping past (CC0).',
  }, (take) => clean(fade(take('horses_six_galloping', { from: 6.5, to: 9.3 }), 0.3, 0.6), { low: 40 }));

  def('victory', {
    ch: 2, family: 'calls', bus: 'calls', group: DESK, title: 'Victory: To the Colors',
    battle: 'A battle won, a town taken',
    about: 'The opening of To the Colors, the bugle call for raising the flag, played by a United States Air Force band (public domain).',
  }, (take) => clean(fade(take('bugle_to_the_colors', { from: 1.45, to: 4.5 }), 0.005, 0.6), { low: 60 }));

  def('defeat', {
    ch: 2, family: 'calls', bus: 'calls', group: DESK, title: 'Defeat: Taps',
    battle: 'A battle lost, a town fallen, a nation\'s fall',
    about: 'The first three notes of Taps on a bugle, the call played at a soldier\'s funeral (US Army Band, public domain).',
  }, (take) => clean(fade(take('bugle_taps', { from: 0.05, to: 3.6 }), 0.005, 0.9), { low: 60 }));
  def('pickup', {
    family: 'desk', bus: 'interface', group: DESK, title: 'An army taken in hand',
    battle: 'A piece lifted off the map table (an army or fleet taken in hand)',
    about: 'A wooden chess piece touched and lifted off a board (Alnotreally, CC0).',
  }, (take) => clean(fade(fs(take, 801348, 2.7, 3.0), 0.002, 0.08), { low: 80 }));
}
