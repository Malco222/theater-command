// Builds the gray-box air map: coastlines of south-east England and the Pas-de-Calais
// in local metres (x east, y north) around a reference point in Kent.
import fs from 'node:fs';
import { loadLand, clipToBox, simplifyRing, ringArea } from './geo-common.mjs';

const LAT0 = 51.25, LON0 = 0.6;
const KX = Math.cos((LAT0 * Math.PI) / 180) * 111320, KY = 110574;
const box = [-1.75, 49.95, 3.05, 52.55]; // lon/lat window
const proj = ([lon, lat]) => [Math.round((lon - LON0) * KX), Math.round((lat - LAT0) * KY)];

const polys = clipToBox(loadLand('10m'), box);
const out = [];
for (const poly of polys) {
  const rings = [];
  for (let i = 0; i < poly.length; i++) {
    const r = simplifyRing(poly[i].map(proj), 120);
    if (!r) continue;
    if (Math.abs(ringArea(r)) < 4e5) continue; // drop specks under 0.4 km2
    rings.push(r.slice(0, -1));
    if (i === 0 && rings.length === 0) break;
  }
  if (rings.length) out.push(rings);
}
const result = {
  lat0: LAT0, lon0: LON0, kx: KX, ky: KY,
  bounds: { minX: (box[0] - LON0) * KX, maxX: (box[2] - LON0) * KX, minY: (box[1] - LAT0) * KY, maxY: (box[3] - LAT0) * KY },
  land: out,
};
fs.mkdirSync(new URL('../src/generated/', import.meta.url), { recursive: true });
fs.writeFileSync(new URL('../src/generated/geo-air.json', import.meta.url), JSON.stringify(result));
const pts = out.reduce((s, p) => s + p.reduce((t, r) => t + r.length, 0), 0);
console.log(`air map: ${out.length} land polygons, ${pts} points`);
