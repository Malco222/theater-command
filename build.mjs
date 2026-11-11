// Builds the campaign prototype into single self-contained pages:
//   dist/theater-command.html   the page body, as published
//   dist/play.html              the same thing wrapped as a complete page you can open from disk
import { build } from 'esbuild';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';

mkdirSync('dist', { recursive: true });
const out = await build({
  entryPoints: ['src/campaign/ui/main.ts'],
  bundle: true,
  format: 'iife',
  target: 'es2020',
  minify: true,
  write: false,
  legalComments: 'none',
});
const js = out.outputFiles[0].text.replace(/<\/script/gi, '<\\/script');
const css = readFileSync('src/campaign/ui/style.css', 'utf8');
const fonts = '<link href="https://fonts.googleapis.com/css2?family=Barlow:wght@400;500;600&family=Barlow+Condensed:wght@500;600;700&display=swap" rel="stylesheet">';
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
