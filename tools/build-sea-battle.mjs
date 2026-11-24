// Builds the fleet action page into one self-contained page:
//   dist/sea-battle.html   the page body, as published
//   dist/sea-battle-play.html   the same wrapped as a complete page you can open from disk
// Its own page, apart from the campaign's (build.mjs) and the field battle's (build-battle.mjs): no press on the campaign screen until the play version ships.
//
// node tools/build-sea-battle.mjs --embed-fonts   also writes the kit's two faces (PT Serif, Courier Prime: both under
// the SIL Open Font Licence) into the page from disk, so the preview reads in its own type with no network. Without
// the flag, or if the fonts cannot be fetched, the page loads them from Google Fonts as the land page does.
import { build } from 'esbuild';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';

const FONTS = 'https://fonts.googleapis.com/css2?family=PT+Serif:ital,wght@0,400;0,700;1,400&family=Courier+Prime:wght@400;700&display=swap';

/** The Google Fonts stylesheet with every face fetched and written in as data, or null if it cannot be had. */
async function embeddedFonts() {
  try {
    // A current browser's name, so the service answers with woff2; only the Latin subsets are kept.
    const ua = { 'user-agent': 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Safari/537.36' };
    const css = await (await fetch(FONTS, { headers: ua })).text();
    const blocks = css.split('}').filter((b) => b.includes('@font-face') && /\/\* latin \*\//.test(b));
    let out = '';
    for (const b of blocks) {
      const url = b.match(/url\((https:[^)]+)\)/)?.[1];
      if (!url) continue;
      const bytes = Buffer.from(await (await fetch(url)).arrayBuffer());
      out += `${b.slice(b.indexOf('@font-face')).replace(url, `data:font/woff2;base64,${bytes.toString('base64')}`)}}\n`;
    }
    return out || null;
  } catch {
    return null;
  }
}

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
const link = `<link href="${FONTS}" rel="stylesheet">`;
const faces = process.argv.includes('--embed-fonts') ? await embeddedFonts() : null;
const page = (fonts) => `<title>Fleet Action</title>
${fonts}
<style>
${css}
</style>
<div id="app"></div>
<script>
${js}
</script>
`;
writeFileSync('dist/sea-battle.html', page(link));
// --open=N: the page body again, the fonts written in when they could be had, opening straight into practice card N
// (the copy published for Brian opens on "Carriers against a battle line", 1).
const open = process.argv.find((a) => a.startsWith('--open='));
if (open) {
  const n = Number(open.slice(7));
  const opening = `<script>window.SEA_OPEN = { practice: ${JSON.stringify(n)} };</script>\n`;
  writeFileSync('dist/sea-battle-open.html', page(faces ? `<style>\n${faces}</style>` : link).replace('<div id="app"></div>', `<div id="app"></div>\n${opening}`));
  console.log(`built dist/sea-battle-open.html, opening on practice card ${n}`);
}
writeFileSync('dist/sea-battle-play.html', `<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n<meta name="viewport" content="width=device-width, initial-scale=1">\n</head>\n<body>\n${page(faces ? `<style>\n${faces}</style>` : link)}</body>\n</html>\n`);
console.log(`built dist/sea-battle.html (${Math.round(page(link).length / 1024)} KB) and dist/sea-battle-play.html${faces ? ' with the fonts written in' : ''}`);
