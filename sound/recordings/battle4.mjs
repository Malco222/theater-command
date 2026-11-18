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

import { fade } from '../kit/dsp.mjs';
import { clean } from '../kit/film.mjs';

const eased = (x, fadeIn, fadeOut) => fade(x, fadeIn, fadeOut);

export default function battle4(def) {
  const fs = (take, id, from, to, o = {}) => take('fs_' + id, { from, to, ...o });
  const KIT = 'The soldier: feet, kit and bolt';

  // ---- A hit man cries out as he falls ---------------------------------------------------------------------
  [[0.5, 1.15], [3.84, 4.56], [5.0, 5.85], [6.86, 7.4], [11.04, 12.1]].forEach(([a, b], i) => def(`body_fall_cry_0${i + 1}`, {
    family: 'ground', group: KIT, lufs: -24, title: `A man is hit and cries out as he falls (take ${i + 1})`, finish: { fadeOut: 0.15 },
    battle: 'A soldier is hit and falls: about one fall in four, close camera only, in place of a plain fall',
    about: 'A real body fall on dirt with the man\'s own grunt or cry in it, from an old effects library tape given away free (craigsmith). The tape stops near 10 kHz.',
  }, (take) => clean(eased(fs(take, 675923, a, b), 0.01, 0.12), { low: 50 })));
}
