// Writes the three sample staged recordings of the month's submarine scene (form D1) into a folder (sample-facts.ts),
// for the sea scene's cloud test.
//
//   Tools/sub_scene/run.sh [out-dir]
import { writeFileSync, mkdirSync } from 'node:fs';
import { join } from 'node:path';
import { stageScene, type StagedNote } from '../../Rules/sub-scene';
import { SAMPLES } from './sample-facts';

const out = process.argv[2] ?? '.';
mkdirSync(out, { recursive: true });
for (const s of SAMPLES) {
  const staged = stageScene(s.facts);
  if (!staged) throw new Error(`${s.file}: no scene`);
  const text = JSON.stringify(staged);
  writeFileSync(join(out, s.file), text);
  const st = staged.field.staged as StagedNote;
  console.log(`${s.file}: ${(text.length / 1024).toFixed(0)} KB, ${staged.views.length} views, ${st.case}, ${st.seconds} s on screen, beats ${st.beats.map((b) => `${b.what} ${b.from}-${b.to}/${b.screen}`).join(', ')}; drawn ${JSON.stringify(st.drawn)}`);
}
