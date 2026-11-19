// The battle pack, part seven (19 November 2026): the air battle heard close. Brian's clips for the air battle (the
// air battles thread, 19 Nov) asked for weight, grit and intensity close in; these are real recordings Brian fetched
// on his Mac for that: a B-17's engines heard from inside, rounds striking metal, and long bursts from a .50-calibre
// machine gun. They go to Brian's ear on the listening page before anything is wired; nothing here is in the pack yet.
// Cut with as little done to them as possible: a cut, a fade, a low cut, and a loop seam for the interior.
//
// Fetched and measured but not used: "Stryker Gunnery" (Kevin Payne, DVIDS 895185, public domain, Wikimedia
// Commons): over half its energy is under 120 Hz, it is mostly single shots, and its bursts are far off under a
// running engine, so it gives no close .50.
//
// Measured, for the record: the .50 here is the ground gun (bursts at about 400 to 600 rounds a minute). The bombers'
// own guns fired faster, about 750 to 850 a minute, so these bursts are slower than a B-17 gunner's. The source is
// clipped at the peaks in places (the camera's sound, not our doing); the bursts are cut where it is least clipped.

import { fade, around } from '../kit/dsp.mjs';
import { clean, loopFrom } from '../kit/film.mjs';

export default function battle7(def) {
  const fs = (take, id, from, to, o = {}) => take('fs_' + id, { from, to, ...o });
  const NEW = 'New: round seven (the air battle, close)';

  // ---- Inside the bomber -----------------------------------------------------------------------------------------
  def('b17_interior_loop', {
    family: 'air', loop: true, group: NEW, title: 'Inside a B-17: four engines through the airframe',
    battle: 'The close view of a bomber formation (closeView.bed), under the guns',
    about: 'A B-17\'s four engines recorded from inside the aircraft (craigsmith, CC0): the propellers\' beat at about 68 Hz and a muffled, structure-borne roar, almost nothing over 2 kHz. Thirty seconds of the steadiest part, looped. No other treatment.',
  }, (take) => around(loopFrom(fs(take, 437850, 26.5, 56.5), { from: 0.5, seconds: 27, overlap: 2 }), (y) => clean(y, { low: 30 })));

  // ---- Rounds striking metal ---------------------------------------------------------------------------------------
  // Four single hits from a shooting gallery's metal targets (craigsmith, CC0): two that ring long and two short. A
  // plane's skin is thinner than a steel target, so each is cut at 0.7 s with the ring faded out.
  const hits = [
    ['plane_hit_01', 1.64, 'low, rings long'], ['plane_hit_02', 12.92, 'bright, rings long'],
    ['plane_hit_03', 17.07, 'short'], ['plane_hit_04', 21.79, 'short'],
  ];
  for (const [name, at, kind] of hits) {
    def(name, {
      family: 'guns', lufs: -18, group: NEW, title: `A round strikes metal (${kind})`,
      battle: 'Rounds striking a plane (air.roundsPassing has none for hits yet)',
      about: `One real hit on a metal target at a shooting gallery (craigsmith, CC0), ${kind}; cut at 0.7 s with its ring faded over the last 0.35 s.`,
    }, (take) => clean(fade(fs(take, 675951, at - 0.01, at + 0.7), 0.001, 0.35), { low: 120 }));
  }

  // ---- Long .50 bursts ---------------------------------------------------------------------------------------------
  const bursts = [
    ['hmg_burst_long_01', 2.05, 5.3, 'about 3 s, the densest'],
    ['hmg_burst_long_02', 14.28, 16.28, 'about 2 s'],
    ['hmg_burst_long_03', 18.5, 20.3, 'about 1 s, then its echo'],
  ];
  for (const [name, from, to, kind] of bursts) {
    def(name, {
      ch: 2, family: 'guns', lufs: -18, group: NEW, title: `A long burst from a .50 machine gun (${kind})`,
      battle: 'A bomber\'s gunners firing at a fighter close by',
      about: `U.S. Army soldiers firing an M2 .50-calibre machine gun on a range (Sgt. Michael Westendorf, U.S. Army SETAF-AF, public domain): ${kind}. The ground gun fires slower than a bomber's (about 400 to 600 rounds a minute against 750 to 850), and the source is clipped at some peaks.`,
    }, (take) => clean(fade(take('m2_practice_977575', { from, to }), 0.001, 0.08), { low: 40 }));
  }
}
