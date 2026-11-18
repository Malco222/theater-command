// The battle pack, part six (18 November 2026): new takes for what Brian cut on the listening board at 14:49Z (the
// ship's horn, the hammer, the horses), a rifle line to answer his note on `rifles` ("doesnt sound real"), and the
// three beds the battle mix needs under its fighting (wind on open ground, a wood, rain). The takes carry names of
// their own on the board (Brian's stamps on the cut ones stay as he left them); once he chooses, the chosen file goes
// into the pack under the name the games call: horses_column -> horses, hammer_anvil or hammer_blows -> hammer,
// ship_whistle or ship_horn_blast -> ship_horn, rifles_line -> rifles. Brian's stamps, 18 Nov 15:51 to 15:52Z:
// approved rifles_line (into the pack as rifles), hammer_anvil (as hammer), wind_loop, woods_loop, rain_loop; cut
// horses_column, hammer_blows, ship_whistle, ship_horn_blast (horses and ship_horn stay silent). All Freesound field
// recordings fetched on Brian's Mac (wanted3.json), cut with as little done to them as possible: a cut, a fade, a
// low cut, and for the rifle line a distance for each shot.
//
// Not remade on purpose: the bugle calls. Brian cut both, and WWII infantry did not attack to a bugle (the officer's
// whistle and shouted orders, both approved, carry that moment). A shell whistling in: the two found (a 1940s
// library's long falling whistle and a modern "incoming artillery" with a pure falling tone) are the film
// convention, not what a shell sounds like, so the moment stays as the crack and the blast.
//
// Fetched and measured but not used: an M1 Garand shot with a long flat tail that reads as layered or processed;
// horses in tall grass (the grass louder than the horses); "horses running" (all its weight under 50 Hz); three
// steam whistles (one 33 s blast with no ends, two non-commercial); a modern liner's horn heard across a harbour; a
// shipyard ambience with no clear blows; wind takes buffeting the microphone or with insects or an engine in them;
// two woods takes that are mostly microphone rumble; a light rain with insects chirping through it.

import { fade } from '../kit/dsp.mjs';
import { clean, loopFrom, layers, away, tuned } from '../kit/film.mjs';
import { around } from '../kit/dsp.mjs';

export default function battle6(def) {
  const fs = (take, id, from, to, o = {}) => take('fs_' + id, { from, to, ...o });
  const cleanLoop = (x, low) => around(x, (y) => clean(y, { low }));
  const NEW = 'New: round six (what you cut, and the beds under the battle)';

  // ---- A rifle line: real .303 shots, each at its own distance -------------------------------------------------
  def('rifles_line', {
    ch: 2, family: 'guns', group: NEW, title: 'A line of riflemen fires (to replace "rifles")', finish: { fadeOut: 0.5 },
    battle: 'A line of infantry opens fire (in place of "rifles")',
    about: 'Nine real Lee-Enfield .303 shots (kyles, CC0, three takes) spread along a line from 20 to 120 m, left to right, over three seconds: nearer shots crack, farther ones are duller. No other treatment, unlike the old take, which was a modern firing range made heavier by processing.',
  }, (take) => {
    const shots = [fs(take, 450853, 0.0, 1.9), fs(take, 450854, 0.0, 1.3), fs(take, 450851, 0.16, 2.1)];
    const line = [
      [0.00, 20, -0.5, 0], [0.31, 55, 0.3, 1], [0.52, 35, -0.1, 2], [0.95, 90, 0.6, 0], [1.18, 25, -0.7, 1],
      [1.62, 120, 0.1, 2], [1.90, 45, 0.45, 0], [2.35, 70, -0.3, 1], [2.70, 30, 0.2, 2],
    ];
    const pitch = [1.0, 0.98, 1.02, 0.99, 1.01, 0.97, 1.03, 1.0, 0.985];
    const list = line.map(([at, m, pan, k], i) => ({
      x: away(tuned(fade(shots[k], 0.001, 0.25), pitch[i]), m), at, pan, db: -0.75 * 20 * Math.log10(m / 20),
    }));
    return clean(layers(list, { stereo: true }), { low: 40 });
  });

  // ---- The campaign map's cues Brian cut ----------------------------------------------------------------------
  def('horses_column', {
    family: 'ground', bus: 'world', group: NEW, title: 'A cavalry column moves off',
    battle: 'Cavalry taken in hand or setting off',
    about: 'A large group of men walking with their horses, from a 1940s film studio\'s tape (craigsmith, CC0): hooves and boots together, the way a mounted unit moves on the march. Replaces the galloping horses you cut.',
  }, (take) => clean(fade(fs(take, 480018, 14.0, 17.2), 0.4, 0.8), { low: 50 }));

  def('hammer_anvil', {
    ch: 2, family: 'desk', bus: 'interface', group: NEW, title: 'A hammer on an anvil (take A)',
    battle: 'Something built: a factory, a fort, a ship laid down',
    about: 'One blow of a steel hammer on a large anvil, ringing (Benboncan, CC BY 4.0). A forge, not a carpenter: replaces the hammer and nail you cut.',
  }, (take) => clean(fade(fs(take, 103630, 0.0, 1.5), 0.001, 0.4), { low: 60 }));

  def('hammer_blows', {
    ch: 2, family: 'desk', bus: 'interface', group: NEW, title: 'Two hammer blows (take B)',
    battle: 'Something built (the other take: choose one)',
    about: 'Two blows of a hammer, a workman driving something home (blukotek, CC0).',
  }, (take) => clean(fade(fs(take, 337815, 2.30, 3.75), 0.001, 0.35), { low: 50 }));

  def('ship_whistle', {
    ch: 2, family: 'sea', group: NEW, title: 'A ship sounds her whistle, three blasts (take A)',
    battle: 'A fleet signals, sails or breaks off',
    about: 'A real ship\'s whistle, three short blasts across the water (monotraum, CC0). Warships of the war signalled on steam whistles and sirens.',
  }, (take) => clean(fade(fs(take, 208714, 11.2, 16.3), 0.02, 0.8), { low: 40 }));

  def('ship_horn_blast', {
    ch: 2, family: 'sea', group: NEW, title: 'A big ship sounds one long blast (take B)',
    battle: 'A fleet signals (the other take: choose one)',
    about: 'One long blast of a big ship\'s horn, deep and close (DominikBraun, CC BY 4.0). Replaces the liner\'s fog horn you cut.',
  }, (take) => clean(fade(fs(take, 504567, 0.05, 4.4), 0.01, 0.7), { low: 35 }));

  // ---- Beds under the battle (the mix's layer 1) --------------------------------------------------------------
  def('wind_loop', {
    ch: 2, family: 'beds', loop: true, group: NEW, title: 'Wind over open ground (loop)',
    battle: 'Under a battle on open ground, loudest before the fighting starts',
    about: 'Wind blowing through the grass of a wide open plain (felix.blume, Tierra del Fuego, CC0), from a stretch without the wind buffeting the microphone, its rumble cut away.',
  }, (take) => cleanLoop(loopFrom(fs(take, 139337, 60, 92), { from: 0.5, seconds: 28, overlap: 2 }), 110));

  def('woods_loop', {
    ch: 2, family: 'beds', loop: true, group: NEW, title: 'A wood in the wind (loop)',
    battle: 'Under a battle in or beside a wood',
    about: 'Trees rustling in the wind, heard close (Anya_Media, CC0).',
  }, (take) => cleanLoop(loopFrom(fs(take, 523389, 27, 59), { from: 0.5, seconds: 28, overlap: 2 }), 90));

  def('rain_loop', {
    ch: 2, family: 'beds', loop: true, group: NEW, title: 'Steady rain (loop)',
    battle: 'Under any battle fought in rain',
    about: 'Steady rain on open ground (Benboncan, CC BY 4.0).',
  }, (take) => cleanLoop(loopFrom(fs(take, 119784, 10, 42), { from: 0.5, seconds: 28, overlap: 2 }), 60));
}
