// TRIAL: builds tools/trial/front.ts the way build.mjs builds seat-play.ts.
import path from 'node:path';
import esbuild from 'esbuild';
import { ROOT, findSeatFiles, mergedText, plan } from './seats.mjs';
const P = plan(process.env.FRONT_SEATS ? process.env.FRONT_SEATS.split(',') : findSeatFiles());
const PT = process.env.PT_HARNESS ?? '/mnt/project-files/playtest/harness';
await esbuild.build({
  entryPoints: [path.join(ROOT, 'tools/trial/front.ts')], bundle: true, platform: 'node', format: 'esm', outfile: path.join(ROOT, process.env.FRONT_OUT ?? 'dist/trial/front.mjs'), logLevel: 'warning',
  alias: { '@tc': path.join(ROOT, 'src'), '@pt': PT },
  define: { TRIAL_SEATS: JSON.stringify(P.seats), TRIAL_FILES: '[]', TRIAL_WARNINGS: '0' },
  plugins: [{ name: 'trial-seats', setup(build) { build.onLoad({ filter: /[\\/]data[\\/]campaign[\\/][^\\/]+\.json$/ }, (a) => { const text = mergedText(P, a.path); return text ? { contents: text, loader: 'json' } : null; }); } }],
});
console.log('built dist/trial/front.mjs');
