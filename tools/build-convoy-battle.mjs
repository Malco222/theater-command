// Builds the convoy night's practice page (release 2; ruling 241 D2) into one self-contained page:
//   dist/convoy-battle.html        the page body, as published
//   dist/convoy-battle-play.html   the same wrapped as a complete page you can open from disk
// Its own page, apart from the campaign's and the fleet action's: no press on the campaign screen until the Playtester
// finds the five practice nights easy (217(5)).
//
// node tools/build-convoy-battle.mjs [--open=N[:att|def]]   also writes dist/convoy-battle-open.html, opening straight
// into practice night N (0 to 4) on the side given (the escorts if none), on seed 1.
import { build } from 'esbuild';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';

const FONTS = 'https://fonts.googleapis.com/css2?family=PT+Serif:ital,wght@0,400;0,700;1,400&family=Courier+Prime:wght@400;700&display=swap';
mkdirSync('dist', { recursive: true });
const out = await build({ entryPoints: ['src/battle/ui/convoy-page.ts'], bundle: true, format: 'iife', target: 'es2020', minify: true, write: false, legalComments: 'none' });
const js = out.outputFiles[0].text.replace(/<\/script/gi, '<\\/script');
const css = ['src/battle/ui/kit/tokens.css', 'src/battle/ui/kit/kit.css', 'src/battle/ui/convoy.css'].map((f) => readFileSync(f, 'utf8')).join('\n');
const page = (opening = '') => `<title>Convoy Night</title>
<link href="${FONTS}" rel="stylesheet">
<style>
${css}
</style>
<div id="app"></div>
${opening}<script>
${js}
</script>
`;
const wrap = (body) => `<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n<meta name="viewport" content="width=device-width, initial-scale=1">\n</head>\n<body>\n${body}</body>\n</html>\n`;
writeFileSync('dist/convoy-battle.html', page());
writeFileSync('dist/convoy-battle-play.html', wrap(page()));
const open = process.argv.find((a) => a.startsWith('--open='));
if (open) {
  const [n, side] = open.slice(7).split(':');
  writeFileSync('dist/convoy-battle-open.html', wrap(page(`<script>window.CONVOY_OPEN = ${JSON.stringify({ practice: Number(n), side: side === 'att' ? 'att' : 'def', seed: 1 })};</script>\n`)));
  console.log(`built dist/convoy-battle-open.html, opening on practice night ${n}`);
}
console.log(`built dist/convoy-battle.html (${Math.round(page().length / 1024)} KB) and dist/convoy-battle-play.html`);
