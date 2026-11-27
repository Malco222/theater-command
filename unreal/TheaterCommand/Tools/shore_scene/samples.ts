// Writes the shelling scene's sample staged recordings (sea-scene/shore-guns/plan.md R1) into a folder, one for each
// stand-in report of sample-reports.ts that gets a scene, for the sea scene's cloud test (Tools/sea_show/run.sh
// <folder>/*.json). The night and gale stand-ins get none before R6 and are only listed.
//
//   Tools/shore_scene/run.sh [out-dir]
import { writeFileSync, mkdirSync } from 'node:fs';
import { join } from 'node:path';
import { stageShore, type ShoreNote } from '../../Rules/shore-scene';
import { SAMPLES, sampleFacts, sampleGame } from './sample-reports';

const out = process.argv[2] ?? '.';
mkdirSync(out, { recursive: true });
SAMPLES.forEach((s, i) => {
  const g = sampleGame(s.human);
  const staged = stageShore(sampleFacts(g, s, i + 1));
  if (!staged) {
    if (!s.skip) throw new Error(`${s.file}: no scene`);
    console.log(`${s.file}: no scene (${s.skip}), as R1 skips it`);
    return;
  }
  if (s.skip) throw new Error(`${s.file}: staged, but a ${s.skip} report gets no scene before R6`);
  const text = JSON.stringify(staged);
  writeFileSync(join(out, s.file), text);
  const st = staged.field.staged as ShoreNote;
  console.log(`${s.file}: ${(text.length / 1024).toFixed(0)} KB, ${staged.views.length} views, ${st.seconds} s on screen, beats ${st.beats.map((b) => `${b.what} ${b.from}-${b.to}/${b.screen}`).join(', ')}`);
  console.log(`  ${st.caption.lines.join(' / ')}`);
  console.log(`  drawn ${JSON.stringify(st.drawn)}`);
});
