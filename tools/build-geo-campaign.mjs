// Builds the prototype campaign map: one region grown around every seed in
// data/campaign/regions.mjs, clipped to the real coastline.
import fs from 'node:fs';
import { Delaunay } from 'd3-delaunay';
import polygonClipping from 'polygon-clipping';
import { loadLand, clipToBox, simplifyRing, ringArea } from './geo-common.mjs';
import { REGIONS, LINKS, ROADS, NO_BORDER } from '../data/campaign/regions.mjs';
import { buildSeas } from './geo-seas.mjs';

const W = -11, E = 47, S = 28.6, N = 71.4;
const SCALE = 40, KX = Math.cos((50 * Math.PI) / 180);
const proj = ([lon, lat]) => [(lon - W) * KX * SCALE, (N - lat) * SCALE];
const width = (E - W) * KX * SCALE, height = (N - S) * SCALE;
const r1 = (v) => Math.round(v * 10) / 10;

// Land, projected and lightly simplified.
const land = [];
for (const poly of clipToBox(loadLand('50m'), [W, S, E, N])) {
  const rings = [];
  poly.forEach((ring, i) => {
    const r = simplifyRing(ring.map(proj), 0.3);
    if (!r || Math.abs(ringArea(r)) < (i === 0 ? 3 : 6)) return;
    rings.push(r);
  });
  if (rings.length && Math.abs(ringArea(rings[0])) >= 3) land.push(rings);
}

function inRing(ring, x, y) {
  let inside = false;
  for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
    const [xi, yi] = ring[i], [xj, yj] = ring[j];
    if (yi > y !== yj > y && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) inside = !inside;
  }
  return inside;
}
const inPoly = (poly, x, y) => inRing(poly[0], x, y) && !poly.slice(1).some((h) => inRing(h, x, y));

const seeds = REGIONS.map(([name, lat, lon, owner, terrain, vp, fac, flags], id) => {
  const [x, y] = proj([lon, lat]);
  const mass = land.findIndex((p) => inPoly(p, x, y));
  if (mass < 0) console.warn(`! ${name} is not on land (${lat}, ${lon})`);
  return { id, name, owner, terrain, vp, fac, flags, x, y, mass };
});

const delaunay = Delaunay.from(seeds, (s) => s.x, (s) => s.y);
const voronoi = delaunay.voronoi([0, 0, width, height]);

const regions = seeds.map((s) => {
  const cell = voronoi.cellPolygon(s.id);
  let polys = [];
  if (cell && s.mass >= 0) {
    try { polys = polygonClipping.intersection([cell], land[s.mass]); } catch (e) { console.warn('clip failed', s.name); }
  }
  polys = polys
    .map((p) => p.filter((r) => Math.abs(ringArea(r)) > 1.5).map((r) => r.slice(0, -1).map(([x, y]) => [r1(x), r1(y)])))
    .filter((p) => p.length && p[0].length >= 3);
  if (!polys.length) console.warn(`! ${s.name} has no land`);
  return { id: s.id, name: s.name, owner: s.owner, terrain: s.terrain, vp: s.vp, fac: s.fac, flags: s.flags, x: r1(s.x), y: r1(s.y), polys, nb: [] };
});

// Two regions are neighbours when their shared edge runs over land.
for (const s of seeds) {
  const cell = voronoi.cellPolygon(s.id);
  if (!cell) continue;
  for (const j of delaunay.neighbors(s.id)) {
    if (j < s.id) continue;
    const t = seeds[j];
    if (t.mass !== s.mass || s.mass < 0) continue;
    let hits = 0;
    for (let k = 0; k < cell.length - 1; k++) {
      const [ax, ay] = cell[k], [bx, by] = cell[k + 1];
      const eq = (x, y) => Math.abs(Math.hypot(x - s.x, y - s.y) - Math.hypot(x - t.x, y - t.y)) < 0.05;
      if (!eq(ax, ay) || !eq(bx, by)) continue;
      for (let q = 0; q <= 12; q++) {
        const x = ax + ((bx - ax) * q) / 12, y = ay + ((by - ay) * q) / 12;
        if (inPoly(land[s.mass], x, y)) hits++;
      }
    }
    // A shared edge that only clips a headland does not count as a land border.
    if (hits >= 3) { regions[s.id].nb.push(j); regions[j].nb.push(s.id); }
  }
}
const byName = new Map(regions.map((r) => [r.name, r]));
const links = [];
for (const [a, b] of LINKS) {
  const ra = byName.get(a), rb = byName.get(b);
  if (!ra || !rb) { console.warn('bad link', a, b); continue; }
  if (!ra.nb.includes(rb.id)) { ra.nb.push(rb.id); rb.nb.push(ra.id); }
  links.push([ra.id, rb.id]);
}
for (const [a, b] of ROADS) {
  const ra = byName.get(a), rb = byName.get(b);
  if (!ra || !rb) { console.warn('bad road', a, b); continue; }
  if (!ra.nb.includes(rb.id)) { ra.nb.push(rb.id); rb.nb.push(ra.id); }
}
for (const [a, b] of NO_BORDER) {
  const ra = byName.get(a), rb = byName.get(b);
  if (!ra || !rb) { console.warn('bad border', a, b); continue; }
  ra.nb = ra.nb.filter((i) => i !== rb.id);
  rb.nb = rb.nb.filter((i) => i !== ra.id);
}
for (const r of regions) if (!r.nb.length) console.warn(`! ${r.name} has no neighbours`);

const seas = buildSeas({ land, regions, proj, width, height });
regions.forEach((r) => { r.seas = seas.seaOf[r.id]; });
const out = {
  width: Math.round(width), height: Math.round(height),
  land: land.map((p) => p.map((r) => r.slice(0, -1).map(([x, y]) => [r1(x), r1(y)]))),
  regions, links,
  seas: { zones: seas.zones, gates: seas.gates, borders: seas.borders, cell: seas.cell, grid: seas.grid },
};
fs.writeFileSync(new URL('../src/generated/campaign-map.json', import.meta.url), JSON.stringify(out));
const pts = regions.reduce((s, r) => s + r.polys.reduce((t, p) => t + p.reduce((u, q) => u + q.length, 0), 0), 0);
console.log(`campaign map: ${regions.length} regions, ${pts} points, ${Math.round(width)} x ${Math.round(height)}, ${(JSON.stringify(out).length / 1024).toFixed(0)} KB`);
