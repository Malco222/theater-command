import fs from 'node:fs';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
const { chromium } = require('playwright');
const m = JSON.parse(fs.readFileSync('/home/user/theater-command/src/generated/campaign-map.json', 'utf8'));
const names = ['Sicily','Calabria','Apulia','Naples','Cassino','Anzio','Abruzzo','Rome','Marche','Tuscany','Emilia','Liguria','Sardinia','Corsica','Veneto','Lombardy'];
const rs = m.regions.filter((r) => names.includes(r.name));
let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
for (const r of rs) for (const p of r.polys) for (const [x, y] of p[0]) { x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y); }
const col = { mountain: '#b9a58a', hills: '#cfc39a', plains: '#cfe0b0', urban: '#e3c9c9' };
let svg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="${x0 - 10} ${y0 - 10} ${x1 - x0 + 20} ${y1 - y0 + 20}" width="1100" height="${Math.round(1100 * (y1 - y0 + 20) / (x1 - x0 + 20))}" style="background:#9fc4d8">`;
for (const r of rs) {
  for (const p of r.polys) svg += `<path d="${p.map((ring) => 'M' + ring.map((q) => q.join(',')).join('L') + 'Z').join('')}" fill="${col[r.terrain] ?? '#ddd'}" stroke="#333" stroke-width="0.5"/>`;
}
for (const r of rs) { svg += `<circle cx="${r.x}" cy="${r.y}" r="1.2" fill="#000"/><text x="${r.x + 2}" y="${r.y + 1}" font-size="5" font-family="sans-serif">${r.name}</text>`; for (const j of r.nb) { const t = m.regions[j]; if (j > r.id && names.includes(t.name)) svg += `<line x1="${r.x}" y1="${r.y}" x2="${t.x}" y2="${t.y}" stroke="#c00" stroke-width="0.4" stroke-dasharray="2 1"/>`; } }
svg += '</svg>';
const b = await chromium.launch(); const pg = await b.newPage({ viewport: { width: 1120, height: 1300 }, deviceScaleFactor: 1 });
await pg.setContent(`<body style="margin:0">${svg}</body>`); await pg.screenshot({ path: process.argv[2], fullPage: true }); await b.close();
