// Builds the map of the wider world for the grand campaign: the provinces of data/campaign/world.mjs and the
// seas of data/campaign/world-seas.json, laid around the map of Europe without moving anything on it.
// Run after build-geo-campaign.mjs. Europe's provinces keep their numbers, shapes and coasts; its seas keep their
// numbers and simply reach further out into the ocean, until they meet the new ones.
import fs from 'node:fs';
import { Delaunay } from 'd3-delaunay';
import polygonClipping from 'polygon-clipping';
import { loadLand, clipToBox, simplifyRing, ringArea } from './geo-common.mjs';
import { CELL, fill, simplify, smooth, buildSeas } from './geo-seas.mjs';
import { FRAME, WORLD_REGIONS, BEYOND, WORLD_LINKS, WORLD_ROADS, WORLD_NO_BORDER, LAKES } from '../data/campaign/world.mjs';

const core = JSON.parse(fs.readFileSync(new URL('../src/generated/campaign-map.json', import.meta.url), 'utf8'));
const seaData = JSON.parse(fs.readFileSync(new URL('../data/campaign/world-seas.json', import.meta.url), 'utf8'));

// The frame and scale of the map of Europe (build-geo-campaign.mjs). The chart shares its top edge and its scale.
const CW = -11, CE = 47, CS = 28.6, N = 71.4;
const SCALE = 40, KX = Math.cos((50 * Math.PI) / 180);
const proj = ([lon, lat]) => [(lon - CW) * KX * SCALE, (N - lat) * SCALE];
const coreW = (CE - CW) * KX * SCALE, coreH = (N - CS) * SCALE;
if (FRAME.north !== N) throw new Error('the chart must share the top edge of the map of Europe');
// The grid of water cells keeps step with Europe's: the chart's left edge falls on a whole cell.
const gx0 = Math.floor(proj([FRAME.west, N])[0] / CELL) * CELL;
const x1 = proj([FRAME.east, N])[0], y1 = (N - FRAME.south) * SCALE;
const r1 = (v) => Math.round(v * 10) / 10;

function inRing(ring, x, y) {
  let inside = false;
  for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
    const [xi, yi] = ring[i], [xj, yj] = ring[j];
    if (yi > y !== yj > y && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) inside = !inside;
  }
  return inside;
}
const inPoly = (poly, x, y) => inRing(poly[0], x, y) && !poly.slice(1).some((h) => inRing(h, x, y));

// ---- Land ----------------------------------------------------------------------------------------------------

// Everything on the chart outside the map of Europe, with the Great Lakes cut out of it.
const box = [[[0, 0], [coreW, 0], [coreW, coreH], [0, coreH], [0, 0]]];
const round = (ring) => ring.flatMap(([ax, ay], i) => { const [bx, by] = ring[(i + 1) % ring.length]; return [[ax * 0.75 + bx * 0.25, ay * 0.75 + by * 0.25], [ax * 0.25 + bx * 0.75, ay * 0.25 + by * 0.75]]; });
const lakes = LAKES.map(([name, pts]) => {
  // The outline is drawn with a few points: its corners are rounded off twice.
  const ring = round(round(pts.map(([lat, lon]) => proj([lon, lat]))));
  return { name, poly: [[...ring, ring[0]]] };
});
const land = [];
for (const poly of clipToBox(loadLand('50m'), [FRAME.west, FRAME.south, FRAME.east, FRAME.north])) {
  const rings = [];
  poly.forEach((ring, i) => {
    const r = simplifyRing(ring.map(proj), 0.4);
    if (!r || Math.abs(ringArea(r)) < (i === 0 ? 3 : 6)) return;
    rings.push(r);
  });
  if (!rings.length || Math.abs(ringArea(rings[0])) < 3) continue;
  let parts;
  try { parts = polygonClipping.difference(rings, box, ...lakes.map((l) => l.poly)); } catch (e) { console.warn('! could not cut a land mass', e.message); continue; }
  for (const p of parts) if (p.length && Math.abs(ringArea(p[0])) >= 3) land.push(p);
}

// ---- Provinces -----------------------------------------------------------------------------------------------

const first = core.regions.length;
const taken = new Set(core.regions.map((r) => r.name));
const seeds = [];
WORLD_REGIONS.forEach(([name, lat, lon, owner, terrain, vp, fac, flags], i) => {
  const [x, y] = proj([lon, lat]);
  const mass = land.findIndex((p) => inPoly(p, x, y));
  if (mass < 0) console.warn(`! ${name} is not on land (${lat}, ${lon})`);
  if (taken.has(name)) console.warn(`! two provinces are called ${name}`);
  taken.add(name);
  seeds.push({ id: first + i, name, owner, terrain, vp, fac, flags, x, y, mass });
});
for (const [what, lat, lon] of BEYOND) {
  const [x, y] = proj([lon, lat]);
  const mass = land.findIndex((p) => inPoly(p, x, y));
  if (mass < 0) console.warn(`! the seed for ${what} is not on land (${lat}, ${lon})`);
  seeds.push({ id: -1, name: what, x, y, mass });
}
// Each land mass is shared out among the seeds that stand on it, so that no province's ground is cut off
// by a seed across the water.
const groups = new Map();
for (const s of seeds) if (s.mass >= 0) (groups.get(s.mass) ?? groups.set(s.mass, []).get(s.mass)).push(s);
for (const group of groups.values()) {
  if (group.length < 2) { group[0].cell = null; group[0].near = []; continue; }
  const delaunay = Delaunay.from(group, (s) => s.x, (s) => s.y);
  const voronoi = delaunay.voronoi([gx0, 0, x1, y1]);
  group.forEach((s, k) => { s.cell = voronoi.cellPolygon(k); s.near = [...delaunay.neighbors(k)].map((j) => group[j]); });
}

const regions = [];
seeds.forEach((s) => {
  if (s.id < 0) return;
  let polys = [];
  if (s.mass >= 0) {
    // A seed alone on its land mass has all of it.
    try { polys = s.cell ? polygonClipping.intersection([s.cell], land[s.mass]) : [land[s.mass]]; } catch (e) { console.warn('clip failed', s.name); }
  }
  polys = polys
    .map((p) => p.filter((r) => Math.abs(ringArea(r)) > 1.5).map((r) => r.slice(0, -1).map(([x, y]) => [r1(x), r1(y)])))
    .filter((p) => p.length && p[0].length >= 3);
  if (!polys.length) console.warn(`! ${s.name} has no land`);
  regions.push({ id: s.id, name: s.name, owner: s.owner, terrain: s.terrain, vp: s.vp, fac: s.fac, flags: s.flags, x: r1(s.x), y: r1(s.y), polys, nb: [] });
});

// An island with no seed of its own goes with the nearest province, if it lies just off its shore:
// Long Island with New York, Prince Edward Island with the mainland beside it.
const seeded = new Set(seeds.map((s) => s.mass));
const joined = [];
land.forEach((p, m) => {
  if (seeded.has(m) || Math.abs(ringArea(p[0])) > 4000) return;
  let best = null, bd = 70;
  for (const s of seeds) {
    if (s.id < 0) continue;
    for (const [x, y] of p[0]) { const d = Math.hypot(x - s.x, y - s.y); if (d < bd) { bd = d; best = s; } }
  }
  if (!best) return;
  const rings = p.filter((r) => Math.abs(ringArea(r)) > 1.5).map((r) => r.slice(0, -1).map(([x, y]) => [r1(x), r1(y)]));
  if (!rings.length || rings[0].length < 3) return;
  regions[best.id - first].polys.push(rings);
  joined.push(best.name);
});

// Two provinces are neighbours when the line between them runs over land for a fair stretch (BORDER map units).
const BORDER = 8;
const boxOf = new Map();
const bbox = (m) => {
  if (!boxOf.has(m)) { let a = 1e9, b = 1e9, c = -1e9, d = -1e9; for (const [x, y] of land[m][0]) { a = Math.min(a, x); b = Math.min(b, y); c = Math.max(c, x); d = Math.max(d, y); } boxOf.set(m, [a - 1, b - 1, c + 1, d + 1]); }
  return boxOf.get(m);
};
seeds.forEach((s) => {
  if (s.id < 0 || s.mass < 0) return;
  const cell = s.cell;
  if (!cell) return;
  const [x0, y0, x9, y9] = bbox(s.mass);
  for (const t of s.near) {
    if (t.id < 0 || t.id < s.id || t.mass !== s.mass) continue;
    let on = 0;
    for (let q = 0; q < cell.length - 1; q++) {
      let [ax, ay] = cell[q], [bx, by] = cell[q + 1];
      const eq = (x, y) => Math.abs(Math.hypot(x - s.x, y - s.y) - Math.hypot(x - t.x, y - t.y)) < 0.05;
      if (!eq(ax, ay) || !eq(bx, by)) continue;
      // Only the part of the line that crosses the land mass's own box is walked: off a thin island most of it is at sea.
      let t0 = 0, t1 = 1;
      const dx = bx - ax, dy = by - ay;
      for (const [p, q2] of [[-dx, ax - x0], [dx, x9 - ax], [-dy, ay - y0], [dy, y9 - ay]]) {
        if (p === 0) { if (q2 < 0) t0 = 2; continue; }
        const r = q2 / p;
        if (p < 0) t0 = Math.max(t0, r); else t1 = Math.min(t1, r);
      }
      if (t0 >= t1) continue;
      [ax, ay, bx, by] = [ax + dx * t0, ay + dy * t0, ax + dx * t1, ay + dy * t1];
      const len = Math.hypot(bx - ax, by - ay), n = Math.max(12, Math.ceil(len / 3));
      for (let k = 0; k <= n; k++) if (inPoly(land[s.mass], ax + ((bx - ax) * k) / n, ay + ((by - ay) * k) / n)) on += len / (n + 1);
    }
    if (on >= BORDER) { regions[s.id - first].nb.push(t.id); regions[t.id - first].nb.push(s.id); }
  }
});
const byName = new Map(regions.map((r) => [r.name, r]));
const links = [];
const pair = (a, b, what) => {
  const ra = byName.get(a), rb = byName.get(b);
  if (!ra || !rb) { console.warn(`bad ${what}`, a, b); return null; }
  return [ra, rb];
};
for (const [a, b] of WORLD_LINKS) {
  const p = pair(a, b, 'link');
  if (!p) continue;
  if (!p[0].nb.includes(p[1].id)) { p[0].nb.push(p[1].id); p[1].nb.push(p[0].id); }
  links.push([p[0].id, p[1].id]);
}
for (const [a, b] of WORLD_ROADS) {
  const p = pair(a, b, 'road');
  if (p && !p[0].nb.includes(p[1].id)) { p[0].nb.push(p[1].id); p[1].nb.push(p[0].id); }
}
for (const [a, b] of WORLD_NO_BORDER) {
  const p = pair(a, b, 'border');
  if (!p) continue;
  p[0].nb = p[0].nb.filter((i) => i !== p[1].id);
  p[1].nb = p[1].nb.filter((i) => i !== p[0].id);
}
const alone = regions.filter((r) => !r.nb.length).map((r) => r.name);

// ---- Seas ----------------------------------------------------------------------------------------------------

// The seas of Europe, worked out again from the map as it was written, cell for cell.
const coreLand = core.land.map((p) => p.map((r) => [...r, r[0]]));
const cs = buildSeas({ land: coreLand, regions: core.regions, proj, width: coreW, height: coreH });
core.regions.forEach((r) => { if ((r.seas ?? []).join() !== cs.seaOf[r.id].join()) console.warn(`! the coast of ${r.name} does not come out as it stands on the map of Europe`); });

const cols = Math.ceil((x1 - gx0) / CELL), rows = Math.ceil(y1 / CELL);
const off = -gx0 / CELL;
const shift = (poly) => poly.map((ring) => ring.map(([x, y]) => [x - gx0, y]));
const isLand = new Uint8Array(cols * rows);
for (const p of coreLand) fill(isLand, cols, rows, shift(p), 1);
for (const p of land) fill(isLand, cols, rows, shift(p), 1);
const regionAt = new Int16Array(cols * rows).fill(-1);
for (const r of regions) for (const p of r.polys) fill(regionAt, cols, rows, shift(p.map((ring) => [...ring, ring[0]])), r.id);

const nCore = core.seas.zones.length;
const zones = core.seas.zones.map((z, i) => ({ ...z, i, adj: [...z.adj], coast: [...z.coast] }));
seaData.zones.forEach((z, k) => {
  const at = z.label ?? z.at;
  const [x, y] = proj([at[1], at[0]]);
  zones.push({ i: nCore + k, id: z.id, name: z.name, x: r1(x), y: r1(y), air: z.air ?? 1, ...(z.narrow ? { narrow: true } : {}), adj: [], coast: [] });
});
const index = new Map(zones.map((z) => [z.id, z.i]));

const zoneAt = new Int16Array(cols * rows).fill(-1);
const dist = new Float64Array(cols * rows).fill(1e9);
const frozen = new Uint8Array(cols * rows);
const heap = [];
const push = (d, k) => {
  heap.push([d, k]);
  for (let i = heap.length - 1; i > 0;) { const p = (i - 1) >> 1; if (heap[p][0] <= heap[i][0]) break; [heap[p], heap[i]] = [heap[i], heap[p]]; i = p; }
};
const pop = () => {
  const top = heap[0], last = heap.pop();
  if (heap.length) {
    heap[0] = last;
    for (let i = 0; ;) {
      const l = 2 * i + 1, r = l + 1; let m = i;
      if (l < heap.length && heap[l][0] < heap[m][0]) m = l;
      if (r < heap.length && heap[r][0] < heap[m][0]) m = r;
      if (m === i) break;
      [heap[m], heap[i]] = [heap[i], heap[m]]; i = m;
    }
  }
  return top;
};
// Europe's waters stay with the seas they belong to, and those seas go on spreading from where they stand.
for (let r = 0; r < cs.rows && r < rows; r++) {
  for (let c = 0; c < cs.cols; c++) {
    const z = cs.zoneAt[r * cs.cols + c];
    if (z < 0 || c + off >= cols) continue;
    const j = r * cols + c + off;
    zoneAt[j] = z; dist[j] = cs.dist[r * cs.cols + c]; frozen[j] = 1; push(dist[j], j);
  }
}
const anchor = (lat, lon, zone, what) => {
  const [x, y] = proj([lon, lat]);
  const c = Math.floor((x - gx0) / CELL), r = Math.floor(y / CELL);
  if (c < 0 || r < 0 || c >= cols || r >= rows || isLand[r * cols + c]) { console.warn(`! sea anchor on land or off the chart: ${what} (${lat}, ${lon})`); return; }
  if (frozen[r * cols + c]) { console.warn(`! sea anchor inside the seas of Europe: ${what} (${lat}, ${lon})`); return; }
  dist[r * cols + c] = 0; zoneAt[r * cols + c] = zone; push(0, r * cols + c);
};
seaData.zones.forEach((z, k) => { anchor(z.at[0], z.at[1], nCore + k, z.name); for (const w of z.waters ?? []) anchor(w[0], w[1], nCore + k, z.name); });
const STEPS = [[1, 0, 1], [-1, 0, 1], [0, 1, 1], [0, -1, 1], [1, 1, Math.SQRT2], [1, -1, Math.SQRT2], [-1, 1, Math.SQRT2], [-1, -1, Math.SQRT2]];
while (heap.length) {
  const [d, k] = pop();
  if (d > dist[k]) continue;
  const c = k % cols, r = (k - c) / cols;
  for (const [dc, dr, w] of STEPS) {
    const cc = c + dc, rr = r + dr;
    // The outermost ring of cells is the edge of the chart, not water.
    if (cc < 1 || rr < 1 || cc >= cols - 1 || rr >= rows - 1) continue;
    const j = rr * cols + cc;
    if (isLand[j] || frozen[j] || d + w >= dist[j]) continue;
    if (dc && dr && (isLand[r * cols + cc] || isLand[rr * cols + c])) continue;
    dist[j] = d + w; zoneAt[j] = zoneAt[k]; push(d + w, j);
  }
}

// The coasts of the new provinces. Europe's keep the coasts they have.
const touch = new Map();
for (let r = 0; r < rows; r++) {
  for (let c = 0; c < cols; c++) {
    const zone = zoneAt[r * cols + c];
    if (zone < 0) continue;
    for (const [dc, dr] of STEPS) {
      const cc = c + dc, rr = r + dr;
      if (cc < 0 || rr < 0 || cc >= cols || rr >= rows) continue;
      const id = regionAt[rr * cols + cc];
      if (id < 0) continue;
      let m = touch.get(id);
      if (!m) touch.set(id, (m = new Map()));
      m.set(zone, (m.get(zone) ?? 0) + 1);
    }
  }
}
for (const r of regions) r.seas = [];
for (const [id, m] of touch) {
  const r = regions[id - first];
  const most = Math.max(...m.values());
  for (const [zone, n] of [...m].sort((a, b) => b[1] - a[1])) if (n >= 8 && n >= most * 0.2) { r.seas.push(zone); zones[zone].coast.push(id); }
  if (!r.seas.length && (r.flags.port || r.flags.depot)) { const zone = [...m].sort((a, b) => b[1] - a[1])[0][0]; r.seas.push(zone); zones[zone].coast.push(id); }
}
for (const r of regions) if (r.flags.port && !r.seas.length) console.warn(`! ${r.name} is a port with no sea`);
for (const z of zones) z.coast.sort((a, b) => a - b);

for (const [a, b] of seaData.adjacent) {
  const ia = index.get(a), ib = index.get(b);
  if (ia === undefined || ib === undefined) { console.warn('bad sea link', a, b); continue; }
  if (!zones[ia].adj.includes(ib)) { zones[ia].adj.push(ib); zones[ib].adj.push(ia); }
}
const gates = [...core.seas.gates, ...seaData.gates.map((gt) => ({ a: index.get(gt.between[0]), b: index.get(gt.between[1]), name: gt.name, region: gt.region ?? null, nation: gt.nation ?? null, closed: !!gt.closed }))];

// The lines between the seas, traced along the grid where two meet and then smoothed.
const segs = new Map();
const V = (c, r) => r * (cols + 1) + c;
const add = (za, zb, v0, v1) => {
  const key = za < zb ? `${za}-${zb}` : `${zb}-${za}`;
  let list = segs.get(key);
  if (!list) segs.set(key, (list = []));
  list.push([v0, v1]);
};
for (let r = 0; r < rows; r++) {
  for (let c = 0; c < cols; c++) {
    const z = zoneAt[r * cols + c];
    if (z < 0) continue;
    if (c + 1 < cols) { const o = zoneAt[r * cols + c + 1]; if (o >= 0 && o !== z) add(z, o, V(c + 1, r), V(c + 1, r + 1)); }
    if (r + 1 < rows) { const o = zoneAt[(r + 1) * cols + c]; if (o >= 0 && o !== z) add(z, o, V(c, r + 1), V(c + 1, r + 1)); }
  }
}
const borders = [];
for (const list of segs.values()) {
  const at = new Map();
  list.forEach((sg, i) => { for (const v of sg) { let l = at.get(v); if (!l) at.set(v, (l = [])); l.push(i); } });
  const used = new Uint8Array(list.length);
  const walk = (start) => {
    const line = [start];
    let v = start;
    for (;;) {
      const next = (at.get(v) ?? []).find((i) => !used[i]);
      if (next === undefined) break;
      used[next] = 1;
      v = list[next][0] === v ? list[next][1] : list[next][0];
      line.push(v);
    }
    return line;
  };
  const lines = [];
  for (const [v, l] of at) if (l.length === 1) { const line = walk(v); if (line.length > 1) lines.push(line); }
  for (const [v] of at) { const line = walk(v); if (line.length > 1) lines.push(line); }
  for (const line of lines) {
    if (line.length < 6) continue;
    const pts = line.map((v) => [(v % (cols + 1)) * CELL + gx0, Math.floor(v / (cols + 1)) * CELL]);
    borders.push(smooth(simplify(pts, 5)).map(([x, y]) => [r1(x), r1(y)]));
  }
}
// Seas that meet on the chart but are not joined in the data, and the other way about.
const meets = new Set([...segs].filter(([, l]) => l.length >= 12).map(([k]) => k));
const told = new Set();
for (const z of zones) for (const j of z.adj) told.add(z.i < j ? `${z.i}-${j}` : `${j}-${z.i}`);
const nm = (k) => k.split('-').map((i) => zones[+i].id).join(' and ');
for (const k of meets) if (!told.has(k) && k.split('-').some((i) => +i >= nCore)) console.warn(`? ${nm(k)} meet on the chart (${segs.get(k).length} cells) but are not adjacent`);
for (const k of told) if (!meets.has(k) && k.split('-').some((i) => +i >= nCore)) console.warn(`? ${nm(k)} are adjacent but do not meet on the chart`);

// Which sea a point of the chart lies in, at half the detail: each row is [sea, cells, sea, cells...], -1 for land.
const LOOK = 2;
const grid = [];
for (let r = 0; r < rows; r += LOOK) {
  const row = [];
  let last = -2, run = 0;
  for (let c = 0; c < cols; c += LOOK) {
    const z = zoneAt[r * cols + c];
    if (z === last) { run++; continue; }
    if (run) row.push(last, run);
    last = z; run = 1;
  }
  row.push(last, run);
  grid.push(row);
}

const out = {
  // The chart's edges in map units. Europe's own corner stays at 0, 0, so the new world lies at negative x.
  frame: { x0: gx0, y0: 0, x1: Math.round(x1), y1: Math.round(y1) },
  first,
  land: land.map((p) => p.map((r) => r.slice(0, -1).map(([x, y]) => [r1(x), r1(y)]))),
  lakes: lakes.map((l) => {
    const ring = l.poly[0].slice(0, -1);
    return { name: l.name, x: r1(ring.reduce((s, p) => s + p[0], 0) / ring.length), y: r1(ring.reduce((s, p) => s + p[1], 0) / ring.length) };
  }),
  regions, links,
  seas: { zones: zones.map(({ i, ...z }) => z), gates, borders, cell: CELL * LOOK, cols: Math.ceil(cols / LOOK), grid },
};
fs.writeFileSync(new URL('../src/generated/world-map.json', import.meta.url), JSON.stringify(out));
const pts = regions.reduce((s, r) => s + r.polys.reduce((t, p) => t + p.reduce((u, q) => u + q.length, 0), 0), 0);
const landPts = out.land.reduce((s, p) => s + p.reduce((t, r) => t + r.length, 0), 0);
console.log(`world map: ${regions.length} new provinces (${first} to ${first + regions.length - 1}), ${pts} points, ${landPts} of coast, ${zones.length - nCore} new seas, ${out.frame.x0} to ${out.frame.x1} by ${out.frame.y1}, ${(JSON.stringify(out).length / 1024).toFixed(0)} KB`);
if (joined.length) console.log(`islands joined to: ${[...new Set(joined)].join(', ')}`);
if (alone.length) console.log(`no neighbours: ${alone.join(', ')}`);
