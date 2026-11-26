// The setup step as the general's walk plays it (release 2; the Mac builder's -TheaterLandScript run, 22 November): a live
// battle opened with his side deploying, quiet pieces while he looks, two of his units set down in the woods (each place a
// reset piece: every man stands again), the battle begun, and the fight after it. Written as the 3D game hears it, for
// land_live_setup_test.cpp, which plays the game's own cores through it frame by frame, as the scene does.
//
// Built and run by setup.sh:  node land-live-setup.mjs <out folder> [key] [seed] [until]
import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { livePiece } from '../../../../src/battle/land/live';

const [folder = '.', key = 'normandy-1944', seedArg = '11', untilArg = '90'] = process.argv.slice(2);
const L = livePiece(key, Number(seedArg), { me: 'att', setup: true });
const header = L.header();
const lines: string[] = [];
const quiet = (n: number): void => { for (let i = 0; i < n; i++) lines.push(JSON.stringify(L.tick(0.5, 1000))); };
quiet(6);
// (the walk's two units and their woods, as land_command_test's walk finds them on this field: setup.sh passes them)
const places = (process.env.LAND_SETUP_PLACES ?? '1:1316:339,2:1316:276').split(',').map((p) => p.split(':').map(Number));
for (const [id, x, y] of places) {
  const why = L.canPlace(id, x, y);
  if (!L.place(id, x, y)) throw new Error(`block ${id} not placed at ${x}, ${y}: ${why}`);
  quiet(12);
}
if (!L.begin()) throw new Error('the battle would not begin');
let guard = 0;
while (!L.sim.over && L.sim.t < Number(untilArg) && guard++ < 100000) lines.push(JSON.stringify(L.tick(0.5, 1000)));
mkdirSync(folder, { recursive: true });
writeFileSync(join(folder, 'header.json'), JSON.stringify(header));
writeFileSync(join(folder, 'deltas.jsonl'), lines.join('\n') + '\n');
const resets = lines.filter((l) => l.includes('"reset":true')).length;
console.log(`${key}: the setup step with ${places.length} units placed; ${lines.length} pieces (${resets} resets) to t=${L.sim.t.toFixed(1)}`);
