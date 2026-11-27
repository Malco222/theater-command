// Builds the campaign prototype into single self-contained pages:
//   dist/theater-command.html   the page body, as published
//   dist/play.html              the same thing wrapped as a complete page you can open from disk
import { build } from 'esbuild';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';

mkdirSync('dist', { recursive: true });
// The land field's own stylesheet, for FIGHT IT YOURSELF on the attack card: the field opens over the campaign in a
// layer of its own and wears it there (src/battle/ui/page.ts, fightOnField).
const FIELD_CSS = ['src/battle/ui/kit/tokens.css', 'src/battle/ui/kit/kit.css', 'src/battle/ui/battle.css'].map((f) => readFileSync(f, 'utf8')).join('\n');
const out = await build({
  entryPoints: ['src/campaign/ui/main.ts'],
  define: { FIELD_CSS: JSON.stringify(FIELD_CSS) },
  bundle: true,
  format: 'iife',
  target: 'es2020',
  minify: true,
  write: false,
  legalComments: 'none',
});
const js = out.outputFiles[0].text.replace(/<\/script/gi, '<\\/script');
// The eight target pictures of the month's report card (the art director's own drawings), each once, as a style rule.
const PICTURES = ['troops', 'fieldworks', 'town', 'fleet', 'factories', 'rail-yard', 'harbour', 'airfield'];
const pictures = PICTURES.map((k) => `.tpic.k-${k} { background-image: url("data:image/svg+xml;base64,${readFileSync(`src/campaign/ui/pictures/target-${k}.svg`).toString('base64')}"); }`).join('\n');
// FIRST COMMAND's notes, pointer and Guide (firsthour.ts) come after the page's own rules.
const css = `${readFileSync('src/campaign/ui/style.css', 'utf8')}\n${pictures}\n${readFileSync('src/campaign/ui/firsthour.css', 'utf8')}\n`;
const fonts = '<link href="https://fonts.googleapis.com/css2?family=Barlow:wght@400;500;600&family=Barlow+Condensed:wght@500;600;700&family=Courier+Prime:wght@400;700&family=PT+Serif:ital,wght@0,400;0,700;1,400&display=swap" rel="stylesheet">';
const page = `<title>Theater Command</title>
${fonts}
<style>
${css}
</style>
<div id="app"></div>
<script>
${js}
</script>
`;
writeFileSync('dist/theater-command.html', page);
writeFileSync('dist/play.html', `<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n<meta name="viewport" content="width=device-width, initial-scale=1">\n</head>\n<body>\n${page}</body>\n</html>\n`);
console.log(`built dist/theater-command.html (${Math.round(page.length / 1024)} KB) and dist/play.html`);
