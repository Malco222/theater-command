// The lakes the chart was not handed over with, from the Natural Earth survey (public domain,
// naturalearthdata.com), by the list MORE_LAKE_LIST in data/campaign/detail.mjs. Writes
// data/campaign/more-lakes.mjs: each lake's name, water, level and shore, thinned to what the chart can show.
//
//   node tools/survey-lakes.mjs <ne_10m_lakes.geojson> <ne_10m_lakes_historic.geojson> <ne_10m_playas.geojson>
//
// The survey's files are not kept in the repository: they are downloaded into a folder of their own and read here.
import fs from 'node:fs';
import { MORE_LAKE_LIST, LAKE_HAND, OLD_LAKE_SURVEY } from '../data/campaign/detail.mjs';

const [lakesFile, historicFile, playasFile] = process.argv.slice(2).filter((v) => !v.startsWith('--'));
if (!lakesFile) { console.error('usage: node tools/survey-lakes.mjs <lakes.geojson> <historic.geojson> <playas.geojson>'); process.exit(1); }
const read = (f) => (f ? JSON.parse(fs.readFileSync(f, 'utf8')).features : []);
const files = { lakes: read(lakesFile), historic: read(historicFile), playas: read(playasFile) };
const K = Math.cos((50 * Math.PI) / 180) * 40;
const uv = ([lon, lat]) => [lon * K, -lat * 40];
const fold = (s) => (s ?? '').normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/[’`‘]/g, "'").replace(/\s+/g, ' ').trim();
const areaOf = (ring) => { let a = 0; for (let k = 0; k < ring.length; k++) { const p = uv(ring[k]), q = uv(ring[(k + 1) % ring.length]); a += p[0] * q[1] - q[0] * p[1]; } return Math.abs(a) / 2; };

/** A closed shore thinned: no point is dropped that stands more than 'tol' units from the line drawn without it. */
function thin(ring, tol) {
  const p = ring.map(uv);
  // Cut the ring at its two points farthest apart, and thin each half.
  let far = 0, fd = -1;
  for (let i = 1; i < p.length; i++) { const d = Math.hypot(p[i][0] - p[0][0], p[i][1] - p[0][1]); if (d > fd) { fd = d; far = i; } }
  const half = (a, b) => {
    const keep = new Set([a, b]), stack = [[a, b]];
    while (stack.length) {
      const [s, e] = stack.pop();
      let at = -1, worst = tol;
      const dx = p[e][0] - p[s][0], dy = p[e][1] - p[s][1], L2 = dx * dx + dy * dy;
      for (let i = s + 1; i < e; i++) {
        const t = L2 ? Math.max(0, Math.min(1, ((p[i][0] - p[s][0]) * dx + (p[i][1] - p[s][1]) * dy) / L2)) : 0;
        const d = Math.hypot(p[i][0] - p[s][0] - t * dx, p[i][1] - p[s][1] - t * dy);
        if (d > worst) { worst = d; at = i; }
      }
      if (at > 0) { keep.add(at); stack.push([s, at], [at, e]); }
    }
    return [...keep].sort((x, y) => x - y);
  };
  const first = half(0, far), second = half(far, p.length - 1);
  return [...first, ...second.slice(1)].map((i) => ring[i]);
}

const out = [], notes = [];
for (const [where, surveyName, name, water, level] of MORE_LAKE_LIST) {
  // A shore laid by hand stands in place of the survey's.
  if (LAKE_HAND[name]) {
    const ring = LAKE_HAND[name].map(([lat, lon]) => [lon, lat]);
    out.push({ name, water, level, shore: ring, area: Math.round(areaOf(ring)), kept: Math.round(areaOf(ring)), hand: true });
    continue;
  }
  const found = files[where].filter((ft) => ft.geometry && fold(ft.properties.name) === fold(surveyName));
  if (!found.length) { notes.push(`! ${name}: no ${surveyName} among the survey's ${where}`); continue; }
  // The lake's largest sheet of water (its islands are not drawn).
  const rings = found.flatMap((ft) => (ft.geometry.type === 'Polygon' ? [ft.geometry.coordinates] : ft.geometry.coordinates).map((poly) => poly[0]));
  let ring = rings.sort((a, b) => areaOf(b) - areaOf(a))[0];
  if (ring.length > 1 && ring[0][0] === ring[ring.length - 1][0] && ring[0][1] === ring[ring.length - 1][1]) ring = ring.slice(0, -1);
  let tol = 1, shore = thin(ring, tol);
  while (shore.length > 160) { tol *= 1.3; shore = thin(ring, tol); }
  out.push({ name, water, level, shore, area: Math.round(areaOf(ring)), kept: Math.round(areaOf(shore)) });
}
// The eleven the chart was handed over with, from the same survey at the same grain.
const redrawn = [];
for (const [where, surveyName, name] of OLD_LAKE_SURVEY) {
  const found = files[where].filter((ft) => ft.geometry && fold(ft.properties.name) === fold(surveyName));
  if (!found.length) { notes.push(`! ${name}: no ${surveyName} among the survey's ${where}`); continue; }
  const rings = found.flatMap((ft) => (ft.geometry.type === 'Polygon' ? [ft.geometry.coordinates] : ft.geometry.coordinates).map((poly) => poly[0]));
  let ring = rings.sort((a, b) => areaOf(b) - areaOf(a))[0];
  if (ring.length > 1 && ring[0][0] === ring[ring.length - 1][0] && ring[0][1] === ring[ring.length - 1][1]) ring = ring.slice(0, -1);
  let tol = 1, shore = thin(ring, tol);
  while (shore.length > 160) { tol *= 1.3; shore = thin(ring, tol); }
  redrawn.push({ name, shore, area: Math.round(areaOf(ring)), kept: Math.round(areaOf(shore)) });
}
const q = (v) => JSON.stringify(v);
fs.writeFileSync(new URL('../data/campaign/more-lakes.mjs', import.meta.url), `// Written by tools/survey-lakes.mjs from the lakes of the Natural Earth survey (public domain, naturalearthdata.com)
// and the list MORE_LAKE_LIST in detail.mjs. Not to be changed by hand: change the list and run the tool.

/** The lakes the chart was not handed over with: name, water, level in metres, shore as [lat, lon] points. */
export const MORE_LAKES = [
${out.map((l) => `  [${q(l.name)}, ${q(l.water)}, ${l.level}, ${q(l.shore.map(([lon, lat]) => [Math.round(lat * 1000) / 1000, Math.round(lon * 1000) / 1000]))}],`).join('\n')}
];

/** The eleven lakes the chart was handed over with, redrawn from the survey: name, shore as [lat, lon] points. */
export const REDRAWN_LAKES = [
${redrawn.map((l) => `  [${q(l.name)}, ${q(l.shore.map(([lon, lat]) => [Math.round(lat * 1000) / 1000, Math.round(lon * 1000) / 1000]))}],`).join('\n')}
];
`);
console.log(`${out.length} lakes written`);
if (notes.length) console.log(notes.join('\n'));
if (process.argv.includes('--list')) for (const l of redrawn) console.log(`  ${l.name.padEnd(20)} redrawn ${String(l.area).padStart(5)} square units, ${String(l.shore.length).padStart(3)} points (${l.kept} as drawn)`);
if (process.argv.includes('--list')) for (const l of out) console.log(`  ${l.name.padEnd(20)} ${l.water.padEnd(5)} ${String(l.level).padStart(5)} m  ${String(l.area).padStart(5)} square units, ${String(l.shore.length).padStart(3)} points (${l.kept} as drawn${l.hand ? ', by hand' : ''})`);
