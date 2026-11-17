// Draws a chart file (src/generated/world-next.json unless another is named) as an SVG, to look at what the map
// tools made: the seas in flat colours with their names and the links between them, the provinces by owner with
// their names, the lakes. The chart runs round the globe, so a picture may cross its east edge.
//
//   node tools/draw-chart.mjs out.svg [--file=world-map.json] [--ll=north,west,south,east] [--scale=0.5]
//                                     [--names=0|1|2] [--links=0|1] [--seas=0|1] [--font=1.5] [--labels=file.json] [--title=...]
//
// --ll is in degrees as an atlas writes them (east positive); west may be greater than east to cross the date line.
import fs from 'node:fs';

const args = process.argv.slice(2);
const opt = (name, dflt) => { const a = args.find((s) => s.startsWith(`--${name}=`)); return a ? a.slice(name.length + 3) : dflt; };
const outFile = args.find((s) => !s.startsWith('--')) ?? 'chart.svg';
const map = JSON.parse(fs.readFileSync(new URL(`../src/generated/${opt('file', 'world-next.json')}`, import.meta.url)));
const K = Math.cos((50 * Math.PI) / 180) * 40, W = 360 * K;
const x0f = map.frame.x0, round = map.frame.x1 - map.frame.x0 > W - 1;
const px = (lon) => { let x = (lon + 11) * K; while (x < x0f) x += W; while (x >= x0f + W) x -= W; return x; };
const py = (lat) => (71.4 - lat) * 40;

let bx0 = map.frame.x0, bx1 = map.frame.x1, by0 = map.frame.y0, by1 = map.frame.y1;
const ll = opt('ll', null);
if (ll) {
  const [n, w, s, e] = ll.split(',').map(Number);
  bx0 = px(w); bx1 = px(e); by0 = py(n); by1 = py(s);
  if (bx1 <= bx0) bx1 += W;
}
const scale = Number(opt('scale', 0.25));
const names = Number(opt('names', 1)), showLinks = Number(opt('links', 1)), showSeas = Number(opt('seas', 1));
const title = opt('title', ''), font = Number(opt('font', 1));
// --labels=file.json: { "Province": { "text": "what to write", "dx": 20, "dy": -10 } } moves or rewords a name.
const labels = opt('labels', null) ? JSON.parse(fs.readFileSync(opt('labels', null))) : {};
const wpx = Math.round((bx1 - bx0) * scale), hpx = Math.round((by1 - by0) * scale);
const f1 = (v) => Math.round(v * 10) / 10;
// Everything is drawn at each turn of the globe that falls inside the picture.
const turns = round ? [-W, 0, W].filter((d) => map.frame.x0 + d < bx1 && map.frame.x1 + d > bx0) : [0];
const X = (x, d) => f1((x + d - bx0) * scale), Y = (y) => f1((y - by0) * scale);
const inBox = (x, y, d, pad = 0) => x + d >= bx0 - pad && x + d <= bx1 + pad && y >= by0 - pad && y <= by1 + pad;

const OWNER = {
  USA: '#7d9a6a', UK: '#c98f8f', CAN: '#c9a58f', JAP: '#d9c26a', CHN: '#b7c48a', MAN: '#d8b77a', MGL: '#b9a98d', SOV: '#b0605a', THA: '#9fb7c9',
  FRA: '#7f95c4', NED: '#e0a35c', AUS: '#c7a0c0', NZL: '#b08db0', POR: '#6fa78f', GER: '#8a8a8a', ITA: '#9cc29c', MEX: '#a9c48c', BRA: '#8fbf7a',
};
const hue = (i) => `hsl(${(i * 47) % 360} ${28 + ((i * 13) % 20)}% ${78 + ((i * 7) % 10)}%)`;
const ownerColour = (o) => OWNER[o] ?? '#cfc7b4';
const esc = (s) => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;');
const path = (rings, d) => rings.map((ring) => 'M' + ring.map(([x, y]) => `${X(x, d)} ${Y(y)}`).join('L') + 'Z').join('');
const boxOf = (rings) => { let a = 1e9, b = -1e9, c = 1e9, e = -1e9; for (const [x, y] of rings[0]) { if (x < a) a = x; if (x > b) b = x; if (y < c) c = y; if (y > e) e = y; } return [a, b, c, e]; };
const seen = (rings, d) => { const [a, b, c, e] = boxOf(rings); return b + d >= bx0 && a + d <= bx1 && e >= by0 && c <= by1; };

const out = [];
out.push(`<svg xmlns="http://www.w3.org/2000/svg" width="${wpx}" height="${hpx}" viewBox="0 0 ${wpx} ${hpx}" font-family="Helvetica, Arial, sans-serif">`);
out.push(`<rect width="${wpx}" height="${hpx}" fill="#e9eef0"/>`);

// The seas, a run of squares at a time. The grid is coarse, so each sea is first let out two squares over the land
// beside it, which the land then covers: no pale fringe shows along a coast or round a small island.
const { cell, zones } = map.seas;
const grid = (() => {
  const cols = map.seas.cols ?? Math.max(...map.seas.grid.map((row) => row.reduce((t, v, i) => t + (i % 2 ? v : 0), 0)));
  const rows = map.seas.grid.length;
  let at = new Int16Array(cols * rows).fill(-1);
  map.seas.grid.forEach((row, r) => { let c = 0; for (let k = 0; k < row.length; k += 2) { at.fill(row[k], r * cols + c, r * cols + Math.min(cols, c + row[k + 1])); c += row[k + 1]; } });
  for (let pass = 0; pass < 2; pass++) {
    const next = at.slice();
    for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
      if (at[r * cols + c] >= 0) continue;
      for (const [dc, dr] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
        const rr = r + dr, cc = round ? (c + dc + cols) % cols : c + dc;
        if (rr < 0 || rr >= rows || cc < 0 || cc >= cols) continue;
        if (at[rr * cols + cc] >= 0) { next[r * cols + c] = at[rr * cols + cc]; break; }
      }
    }
    at = next;
  }
  const out2 = [];
  for (let r = 0; r < rows; r++) {
    const row = [];
    let last = -2, run = 0;
    for (let c = 0; c < cols; c++) { const z = at[r * cols + c]; if (z === last) { run++; continue; } if (run) row.push(last, run); last = z; run = 1; }
    row.push(last, run);
    out2.push(row);
  }
  return out2;
})();
if (showSeas) {
  const runs = new Map(); // zone -> path
  for (const d of turns) {
    grid.forEach((row, r) => {
      const y = map.frame.y0 + r * cell;
      if (y + cell < by0 || y > by1) return;
      let c = 0;
      for (let k = 0; k < row.length; k += 2) {
        const z = row[k], n = row[k + 1];
        if (z >= 0) {
          const xa = map.frame.x0 + c * cell + d, xb = xa + n * cell;
          if (xb >= bx0 && xa <= bx1) {
            let p = runs.get(z); if (!p) runs.set(z, (p = []));
            p.push(`M${f1((xa - bx0) * scale)} ${Y(y)}h${f1(n * cell * scale)}v${f1(cell * scale + 0.3)}h${f1(-n * cell * scale)}Z`);
          }
        }
        c += n;
      }
    });
  }
  for (const [z, p] of runs) out.push(`<path d="${p.join('')}" fill="${hue(z)}"/>`);
}
// Land that belongs to no province shows through in grey.
for (const d of turns) {
  const p = map.land.filter((poly) => seen(poly, d)).map((poly) => path(poly, d)).join('');
  if (p) out.push(`<path d="${p}" fill="#d6d2c8" fill-rule="evenodd" stroke="none"/>`);
}
for (const d of turns) {
  for (const r of map.regions) {
    const polys = r.polys.filter((poly) => seen(poly, d));
    if (!polys.length) continue;
    out.push(`<path d="${polys.map((poly) => path(poly, d)).join('')}" fill="${ownerColour(r.owner)}" fill-rule="evenodd" stroke="#5a5347" stroke-width="${r.id >= (map.stages?.[0] ?? 1e9) + map.first ? 0.6 : 0.3}" stroke-linejoin="round"/>`);
  }
  for (const lake of map.lakes ?? []) {
    if (!lake.outline || !seen([lake.outline], d)) continue;
    out.push(`<path d="${path([lake.outline], d)}" fill="#bcd3e0" stroke="#7d97a6" stroke-width="0.4"/>`);
  }
}
if (showSeas) {
  for (const d of turns) for (const line of map.seas.borders) {
    if (!line.some(([x, y]) => inBox(x, y, d))) continue;
    out.push(`<path d="M${line.map(([x, y]) => `${X(x, d)} ${Y(y)}`).join('L')}" fill="none" stroke="#7f98a8" stroke-width="0.7" stroke-dasharray="4 3"/>`);
  }
}
// The links between seas: a line from anchor to anchor, the short way round.
if (showLinks && showSeas) {
  for (const d of turns) zones.forEach((z, i) => {
    for (const j of z.adj) {
      if (j < i) continue;
      const o = zones[j];
      let ox = o.x; if (round) { if (ox - z.x > W / 2) ox -= W; else if (z.x - ox > W / 2) ox += W; }
      if (!inBox(z.x, z.y, d, 200) && !inBox(ox, o.y, d, 200)) continue;
      out.push(`<path d="M${X(z.x, d)} ${Y(z.y)}L${X(ox, d)} ${Y(o.y)}" stroke="#35607a" stroke-width="1.1" opacity="0.55"/>`);
    }
  });
}
const halo = 'paint-order="stroke" stroke="#fff" stroke-width="${f1(2.4 * font)}" stroke-linejoin="round"';
if (showSeas) {
  for (const d of turns) zones.forEach((z, i) => {
    if (!inBox(z.x, z.y, d)) return;
    out.push(`<circle cx="${X(z.x, d)}" cy="${Y(z.y)}" r="2.6" fill="#35607a"/>`);
    if (names) out.push(`<text x="${X(z.x, d)}" y="${f1(Y(z.y) - 5)}" font-size="${f1((names > 1 ? 11 : 9.5) * font)}" font-style="italic" fill="#1f4257" text-anchor="middle" ${halo}>${esc(z.name)}${names > 1 ? ` (${i})` : ''}</text>`);
  });
}
if (names) {
  const first = names > 1 ? 0 : map.first + (map.stages?.[0] ?? 0);
  for (const d of turns) for (const r of map.regions) {
    if (r.id < first || !inBox(r.x, r.y, d)) continue;
    const small = r.flags?.islet, own = labels[r.name] ?? {};
    const lx = X(r.x, d) + (own.dx ?? 0), ly = Y(r.y) + (own.dy ?? (small ? 12 : -4) * font);
    if (own.dx || own.dy) out.push(`<path d="M${X(r.x, d)} ${Y(r.y)}L${f1(lx)} ${f1(ly - 3 * font)}" stroke="#2b2620" stroke-width="0.7"/>`);
    out.push(`<circle cx="${X(r.x, d)}" cy="${Y(r.y)}" r="${f1((r.vp ? 2.2 : 1.4) * font)}" fill="#2b2620"/>`);
    out.push(`<text x="${f1(lx)}" y="${f1(ly)}" font-size="${f1((r.vp ? 10 : 8.5) * font)}" font-weight="${r.vp ? 700 : 400}" fill="#2b2620" text-anchor="middle" ${halo}>${esc(own.text ?? r.name)}</text>`);
  }
}
if (title) out.push(`<text x="12" y="24" font-size="18" font-weight="700" fill="#22303a" ${halo}>${esc(title)}</text>`);
out.push('</svg>');
fs.writeFileSync(outFile, out.join('\n'));
console.log(`${outFile}: ${wpx} by ${hpx}, ${Math.round(fs.statSync(outFile).size / 1024)} KB`);
