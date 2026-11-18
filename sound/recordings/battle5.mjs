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

  // ---- The desk ----------------------------------------------------------------------------------------------
  def('pickup', {
    family: 'desk', bus: 'interface', group: DESK, title: 'An army taken in hand',
    battle: 'A piece lifted off the map table (an army or fleet taken in hand)',
    about: 'A wooden chess piece touched and lifted off a board (Alnotreally, CC0).',
  }, (take) => clean(fade(fs(take, 801348, 2.7, 3.0), 0.002, 0.08), { low: 80 }));
}
