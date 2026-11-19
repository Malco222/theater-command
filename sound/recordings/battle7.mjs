// The battle pack, part seven (19 November 2026): takes the air battles thread asked for in its feel benchmark
// (/mnt/project-files/air-battles/feel/feel-benchmark.md, gaps 4 and 5). They carry names of their own until Brian
// has heard them on the listening board; none is wired.
//
// Tried and not used: a close flak burst cut from flak_burst_01's own source (explosions_ls100155, 4.94 to 9.4 s)
// without the far treatment. It measures the same as the far one (spectral centroid 180 against 182 Hz, nothing over
// 4 kHz) because the recording itself was made far off: 0.2% of its energy is over 2 kHz anywhere in its 10 s. A
// close burst needs a recording made close.
//
// Fetched and not used: "Native Fury 25- Machine Gun Familiarization Fire (975798)" (US Marine Corps, public
// domain): its sound track is music over the picture, not the guns. The range recording already in the log
// (m2_browning_range) fires only 3 or 4 rounds at a time (0.2 to 0.4 s), so a long heavy-gun burst waits on two more
// Commons clips queued for Brian's Mac (/mnt/project-files/sound/incoming/air-2026-11-19/FETCH.md).

import { fade } from '../kit/dsp.mjs';
import { clean, weight } from '../kit/film.mjs';

export default function battle7(def) {
  const NEW = 'New: round seven (the air battle, close)';
}
