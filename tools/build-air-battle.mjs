// Builds the air battle page into one self-contained page:
//   dist/air-battle.html   the page body, as published
//   dist/air-battle-play.html   the same wrapped as a complete page you can open from disk
// Its own page, apart from the campaign's (build.mjs), the field battle's (build-battle.mjs) and the fleet action's (build-sea-battle.mjs): no press on the campaign screen until the play version ships.
import { build } from 'esbuild';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';

mkdirSync('dist', { recursive: true });
const out = await build({
  entryPoints: ['src/battle/ui/air-page.ts'],
  bundle: true,
  format: 'iife',
  target: 'es2020',
  minify: true,
  write: false,
  legalComments: 'none',
});
const js = out.outputFiles[0].text.replace(/<\/script/gi, '<\\/script');
const css = ['src/battle/ui/kit/tokens.css', 'src/battle/ui/kit/kit.css', 'src/battle/ui/battle.css', 'src/battle/ui/air.css'].map((f) => readFileSync(f, 'utf8')).join('\n');
const fonts = '<link href="https://fonts.googleapis.com/css2?family=PT+Serif:ital,wght@0,400;0,700;1,400&family=Courier+Prime:wght@400;700&display=swap" rel="stylesheet">';
const page = `<title>Air Battle</title>
${fonts}
<style>
${css}
</style>
<div id="app"></div>
<script>
${js}
</script>
`;
writeFileSync('dist/air-battle.html', page);
writeFileSync('dist/air-battle-play.html', `<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n<meta name="viewport" content="width=device-width, initial-scale=1">\n</head>\n<body>\n${page}</body>\n</html>\n`);
console.log(`built dist/air-battle.html (${Math.round(page.length / 1024)} KB) and dist/air-battle-play.html`);
