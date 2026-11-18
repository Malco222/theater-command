// The battle pack, part four (18 November 2026): a hit man who cries out as he falls (Brian chose "sometimes" on the
// cue review's question), and the field gun, the lorry and the half-track the animation thread's third designer
// built. Nothing here is synthesised.
//
// The cries: 675923 (craigsmith, an old effects library tape, CC0) is a man falling on dirt again and again, with a
// grunt or a cry on most falls. The voice and the thud land together, so they cannot be lifted apart; instead the
// falls with a clear voice in them are cut as their own takes, kept apart from the plain falls, so the game can play
// one in place of a plain fall about one time in four, and only with the camera close. Which falls hold a voice was
// measured, not heard: a voice repeats itself (harmonicity 0.55 to 0.93 at a steady or gliding pitch of 160 to 330
// Hz), a thud on dirt does not. Kept: 0.53 s (a short grunt near 310 Hz), 3.9 s (a groan held at 210 to 220 Hz for
// 0.4 s), 5.05 s (a cry falling from 314 to 160 Hz, the clearest), 6.9 s (a short grunt near 308 Hz), 11.1 s (a
// grunt, then a cry from 267 Hz down). The other six falls have no steady pitch in them.
//
// Fetched but not used: howitzer_173rd_995248 (a towed howitzer crew firing) is wind on the microphone from end to
// end (level near 0 dB throughout, one burst 18 s long) with voices at 9 to 11.5 s, so no clean breech can be cut
// from it and field_gun_breech stays silent. jeep_willys_engine is held for a staff car or jeep, not asked for yet.

import { fade, around } from '../kit/dsp.mjs';
import { clean, even, loopFrom } from '../kit/film.mjs';

const eased = (x, fadeIn, fadeOut) => fade(x, fadeIn, fadeOut);

export default function battle4(def) {
  const fs = (take, id, from, to, o = {}) => take('fs_' + id, { from, to, ...o });
  const cleanLoop = (x, low) => around(x, (y) => clean(y, { low }));
  const KIT = 'The soldier: feet, kit and bolt', LORRY = 'Lorries, half-tracks and the field gun', AIR = 'The air';

  // ---- A hit man cries out as he falls ---------------------------------------------------------------------
  [[0.5, 1.15], [3.84, 4.56], [5.0, 5.85], [6.86, 7.4], [11.04, 12.1]].forEach(([a, b], i) => def(`body_fall_cry_0${i + 1}`, {
    family: 'ground', group: KIT, lufs: -24, title: `A man is hit and cries out as he falls (take ${i + 1})`, finish: { fadeOut: 0.15 },
    battle: 'A soldier is hit and falls: about one fall in four, close camera only, in place of a plain fall',
    about: 'A real body fall on dirt with the man\'s own grunt or cry in it, from an old effects library tape given away free (craigsmith). The tape stops near 10 kHz.',
  }, (take) => clean(eased(fs(take, 675923, a, b), 0.01, 0.12), { low: 50 })));

  // ---- The lorry --------------------------------------------------------------------------------------------
  // gmc_cckw_besancon: a real GMC CCKW 353 (the American two-and-a-half-ton lorry of the war) running at a show in
  // Besançon, CC0. Its engine holds steady for the first 8 s (level within 1.1 dB); the firing ladder sits at 45,
  // 90, 135 and 180 Hz.
  def('lorry_idle_loop', {
    family: 'ground', group: LORRY, loop: true, title: 'A wartime lorry stands with its engine running', battle: 'A lorry stands still (loop); the half-track uses it too until its own is in',
    about: 'A real GMC CCKW, the American deuce-and-a-half of the war, ticking over at a show in France; evened out so that it can go round.',
  }, (take) => cleanLoop(loopFrom(even(take('gmc_cckw_besancon', { from: 0, to: 8.6 }), { window: 1.5, strength: 0.7 }), { from: 0.3, seconds: 7, overlap: 1.2 }), 30));

  // m3_halftrack_macarthur: a real M3 half-track (the M16 anti-aircraft version) at Fort MacArthur, California, CC BY 3.0
  // (rafowell). Read by eye and by measure: 15.5 to 29 s the engine ticks over (the vet's steadiest stretch, 1.0 dB
  // wobble; a clean ladder at 120 and 240 Hz); 30 to 47 s it works under load with the running gear clattering (crest
  // 12 to 16, kurtosis 3.7, nothing clipped); 78 to 88 s the engine holds higher revs with no clatter (a level line
  // near 170 Hz), drawing away. Its engine is a White truck engine, so that last stretch also serves the lorry on the
  // move. No voices stand out in the stretches used (the voice band's beat stays at 1 to 5 dB).
  def('halftrack_idle_loop', {
    family: 'ground', group: LORRY, loop: true, title: 'A half-track stands with its engine running', battle: 'A half-track stands still (loop)',
    about: 'A real M3 half-track ticking over at Fort MacArthur, California; evened out so that it can go round.',
  }, (take) => cleanLoop(loopFrom(even(take('m3_halftrack_macarthur', { from: 16, to: 29 }), { window: 1.5, strength: 0.7 }), { from: 0.5, seconds: 10.5, overlap: 1.5 }), 30));
  def('halftrack_drive_loop', {
    family: 'ground', group: LORRY, loop: true, title: 'A half-track on the move', battle: 'A half-track drives (loop; faster or slower with speed)',
    about: 'The same M3 half-track working under load, its tracks and running gear clattering.',
  }, (take) => cleanLoop(loopFrom(even(take('m3_halftrack_macarthur', { from: 31, to: 45 }), { window: 1.2, strength: 0.7 }), { from: 0.5, seconds: 11.5, overlap: 1.5 }), 30));
  def('lorry_drive_loop', {
    family: 'ground', group: LORRY, loop: true, title: 'A lorry engine under way', battle: 'A lorry drives (loop; faster or slower with speed)',
    about: 'The M3 half-track\'s White truck engine holding its revs with no track clatter: the sound of a wartime lorry\'s engine on the move.',
  }, (take) => cleanLoop(loopFrom(even(take('m3_halftrack_macarthur', { from: 78, to: 88.5 }), { window: 1.5, strength: 0.85 }), { from: 0.5, seconds: 8.5, overlap: 1.2 }), 40));

  // ---- The jet -----------------------------------------------------------------------------------------------
  // me262_ila2014: a flying Me 262 (a modern replica, with jet engines) at ILA Berlin 2014, CC BY 3.0 (LEJ.approach/
  // dvldi). 21 to 33 s is its roar at full power, level within 2 dB, with no voice in it (beat under 1.3 dB); the
  // decoded peaks pass 0 dB by 2 dB but only a sample or two sit near them and the kurtosis is 3.0, so the master
  // was hot, not clipped. Elsewhere in both Me 262 films the show's commentary runs over the jet.
  def('jet_engine_loop', {
    family: 'air', group: AIR, loop: true, title: 'An Me 262\'s jets at full power', battle: 'Me 262 jets in the air (loop)',
    about: 'A flying Me 262 (a modern replica with jet engines) roaring at full power at an air show in Berlin; evened out so that it can go round.',
  }, (take) => cleanLoop(loopFrom(even(take('me262_ila2014', { from: 21.5, to: 33 }), { window: 1.5, strength: 0.7 }), { from: 0.3, seconds: 9.5, overlap: 1.5 }), 40));
}
