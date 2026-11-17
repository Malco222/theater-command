// Builds the fleet action page into one self-contained page:
//   dist/sea-battle.html   the page body, as published
//   dist/sea-battle-play.html   the same wrapped as a complete page you can open from disk
// Its own page, apart from the campaign's (build.mjs) and the field battle's (build-battle.mjs): no press on the campaign screen until the play version ships.
import { build } from 'esbuild';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';

mkdirSync('dist', { recursive: true });
const out = await build({
  entryPoints: ['src/battle/ui/sea-page.ts'],
  bundle: true,
  format: 'iife',
  target: 'es2020',
  minify: true,
  write: false,
  legalComments: 'none',
});
const js = out.outputFiles[0].text.replace(/<\/script/gi, '<\\/script');
const css = ['src/battle/ui/kit/tokens.css', 'src/battle/ui/kit/kit.css', 'src/battle/ui/battle.css', 'src/battle/ui/sea.css'].map((f) => readFileSync(f, 'utf8')).join('\n');
const fonts = '<link href="https://fonts.googleapis.com/css2?family=PT+Serif:ital,wght@0,400;0,700;1,400&family=Courier+Prime:wght@400;700&display=swap" rel="stylesheet">';
const page = `<title>Fleet Action</title>
${fonts}
<style>
${css}
</style>
<div id="app"></div>
<script>
${js}
</script>
`;
writeFileSync('dist/sea-battle.html', page);
writeFileSync('dist/sea-battle-play.html', `<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n<meta name="viewport" content="width=device-width, initial-scale=1">\n</head>\n<body>\n${page}</body>\n</html>\n`);
console.log(`built dist/sea-battle.html (${Math.round(page.length / 1024)} KB) and dist/sea-battle-play.html`);
