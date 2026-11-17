// TRIAL (far-side seats rig, never for main). Builds the rig: dist/trial/seat-play.mjs.
//
//   node tools/trial/build.mjs                       every *seats.json under /mnt/project-files/far-side-seats/*/
//   node tools/trial/build.mjs FILE [FILE ...]       these seats files, in this order
//   node tools/trial/build.mjs --placeholders-only   no seats file: the placeholder rows alone
//   --check    read and check the seats files, print what was found, build nothing
//   --force    build even though the check found errors (the rows in error are left out by the game in silence)
//
// The seats files are laid over the data files inside the bundle only (tools/trial/seats.mjs): nothing in data/
// is written. The scripted player's month comes from the Playtester's harness (PT_HARNESS, by default
// /mnt/project-files/playtest/harness/lib.ts).
import fs from 'node:fs';
import path from 'node:path';
import esbuild from 'esbuild';
import { ROOT, findSeatFiles, mergedText, plan, validate } from './seats.mjs';

const args = process.argv.slice(2);
const flag = (f) => args.includes(f);
const named = args.filter((a) => !a.startsWith('--'));
const paths = flag('--placeholders-only') ? [] : named.length ? named.map((p) => path.resolve(p)) : findSeatFiles();
for (const p of paths) if (!fs.existsSync(p)) { console.error(`no such seats file: ${p}`); process.exit(2); }

const P = plan(paths);
const V = validate(P);
console.log(`seats files: ${paths.length ? paths.join(', ') : 'none (placeholder rows only)'}`);
console.log(`codes the seats files bring: ${P.claimed.join(' ') || 'none'}`);
console.log(`seats: ${P.seats.map((s) => `${s.id}${s.placeholder ? ' (placeholder)' : ''}`).join(' ')}`);
for (const w of V.warnings) console.log(`  warning: ${w}`);
for (const e of V.errors) console.error(`  ERROR: ${e}`);
if (V.errors.length && !flag('--force')) { console.error(`${V.errors.length} error(s) in the seats files: nothing built`); process.exit(1); }
if (flag('--check')) process.exit(V.errors.length ? 1 : 0);

const PT = process.env.PT_HARNESS ?? '/mnt/project-files/playtest/harness';
if (!fs.existsSync(path.join(PT, 'lib.ts'))) { console.error(`the Playtester's harness is not at ${PT} (set PT_HARNESS to the folder that holds lib.ts)`); process.exit(2); }
const out = path.join(ROOT, 'dist/trial/seat-play.mjs');
await esbuild.build({
  entryPoints: [path.join(ROOT, 'tools/trial/seat-play.ts')], bundle: true, platform: 'node', format: 'esm', outfile: out, logLevel: 'warning',
  alias: { '@tc': path.join(ROOT, 'src'), '@pt': PT },
  define: { TRIAL_SEATS: JSON.stringify(P.seats), TRIAL_FILES: JSON.stringify(paths), TRIAL_WARNINGS: JSON.stringify(V.warnings.length) },
  plugins: [{
    name: 'trial-seats',
    setup(build) {
      build.onLoad({ filter: /[\\/]data[\\/]campaign[\\/][^\\/]+\.json$/ }, (a) => {
        const text = mergedText(P, a.path);
        return text ? { contents: text, loader: 'json' } : null;
      });
    },
  }],
});
console.log(`built ${path.relative(ROOT, out)}`);
