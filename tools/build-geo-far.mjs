// Lays the far side of the world round today's chart: Asia and the Pacific first, then whatever of the globe has no
// province yet. Run after build-geo-world.mjs, which it runs again itself to read the chart as it stands.
//
// Nothing on today's chart moves. Its provinces (375 to 575) are copied as they are written; every square of water
// that holds a sea today holds the same sea, and those seas go on spreading from where they stand until they meet
// the new ones. The new provinces follow in number (576 up) and the new seas follow the old (67 up), in the order
// of data/campaign/far.mjs and far-seas.json, so a province or a sea keeps its number for good.
//
// The chart is now the full circle: x runs eastward from today's west edge (141 W) for 9256.14 units and comes back
// to it. All the new land lies east of today's east edge (47 E): Asia, the Pacific, and at the far end Alaska, which
// meets the Yukon across the edge. No shape is cut at the edge: one that crosses it runs on past. Water is counted
// round the circle, so the seas west of America and the seas east of Asia meet.
//
// It writes src/generated/world-next.json, in the form of world-map.json and holding all of it. world-map.json
// itself is left as it is: the game reads that one until the rules are ready for the rest.
import fs from 'node:fs';
import { Delaunay } from 'd3-delaunay';
import polygonClipping from 'polygon-clipping';
import { feature } from 'topojson-client';
import { loadLand, clipToBox, simplifyRing, ringArea } from './geo-common.mjs';
import { CELL, fill, simplify, smooth } from './geo-seas.mjs';
import { CHART } from './build-geo-world.mjs';
import { FAR_SLICES, FAR_LAKES, ISLET_SIZE } from '../data/campaign/far.mjs';
import { carveCities } from './carve-cities.mjs';

const core = JSON.parse(fs.readFileSync(new URL('../src/generated/campaign-map.json', import.meta.url), 'utf8'));
const seaData = JSON.parse(fs.readFileSync(new URL('../data/campaign/far-seas.json', import.meta.url), 'utf8'));
const today = CHART.out;
const quiet = process.argv.includes('--quiet');
const arg = (name) => process.argv.find((a) => a.startsWith(`--${name}=`))?.slice(name.length + 3);

// The projection of the map of Europe, carried round the world. Longitude is counted eastward from today's west
// edge, so 141 W is also 219 E and x never jumps.
const NORTH = 71.4, SOUTH = -56, EDGE = -141;
const SCALE = 40, K = Math.cos((50 * Math.PI) / 180) * SCALE;
const W = 360 * K;
const east = (lon) => (lon < EDGE ? lon + 360 : lon);
const proj = ([lon, lat]) => [(east(lon) + 11) * K, (NORTH - lat) * SCALE];
const r1 = (v) => Math.round(v * 10) / 10;
const { gx0 } = CHART;
// The grid of water cells is today's, carried on round the circle. (The last 1.1 units before the edge fall
// between the last column and the first.)
const COLS = Math.floor(W / CELL), ROWS = CHART.rows;
const y1 = (NORTH - SOUTH) * SCALE;
// Today's east and west edges as its coasts are written, the west one a full circle on.
const X47 = 1491.3, X141 = r1(-3342.5 + W);

function inRing(ring, x, y) {
  let inside = false;
  for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
    const [xi, yi] = ring[i], [xj, yj] = ring[j];
    if (yi > y !== yj > y && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) inside = !inside;
  }
  return inside;
}
const inPoly = (poly, x, y) => inRing(poly[0], x, y) && !poly.slice(1).some((h) => inRing(h, x, y));
const boxOf = (ring) => { let a = 1e9, b = 1e9, c = -1e9, d = -1e9; for (const [x, y] of ring) { if (x < a) a = x; if (y < b) b = y; if (x > c) c = x; if (y > d) d = y; } return [a, b, c, d]; };
const boxesMeet = (p, q) => p[0] <= q[2] && q[0] <= p[2] && p[1] <= q[3] && q[1] <= p[3];
const closed = (poly) => poly.map((ring) => [...ring, ring[0]]);
const moved = (poly, dx) => poly.map((ring) => ring.map(([x, y]) => [x + dx, y]));
const areaOf = (multi) => multi.reduce((t, p) => t + Math.abs(ringArea(p[0])) - p.slice(1).reduce((u, h) => u + Math.abs(ringArea(h)), 0), 0);
/** Marks the cells of a shape on the round grid: a shape that runs past either edge of the chart comes back in at the other. */
function fillRound(grid, poly, value) {
  const [a, , c] = boxOf(poly[0]);
  fill(grid, COLS, ROWS, moved(poly, -gx0), value);
  if (c > gx0 + COLS * CELL) fill(grid, COLS, ROWS, moved(poly, -gx0 - W), value);
  if (a < gx0) fill(grid, COLS, ROWS, moved(poly, -gx0 + W), value);
}
const warnings = [];
const warn = (text) => { warnings.push(text); console.warn(text); };

// ---- Land ----------------------------------------------------------------------------------------------------

// The lakes cut out of the new land by hand, as the Great Lakes are (world.mjs).
const round = (ring) => ring.flatMap(([ax, ay], i) => { const [bx, by] = ring[(i + 1) % ring.length]; return [[ax * 0.75 + bx * 0.25, ay * 0.75 + by * 0.25], [ax * 0.25 + bx * 0.75, ay * 0.25 + by * 0.75]]; });
const lakes = FAR_LAKES.map(([name, level, pts]) => {
  const ring = round(round(pts.map(([lat, lon]) => proj([lon, lat]))));
  return { name, level, poly: [[...ring, ring[0]]] };
});

// Everything between today's east edge and its west edge, the long way round: the atlas's land as it lies, and
// again a full circle on, so that Alaska and Hawaii follow Asia eastward.
let land = [];
{
  const raw = loadLand('50m');
  const twice = [...raw, ...raw.map((p) => p.map((ring) => ring.map(([x, y]) => [x + 360, y])))];
  const snap = ([x, y]) => [Math.abs(x - 58 * K) < 0.2 ? X47 : Math.abs(x - 230 * K) < 0.2 ? X141 : x, y];
  for (const poly of clipToBox(twice, [47, SOUTH, 219, NORTH])) {
    const rings = [];
    poly.forEach((ring, i) => {
      const r = simplifyRing(ring.map(([lon, lat]) => snap([(lon + 11) * K, (NORTH - lat) * SCALE])), 0.4);
      if (!r || Math.abs(ringArea(r)) < (i === 0 ? 3 : 6)) return;
      rings.push(r);
    });
    if (!rings.length || Math.abs(ringArea(rings[0])) < 3) continue;
    let parts;
    try { parts = lakes.length ? polygonClipping.difference(rings, ...lakes.map((l) => l.poly)) : [rings]; } catch (e) { warn(`! could not cut a land mass: ${e.message}`); continue; }
    for (const p of parts) if (p.length && Math.abs(ringArea(p[0])) >= 3) land.push(p);
  }
}

// The clerk's tables (data/campaign/tables/*.csv; --tables=folder copies them in fresh from the map tables folder):
// one row to a province, by name. Once a province has a row, the row is the source for its owner, ground, victory
// points, factories and the flags port, col, cap, depot, oil and fort. The seed's own values stand until then, and
// 'islet' is always the seed's: how large an island is drawn is the map's business.
// cities-parents.csv is read apart: its rows say what a province is worth once cities have been cut out of it, on
// the chart with the cities only. The chart as handed over never reads it.
// The chart as handed over (world-next.json) stands byte for byte, so the tables it was built from stand too
// (islands-draft.csv, pacific.csv, globe.csv here). What the clerk has changed in them since is kept beside them, in
// tables/now/, and is laid over the chart with the cities only. --tables=folder copies the cities' two tables in
// fresh, and the three older ones into tables/now/.
const tables = new Map();
const parentRows = new Map();
const nowRows = new Map();
{
  const dir = new URL('../data/campaign/tables/', import.meta.url);
  const now = new URL('now/', dir);
  if (arg('tables')) {
    fs.mkdirSync(now, { recursive: true });
    for (const f of ['cities.csv', 'cities-parents.csv']) { const from = `${arg('tables')}/${f}`; if (fs.existsSync(from)) fs.copyFileSync(from, new URL(f, dir)); }
    for (const f of ['islands-draft.csv', 'pacific.csv', 'globe.csv']) { const from = `${arg('tables')}/${f}`; if (fs.existsSync(from)) fs.copyFileSync(from, new URL(f, now)); }
  }
  const parse = (text) => {
    const rows = [[]];
    let cell = '', quoted = false;
    for (let i = 0; i < text.length; i++) {
      const ch = text[i];
      if (quoted) { if (ch === '"') { if (text[i + 1] === '"') { cell += '"'; i++; } else quoted = false; } else cell += ch; }
      else if (ch === '"') quoted = true;
      else if (ch === ',') { rows[rows.length - 1].push(cell); cell = ''; }
      else if (ch === '\n') { rows[rows.length - 1].push(cell); cell = ''; rows.push([]); }
      else if (ch !== '\r') cell += ch;
    }
    if (cell || rows[rows.length - 1].length) rows[rows.length - 1].push(cell);
    return rows.filter((r) => r.length > 1);
  };
  if (fs.existsSync(dir)) for (const f of fs.readdirSync(dir).filter((x) => x.endsWith('.csv')).sort()) {
    const rows = parse(fs.readFileSync(new URL(f, dir), 'utf8'));
    const head = rows.shift().map((h) => h.trim());
    for (const row of rows) {
      const r = Object.fromEntries(head.map((h, i) => [h, (row[i] ?? '').trim()]));
      const into = f === 'cities-parents.csv' ? parentRows : tables;
      if (into.has(r.name)) warn(`! the tables have two rows for ${r.name}`);
      into.set(r.name, r);
    }
  }
  // Only a row that reads differently now is kept: the columns the chart reads, no others.
  const READ = ['owner', 'ground', 'vp', 'factories', 'feeds_troops', 'colony', 'capital', 'depot', 'oil', 'works', 'works_face'];
  if (fs.existsSync(now)) for (const f of fs.readdirSync(now).filter((x) => x.endsWith('.csv')).sort()) {
    const rows = parse(fs.readFileSync(new URL(f, now), 'utf8'));
    const head = rows.shift().map((h) => h.trim());
    for (const row of rows) {
      const r = Object.fromEntries(head.map((h, i) => [h, (row[i] ?? '').trim()]));
      const was = tables.get(r.name);
      if (!was) warn(`! ${r.name} has a row in tables/now/${f} and none in the tables the chart was built from`);
      else if (READ.some((k) => (was[k] ?? '') !== (r[k] ?? ''))) nowRows.set(r.name, r);
    }
  }
}
const GROUND = new Set(['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert']);
let tabled = 0;
function fromTable(name, owner, terrain, vp, fac, flags, rows = tables) {
  const r = rows.get(name);
  if (!r) return { owner, terrain, vp, fac, flags };
  if (rows === tables) tabled++;
  const f = { ...flags };
  const set = (key, on) => { if (on) f[key] = 1; else delete f[key]; };
  if (r.feeds_troops !== undefined && r.feeds_troops !== '') set('port', r.feeds_troops === '1');
  if (r.colony !== undefined && r.colony !== '') set('col', r.colony === '1');
  if (r.capital !== undefined && r.capital !== '') set('cap', r.capital === '1');
  if (r.depot !== undefined && r.depot !== '') set('depot', r.depot === '1');
  if (r.oil !== undefined && r.oil !== '') set('oil', Number(r.oil) > 0);
  if (r.works !== undefined && r.works !== '') { if (Number(r.works) > 0) f.fort = [Number(r.works), ...(r.works_face ?? '').split(/[;, ]+/).filter(Boolean)]; else delete f.fort; }
  if (r.ground && !GROUND.has(r.ground)) warn(`! the tables give ${name} ground the game does not have: ${r.ground}`);
  const num = (v, d) => (v !== undefined && v !== '' && Number.isFinite(Number(v)) ? Number(v) : d);
  return { owner: r.owner || owner, terrain: GROUND.has(r.ground) ? r.ground : terrain, vp: num(r.vp, vp), fac: num(r.factories, fac), flags: f };
}

// The seeds of every slice, in order: a province keeps its number for good.
const first = core.regions.length + today.regions.length;
const taken = new Set([...core.regions, ...today.regions].map((r) => r.name));
const seeds = [];
FAR_SLICES.forEach((slice, k) => {
  for (const [name, lat, lon, owner0, terrain0, vp0, fac0, flags0] of slice.regions) {
    if (taken.has(name)) warn(`! two provinces are called ${name}`);
    taken.add(name);
    const [x, y] = proj([lon, lat]);
    const { owner, terrain, vp, fac, flags } = fromTable(name, owner0, terrain0, vp0, fac0, flags0 ?? {});
    seeds.push({ id: first + seeds.length, name, owner, terrain, vp, fac, flags, lat, lon, x, y, slice: k });
  }
});
const nReal = seeds.length;
FAR_SLICES.forEach((slice, k) => {
  for (const [what, lat, lon] of slice.beyond ?? []) { const [x, y] = proj([lon, lat]); seeds.push({ id: -1, name: what, x, y, slice: k, flags: {} }); }
});

// An island too small to see or to click is drawn larger than life, centred on its true place: a round shape laid
// over whatever the atlas has there. Its province carries the flag 'islet'. 'disc' lays the same round shape under a
// seed whose province is more than that shape (Samoa: the naval station's small island, with the two large ones
// beside it) and is not carried into the chart.
for (const s of seeds) {
  if (!s.flags.islet && !s.flags.disc) continue;
  const [rx, ry] = ISLET_SIZE[s.name] ?? ISLET_SIZE._default;
  const ring = Array.from({ length: 20 }, (_, i) => [s.x + rx * Math.cos((i * Math.PI) / 10), s.y + ry * Math.sin((i * Math.PI) / 10)]);
  const disc = [[...ring, ring[0]]];
  const box = boxOf(disc[0]);
  const under = [];
  land.forEach((p, i) => {
    if (!boxesMeet(box, boxOf(p[0]))) return;
    let meet = [];
    try { meet = polygonClipping.intersection(p, disc); } catch { /* a shape too small to clip is left under the disc */ }
    if (meet.length) under.push(i);
  });
  let whole = [disc];
  try { whole = polygonClipping.union(disc, ...under.map((i) => land[i])); } catch (e) { warn(`! could not lay the islet ${s.name}: ${e.message}`); }
  if (under.some((i) => Math.abs(ringArea(land[i][0])) > 400)) warn(`! the islet ${s.name} is laid over a large island`);
  land = land.filter((_, i) => !under.includes(i));
  for (const p of whole) land.push(p);
}

// ---- Provinces -----------------------------------------------------------------------------------------------

// The ground a province can be grown on: the new land, and whatever land of today's chart no province stands on
// (Africa below the map of Europe, islands nobody claimed). Ground a province of today stands on is closed.
const oldPoints = new Set();
for (const r of today.regions) for (const p of r.polys) for (const ring of p) for (const [x, y] of ring) oldPoints.add(`${x},${y}`);
// (Each mass remembers one of the coasts it is made of, to tell later which ground a province stands on.)
const nCoreLand = core.land.length, nOldLand = CHART.landFull.length;
let masses = [];
CHART.landFull.forEach((p, i) => {
  if (p[0].some(([x, y]) => oldPoints.has(`${r1(x)},${r1(y)}`))) return;
  // The lakes of Africa lie in land of today's chart, whose coast stands as it is written: they are cut out of the
  // ground the provinces grow on, and listed with the other lakes.
  const box = boxOf(p[0]);
  const wet = lakes.filter((l) => boxesMeet(boxOf(l.poly[0]), box));
  let parts = [p];
  if (wet.length) { try { parts = polygonClipping.difference(p, ...wet.map((l) => l.poly)); } catch (e) { warn(`! could not cut a lake out of today's land: ${e.message}`); } }
  for (const q of parts) if (q.length && Math.abs(ringArea(q[0])) >= 3) masses.push({ poly: q, box: boxOf(q[0]), old: true, shape: nCoreLand + i });
});
land.forEach((p, i) => masses.push({ poly: p, box: boxOf(p[0]), old: false, shape: nCoreLand + nOldLand + i }));
// Today's east edge (47 E) runs through Arabia, Somalia and Madagascar, where no frontier does. South of the map of
// Europe the land of today's chart that no province stands on and the new land east of it are one ground: the two
// are made one mass, so a province lies across the old edge as if it were not there.
{
  const onLine = (x) => Math.abs(x - X47) < 0.11;
  const spans = masses.map((m) => {
    const out = [];
    // (A stretch of the edge that begins beside the map of Europe counts from the map's south edge down.)
    for (const ring of m.poly) for (let i = 0; i + 1 < ring.length; i++) { const [ax, ay] = ring[i], [bx, by] = ring[i + 1]; if (onLine(ax) && onLine(bx) && Math.max(ay, by) > 1712.3) out.push([Math.max(1712, Math.min(ay, by)), Math.max(ay, by)]); }
    return out;
  });
  const boss = masses.map((_, i) => i);
  const find = (i) => { while (boss[i] !== i) { boss[i] = boss[boss[i]]; i = boss[i]; } return i; };
  masses.forEach((a, i) => {
    if (!a.old || !spans[i].length) return;
    masses.forEach((b, j) => {
      if (b.old || !spans[j].length) return;
      if (spans[i].some(([p0, p1]) => spans[j].some(([q0, q1]) => Math.min(p1, q1) - Math.max(p0, q0) > 0.3))) boss[find(j)] = find(i);
    });
  });
  const groups = new Map();
  masses.forEach((_, i) => { const b = find(i); (groups.get(b) ?? groups.set(b, []).get(b)).push(i); });
  const gone = new Set(), made = [];
  for (const [b, list] of groups) {
    if (list.length < 2) continue;
    // The new land is let a hair over the old, so the two overlap instead of touching along a line.
    // Beside the map of Europe the new land keeps to the edge: a stretch of it that runs on south of the map is
    // broken at the map's corner, and only what lies south of the corner is let over.
    const letOver = (ring) => {
      const out = [];
      ring.forEach(([x, y], k) => {
        out.push([onLine(x) && y > 1712.05 ? X47 - 0.4 : x, y]);
        const next = ring[k + 1];
        if (next && onLine(x) && onLine(next[0]) && (y - 1712) * (next[1] - 1712) < 0 && Math.abs(y - 1712) > 0.05 && Math.abs(next[1] - 1712) > 0.05) out.push([X47, 1712]);
      });
      return out;
    };
    const polys = list.map((i) => (masses[i].old ? masses[i].poly : masses[i].poly.map(letOver)));
    let whole;
    try { whole = polygonClipping.union(...polys); } catch (e) { warn(`! could not join the land across today's east edge: ${e.message}`); continue; }
    if (whole.length !== 1) warn(`! land across today's east edge came out in ${whole.length} pieces`);
    for (const i of list) gone.add(i);
    for (const q of whole) made.push({ poly: q, box: boxOf(q[0]), old: false, shape: masses[b].shape, across: list.length });
  }
  if (made.length) masses = [...masses.filter((_, i) => !gone.has(i)), ...made];
}
const massAt = (x, y) => masses.findIndex((m) => x >= m.box[0] && x <= m.box[2] && y >= m.box[1] && y <= m.box[3] && inPoly(m.poly, x, y));

// The atlas's countries, their longitudes counted eastward like everything else here.
const atlas = (() => {
  const topo = JSON.parse(fs.readFileSync(new URL('../node_modules/world-atlas/countries-50m.json', import.meta.url)));
  return feature(topo, topo.objects.countries).features;
})();
function countryOf(name, within) {
  const f = atlas.find((x) => x.properties.name === name);
  if (!f) throw new Error(`the atlas has no country called ${name}`);
  const polys = f.geometry.type === 'Polygon' ? [f.geometry.coordinates] : f.geometry.coordinates;
  const out = [];
  for (const p of polys) {
    // A ring that crosses the 180th meridian is carried on past it, and a piece that lies west of the edge is
    // taken a full circle on.
    const rings = p.map((ring) => {
      let shift = 0;
      return ring.map(([x, y], i) => { if (i > 0) { const d = x - ring[i - 1][0]; if (d > 180) shift -= 360; else if (d < -180) shift += 360; } return [x + shift, y]; });
    });
    let mid = rings[0].reduce((s, q) => s + q[0], 0) / rings[0].length;
    const turn = mid < EDGE ? 360 : mid > EDGE + 360 ? -360 : 0;
    mid += turn;
    const midLat = rings[0].reduce((s, q) => s + q[1], 0) / rings[0].length;
    if (within && !(mid >= within[0] && mid <= within[2] && midLat >= within[1] && midLat <= within[3])) continue;
    out.push(rings.map((ring) => ring.map(([x, y]) => [(x + turn + 11) * K, (NORTH - y) * SCALE])));
  }
  return out;
}
// The frontiers drawn true, as on today's chart (build-geo-world.mjs): each side is a closed shape, a country's own
// outline from the atlas let out by a hair, or a line drawn by hand and closed through open water. 'clip' keeps
// only the part of a country's outline inside a shape drawn by hand: Manchuria out of China, the Far East out of
// Russia. A side listed earlier is cut out of one listed later.
const HAIR = 1.5;
const letOut = (multi) => {
  const k = HAIR * Math.SQRT1_2;
  const moves = [[HAIR, 0], [-HAIR, 0], [0, HAIR], [0, -HAIR], [k, k], [k, -k], [-k, k], [-k, -k]];
  return polygonClipping.union(multi, ...moves.map(([dx, dy]) => multi.map((p) => moved(p, dx).map((r) => r.map(([x, y]) => [x, y + dy])))));
};
const ringOf = (pts) => { const ring = pts.map(([lat, lon]) => proj([lon, lat])); return [[...ring, ring[0]]]; };
const sides = [];
FAR_SLICES.forEach((slice, k) => {
  for (const f of slice.frontiers ?? []) {
    let multi;
    if (f.country) {
      multi = letOut([].concat(f.country).flatMap((name) => countryOf(name, f.within ?? null)));
      if (f.clip) multi = polygonClipping.intersection(multi, ringOf(f.clip));
    } else multi = [ringOf([...f.line, ...(f.close ?? [])])];
    sides.push({ name: f.name ?? [].concat(f.country).join(', '), owners: f.owners, atlas: !!f.country, multi, boxes: multi.map((p) => boxOf(p[0])), slice: k });
  }
});
const sideAt = (x, y) => sides.findIndex((sd) => sd.multi.some((p, i) => x >= sd.boxes[i][0] && x <= sd.boxes[i][2] && y >= sd.boxes[i][1] && y <= sd.boxes[i][3] && inPoly(p, x, y)));
for (const s of seeds) {
  s.mass = massAt(s.x, s.y);
  s.side = sideAt(s.x, s.y);
  if (s.mass < 0) warn(`! ${s.id < 0 ? 'the seed for ' : ''}${s.name} is not on land`);
  if (s.side >= 0 && sides[s.side].slice !== s.slice) warn(`! ${s.name} stands inside a frontier of another slice (${sides[s.side].name})`);
  if (s.id >= 0 && s.side >= 0 && !sides[s.side].owners.includes(s.owner)) warn(`! ${s.name} (${s.owner}) stands on the wrong side of a frontier (${sides[s.side].name})`);
}

// Each land mass is shared out among the seeds that stand on it. A mass a frontier runs through is shared out side
// by side: no province takes ground across the line. Each side, and each mass no frontier runs through, belongs
// to one slice, so a later slice never moves an earlier one's provinces.
const onMass = new Map();
for (const s of seeds) if (s.mass >= 0) (onMass.get(s.mass) ?? onMass.set(s.mass, []).get(s.mass)).push(s);
const sideCut = new Map();
const cut = (m, k) => {
  const key = `${m}:${k}`;
  if (sideCut.has(key)) return sideCut.get(key);
  const mass = masses[m];
  const near = (sd) => sd.boxes.some((b) => boxesMeet(b, mass.box));
  let part;
  try {
    if (k < 0) part = polygonClipping.difference(mass.poly, ...sides.filter(near).map((sd) => sd.multi));
    else {
      part = polygonClipping.intersection(mass.poly, sides[k].multi);
      const before = sides.slice(0, k).filter((sd) => near(sd) && sd.boxes.some((b) => sides[k].boxes.some((c) => boxesMeet(b, c))));
      if (before.length && part.length) part = polygonClipping.difference(part, ...before.map((sd) => sd.multi));
    }
  } catch (e) { warn(`! could not cut a land mass along a frontier (${k < 0 ? 'open side' : sides[k].name}): ${e.message}`); part = [mass.poly]; }
  sideCut.set(key, part);
  return part;
};
for (const [m, all] of onMass) {
  const framed = all.some((s) => s.side >= 0);
  const groups = new Map();
  for (const s of all) { const k = framed ? s.side : 'all'; (groups.get(k) ?? groups.set(k, []).get(k)).push(s); }
  for (const [k, group] of groups) {
    if (new Set(group.map((s) => s.slice)).size > 1) warn(`! provinces of two slices share one stretch of ground: ${group.map((s) => s.name).join(', ')}`);
    if (framed && k < 0) warn(`! ${group.map((s) => s.name).join(', ')}: on a land mass that frontiers run through, but inside none of them`);
    const part = framed ? cut(m, k) : [masses[m].poly];
    for (const s of group) s.part = part;
    if (group.length < 2) { group[0].cell = null; continue; }
    const voronoi = Delaunay.from(group, (s) => s.x, (s) => s.y).voronoi([gx0 - 10, -10, gx0 + W + 20, y1 + 10]);
    group.forEach((s, i) => { s.cell = voronoi.cellPolygon(i); });
  }
}

/**
 * An outline without the hairs the joining of two grounds can leave: a point twice running, or out and straight
 * back. The first slice (the Pacific) was handed over before this was done, and its outlines stand as they were.
 */
function trim(ring, slice) {
  if (slice === 0) return ring;
  const out = ring.slice();
  for (let changed = true; changed && out.length > 3; ) {
    changed = false;
    for (let i = 0; i < out.length && out.length > 3; i++) {
      const a = out[(i + out.length - 1) % out.length], b = out[i], c = out[(i + 1) % out.length];
      if ((b[0] === c[0] && b[1] === c[1]) || (a[0] === c[0] && a[1] === c[1])) { out.splice(i, 1); changed = true; i--; }
    }
  }
  return out;
}

const regions = [];
for (const s of seeds) {
  if (s.id < 0) continue;
  let polys = [];
  if (s.mass >= 0) { try { polys = s.cell ? polygonClipping.intersection([s.cell], s.part) : s.part; } catch (e) { warn(`! clip failed for ${s.name}`); } }
  polys = polys
    .map((p) => p.filter((r) => Math.abs(ringArea(r)) > 1.5).map((r) => trim(r.slice(0, -1).map(([x, y]) => [r1(x), r1(y)]), s.slice)))
    .filter((p) => p.length && p[0].length >= 3);
  if (!polys.length) warn(`! ${s.name} has no land`);
  const flags = { ...s.flags };
  if (flags.islet) flags.islet = 1;
  delete flags.disc;
  regions.push({ id: s.id, name: s.name, owner: s.owner, terrain: s.terrain, vp: s.vp, fac: s.fac, flags, x: r1(s.x), y: r1(s.y), polys, nb: [] });
}
const regionOfId = (id) => regions[id - first];
const sliceOfId = (id) => seeds[id - first].slice;

// A province's share can reach across a gulf or a bay to ground that touches only its neighbours. Such a piece goes
// to the province of its own slice, on its own side of the frontiers, that it has the longest border with.
const pieces = [];
{
  const at = new Int16Array(COLS * ROWS).fill(-1);
  const size = (p) => Math.abs(ringArea(closed(p)[0]));
  const main = regions.map((r) => {
    let k = r.polys.findIndex((p) => inPoly(p, r.x, r.y));
    if (k < 0) r.polys.forEach((p, i) => { if (k < 0 || size(p) > size(r.polys[k])) k = i; });
    return k;
  });
  regions.forEach((r, i) => { if (main[i] >= 0) fillRound(at, closed(r.polys[main[i]]), r.id); });
  regions.forEach((r, i) => {
    const keep = [];
    r.polys.forEach((p, k) => {
      if (k === main[i]) { keep.push(p); return; }
      const mine = new Uint8Array(COLS * ROWS);
      fillRound(mine, closed(p), 1);
      const [a, b, c, d] = boxOf(p[0]);
      const touch = new Map();
      for (let row = Math.max(1, Math.floor(b / CELL) - 1); row <= Math.min(ROWS - 2, Math.ceil(d / CELL) + 1); row++) {
        for (let col = Math.floor((a - gx0) / CELL) - 1; col <= Math.ceil((c - gx0) / CELL) + 1; col++) {
          const cc = ((col % COLS) + COLS) % COLS, j = row * COLS + cc;
          if (!mine[j]) continue;
          for (const [dc, dr] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) { const n = (row + dr) * COLS + (cc + dc + COLS) % COLS, o = at[n]; if (o >= 0 && !mine[n]) touch.set(o, (touch.get(o) ?? 0) + 1); }
        }
      }
      const s = seeds[r.id - first];
      const best = [...touch].filter(([o]) => o !== r.id && seeds[o - first].side === s.side && seeds[o - first].slice === s.slice).sort((p2, q2) => q2[1] - p2[1] || p2[0] - q2[0])[0];
      if (touch.has(r.id) || !best || best[1] < 2) { keep.push(p); return; }
      regionOfId(best[0]).polys.push(p);
      pieces.push(`${Math.round(size(p))} of ${r.name} to ${regionOfId(best[0]).name}`);
    });
    r.polys = keep;
  });
}

// An island with no seed of its own goes with the nearest province, if it lies just off its shore; one inside a
// country's own outline goes to that country's nearest province though it lies further out. The slices take their
// islands in turn, so a later slice never takes one from an earlier.
const joined = [];
{
  const seeded = new Set(seeds.map((s) => s.mass));
  FAR_SLICES.forEach((slice, k) => {
    const mine = seeds.filter((s) => s.id >= 0 && s.slice === k);
    masses.forEach((mass, m) => {
      const p = mass.poly;
      if (seeded.has(m) || Math.abs(ringArea(p[0])) > 4000) return;
      // An island inside a country's outline goes to a province of that country only, and one outside every
      // outline to a province outside them: Bintan is not Singapore's, nor Tsushima Korea's.
      // (The middle of a long bent island can fall in the water, so its shore is asked too.)
      const n0 = p[0].length - 1;
      let home = sideAt(p[0].slice(0, n0).reduce((t, q) => t + q[0], 0) / n0, p[0].slice(0, n0).reduce((t, q) => t + q[1], 0) / n0);
      if (home < 0) {
        const votes = new Map();
        for (let i = 0; i < n0; i += Math.max(1, Math.floor(n0 / 16))) { const sd = sideAt(p[0][i][0], p[0][i][1]); votes.set(sd, (votes.get(sd) ?? 0) + 1); }
        home = [...votes].sort((a, b) => b[1] - a[1])[0][0];
      }
      let best = null, bd = slice.reach ?? 70;
      for (const s of mine) {
        if (s.side !== home) continue;
        if (s.x < mass.box[0] - bd || s.x > mass.box[2] + bd || s.y < mass.box[1] - bd || s.y > mass.box[3] + bd) continue;
        for (const [x, y] of p[0]) { const d = Math.hypot(x - s.x, y - s.y); if (d < bd) { bd = d; best = s; } }
      }
      if (!best) {
        const n = p[0].length - 1;
        const cx = p[0].slice(0, n).reduce((t, q) => t + q[0], 0) / n, cy = p[0].slice(0, n).reduce((t, q) => t + q[1], 0) / n;
        const sd = home;
        if (sd >= 0 && sides[sd].atlas && sides[sd].slice === k) {
          let far = 200;
          for (const s of mine) { if (s.side !== sd) continue; const d = Math.hypot(cx - s.x, cy - s.y); if (d < far) { far = d; best = s; } }
        }
      }
      if (!best) return;
      const rings = p.filter((r) => Math.abs(ringArea(r)) > 1.5).map((r) => trim(r.slice(0, -1).map(([x, y]) => [r1(x), r1(y)]), k));
      if (!rings.length || rings[0].length < 3) return;
      regionOfId(best.id).polys.push(rings);
      seeded.add(m);
      mass.joined = best.id;
      joined.push(best.name);
    });
  });
}

// ---- The grid ------------------------------------------------------------------------------------------------

// Today's grid, set into the round one cell for cell.
const isLand = new Uint8Array(COLS * ROWS);
const zoneAt = new Int16Array(COLS * ROWS).fill(-1);
const dist = new Float64Array(COLS * ROWS).fill(1e9);
const frozen = new Uint8Array(COLS * ROWS);
const regionAt = new Int16Array(COLS * ROWS).fill(-1);
for (const r of core.regions) for (const p of r.polys) fillRound(regionAt, closed(p), r.id);
for (let r = 0; r < ROWS; r++) {
  for (let c = 0; c < CHART.cols; c++) {
    const a = r * CHART.cols + c, b = r * COLS + c;
    isLand[b] = CHART.isLand[a];
    if (CHART.regionAt[a] >= 0) regionAt[b] = CHART.regionAt[a];
    if (CHART.zoneAt[a] >= 0) { zoneAt[b] = CHART.zoneAt[a]; dist[b] = CHART.dist[a]; frozen[b] = 1; }
  }
}
for (const p of land) fillRound(isLand, p, 1);
for (const r of regions) for (const p of r.polys) fillRound(regionAt, closed(p), r.id);

// Land that lies against the edge of an older sheet is one land with the land across it: Alaska with the Yukon,
// Asia with the map of Europe. Two shapes are joined where both have a corner on the same edge and their ground
// meets on the grid.
const landOf = new Int32Array(COLS * ROWS).fill(-1);
const shapes = [...core.land.map((p) => closed(p)), ...CHART.landFull, ...land];
const onEdge = (p) => p[0].some(([x, y]) => Math.abs(x - X47) < 0.11 || Math.abs(x - X141) < 0.11 || Math.abs(x + 3342.5) < 0.11 || (Math.abs(y - 1712) < 0.11 && x > -0.1 && x < X47 + 0.1) || (Math.abs(x) < 0.11 && y < 1712.1));
shapes.forEach((p, i) => fillRound(landOf, p, i));
const boss = shapes.map((_, i) => i);
const find = (i) => { while (boss[i] !== i) { boss[i] = boss[boss[i]]; i = boss[i]; } return i; };
const edgy = shapes.map(onEdge);
for (let r = 0; r < ROWS; r++) {
  for (let c = 0; c < COLS; c++) {
    const a = landOf[r * COLS + c];
    if (a < 0 || !edgy[a]) continue;
    for (const b of [landOf[r * COLS + (c + 1) % COLS], r + 1 < ROWS ? landOf[(r + 1) * COLS + c] : -1]) if (b >= 0 && b !== a && edgy[b] && find(a) !== find(b)) boss[find(a)] = find(b);
  }
}
// The land a province stands on: its seed's, for a new one; whatever lies under its middle, for one of today's.
const groundOf = new Map();
for (const s of seeds) if (s.id >= 0 && s.mass >= 0) groundOf.set(s.id, find(masses[s.mass].shape));
const groundAt = (r) => { const c = Math.floor((r.x - gx0) / CELL), row = Math.floor(r.y / CELL); const i = landOf[row * COLS + c]; return i < 0 ? -1 : find(i); };
for (const r of [...core.regions, ...today.regions]) groundOf.set(r.id, groundAt(r));

// Two provinces are neighbours where their ground meets along a fair stretch, counted a cell's edge at a time.
// Ground on two different land masses never meets: a strait is crossed by a link or not at all.
const BORDER = 2;
const meet = new Map();
const tally = (a, b) => {
  if (a < 0 || b < 0 || a === b || (a < first && b < first)) return;
  const k = a < b ? a * 10000 + b : b * 10000 + a;
  meet.set(k, (meet.get(k) ?? 0) + 1);
};
for (let r = 0; r < ROWS; r++) {
  for (let c = 0; c < COLS; c++) {
    const a = regionAt[r * COLS + c];
    if (a < 0) continue;
    tally(a, regionAt[r * COLS + (c + 1) % COLS]);
    if (r + 1 < ROWS) tally(a, regionAt[(r + 1) * COLS + c]);
  }
}
// A border with a province of today is written apart ('joins'): today's rows are never touched.
const joins = [];
for (const [k, n] of [...meet].sort((p, q) => p[0] - q[0])) {
  if (n < BORDER) continue;
  const a = Math.floor(k / 10000), b = k % 10000;
  if (groundOf.get(a) !== groundOf.get(b) || groundOf.get(a) === undefined || groundOf.get(a) < 0) continue;
  if (a < first) joins.push([a, b]);
  else { regionOfId(a).nb.push(b); regionOfId(b).nb.push(a); }
}
const byName = new Map(regions.map((r) => [r.name, r]));
const oldByName = new Map([...core.regions, ...today.regions].map((r) => [r.name, r]));
const links = [];
const pair = (a, b, what) => {
  const ra = byName.get(a), rb = byName.get(b);
  if (!ra || !rb) { warn(`! bad ${what}: ${a}, ${b}`); return null; }
  return [ra, rb];
};
for (const slice of FAR_SLICES) {
  for (const [a, b] of slice.links ?? []) {
    const p = pair(a, b, 'link');
    if (!p) continue;
    if (!p[0].nb.includes(p[1].id)) { p[0].nb.push(p[1].id); p[1].nb.push(p[0].id); }
    links.push([p[0].id, p[1].id]);
  }
  for (const [a, b] of slice.roads ?? []) {
    const p = pair(a, b, 'road');
    if (p && !p[0].nb.includes(p[1].id)) { p[0].nb.push(p[1].id); p[1].nb.push(p[0].id); }
  }
  for (const [a, b] of slice.noBorder ?? []) {
    // A border with a province of today that is not there is struck from 'joins'.
    const old = oldByName.get(a) ?? oldByName.get(b), fresh = byName.get(a) ?? byName.get(b);
    if (old && fresh) { const i = joins.findIndex(([x, y]) => x === old.id && y === fresh.id); if (i >= 0) joins.splice(i, 1); else warn(`! no border to strike: ${a}, ${b}`); continue; }
    const p = pair(a, b, 'border');
    if (!p) continue;
    p[0].nb = p[0].nb.filter((i) => i !== p[1].id);
    p[1].nb = p[1].nb.filter((i) => i !== p[0].id);
  }
}
for (const r of regions) r.nb.sort((p, q) => p - q);
const alone = regions.filter((r) => !r.nb.length && !joins.some(([, b]) => b === r.id)).map((r) => r.name);

// ---- Seas ----------------------------------------------------------------------------------------------------

const nOld = today.seas.zones.length;
const zones = today.seas.zones.map((z, i) => ({ ...z, i, adj: [...z.adj], coast: [...z.coast] }));
seaData.zones.forEach((z, k) => {
  const at = z.label ?? z.at;
  const [x, y] = proj([at[1], at[0]]);
  if (zones.some((o) => o.id === z.id)) warn(`! two seas have the key ${z.id}`);
  zones.push({ i: nOld + k, id: z.id, name: z.name, x: r1(x), y: r1(y), air: z.air ?? 1, ...(z.narrow ? { narrow: true } : {}), adj: [], coast: [] });
});
const index = new Map(zones.map((z) => [z.id, z.i]));

// A heap of [distance, cell], kept in two plain arrays.
let hd = new Float64Array(1 << 20), hk = new Int32Array(1 << 20), hn = 0;
const push = (d, k) => {
  if (hn === hd.length) { const d2 = new Float64Array(hn * 2), k2 = new Int32Array(hn * 2); d2.set(hd); k2.set(hk); hd = d2; hk = k2; }
  let i = hn++;
  while (i > 0) { const p = (i - 1) >> 1; if (hd[p] <= d) break; hd[i] = hd[p]; hk[i] = hk[p]; i = p; }
  hd[i] = d; hk[i] = k;
};
const pop = () => {
  const top = [hd[0], hk[0]];
  const d = hd[--hn], k = hk[hn];
  let i = 0;
  for (;;) {
    let m = 2 * i + 1;
    if (m >= hn) break;
    if (m + 1 < hn && hd[m + 1] < hd[m]) m++;
    if (hd[m] >= d) break;
    hd[i] = hd[m]; hk[i] = hk[m]; i = m;
  }
  hd[i] = d; hk[i] = k;
  return top;
};
const STEPS = [[1, 0, 1], [-1, 0, 1], [0, 1, 1], [0, -1, 1], [1, 1, Math.SQRT2], [1, -1, Math.SQRT2], [-1, 1, Math.SQRT2], [-1, -1, Math.SQRT2]];
// By agreement with the rules, a new sea may be cut out of one of today's where today's chart had no shore to cut
// it by ('takes': the sea it is cut from, and a shape in [lat, lon]): the Red Sea out of the Gulf of Aden, the
// East African coast out of the Mozambique Channel. Those squares change sea, and nothing else of today's does.
const given = new Uint8Array(COLS * ROWS);
const takings = [];
seaData.zones.forEach((z, k) => {
  for (const t of z.takes ?? []) {
    const from = index.get(t.from);
    if (from === undefined || from >= nOld) { warn(`! ${z.name} takes from ${t.from}, which is not a sea of today's chart`); continue; }
    const ring = t.within.map(([lat, lon]) => proj([lon, lat]));
    const [a, b, c, d] = boxOf(ring);
    let n = 0;
    for (let r = Math.max(1, Math.floor(b / CELL)); r <= Math.min(ROWS - 2, Math.ceil(d / CELL)); r++) {
      for (let col = Math.max(0, Math.floor((a - gx0) / CELL)); col <= Math.min(COLS - 1, Math.ceil((c - gx0) / CELL)); col++) {
        const j = r * COLS + col;
        if (zoneAt[j] === from && inRing(ring, gx0 + (col + 0.5) * CELL, (r + 0.5) * CELL)) { zoneAt[j] = nOld + k; given[j] = 1; n++; }
      }
    }
    if (!n) warn(`! ${z.name} takes no water from ${t.from}`);
    takings.push(`${n} squares of ${zones[from].name} to ${z.name}`);
  }
});
// Today's waters stay with the seas they belong to, and those seas go on spreading from where they stand.
for (let k = 0; k < frozen.length; k++) if (frozen[k]) push(dist[k], k);
const anchor = (lat, lon, zone, what) => {
  const [x, y] = proj([lon, lat]);
  const c = Math.floor((x - gx0) / CELL), r = Math.floor(y / CELL);
  if (c < 0 || r < 1 || c >= COLS || r >= ROWS - 1 || isLand[r * COLS + c]) { warn(`! sea anchor on land or off the chart: ${what} (${lat}, ${lon})`); return; }
  if (frozen[r * COLS + c] && zoneAt[r * COLS + c] === zone) return;
  if (frozen[r * COLS + c]) { warn(`! sea anchor inside the seas of today's chart: ${what} (${lat}, ${lon})`); return; }
  dist[r * COLS + c] = 0; zoneAt[r * COLS + c] = zone; push(0, r * COLS + c);
};
seaData.zones.forEach((z, k) => { anchor(z.at[0], z.at[1], nOld + k, z.name); for (const w of z.waters ?? []) anchor(w[0], w[1], nOld + k, z.name); });
while (hn) {
  const [d, k] = pop();
  if (d > dist[k]) continue;
  const c = k % COLS, r = (k - c) / COLS;
  for (const [dc, dr, w] of STEPS) {
    const rr = r + dr;
    // The top and bottom rows are the edge of the chart, not water. East and west there is no edge.
    if (rr < 1 || rr >= ROWS - 1) continue;
    const cc = (c + dc + COLS) % COLS, j = rr * COLS + cc;
    if (isLand[j] || frozen[j] || d + w >= dist[j]) continue;
    if (dc && dr && (isLand[r * COLS + cc] || isLand[rr * COLS + c])) continue;
    dist[j] = d + w; zoneAt[j] = zoneAt[k]; push(d + w, j);
  }
}

// The coasts. A province has a coast on a sea when open water of that sea laps against it.
const touch = new Map();
for (let r = 0; r < ROWS; r++) {
  for (let c = 0; c < COLS; c++) {
    const zone = zoneAt[r * COLS + c];
    if (zone < 0) continue;
    for (const [dc, dr] of STEPS) {
      const rr = r + dr;
      if (rr < 0 || rr >= ROWS) continue;
      const id = regionAt[rr * COLS + (c + dc + COLS) % COLS];
      if (id < 0) continue;
      let m = touch.get(id);
      if (!m) touch.set(id, (m = new Map()));
      m.set(zone, (m.get(zone) ?? 0) + 1);
    }
  }
}
for (const r of regions) r.seas = [];
const oldOnNew = [];
const shown = (arg('coasts') ?? '').split(',');
for (const [id, m] of touch) {
  const most = Math.max(...m.values());
  const ranked = [...m].sort((a, b) => b[1] - a[1] || a[0] - b[0]);
  if (id < first) {
    // A province of today (or of the map of Europe) keeps the coasts it has. Where it also lies on a new sea, that
    // sea's list says so.
    const r = id < today.first ? core.regions[id] : today.regions[id - today.first];
    for (const [zone, n] of ranked) if (zone >= nOld && n >= 8 && n >= most * 0.2) { zones[zone].coast.push(id); oldOnNew.push(`${r.name} on ${zones[zone].name}`); }
    if (id < today.first) continue;
    // (A sea cut out of one of today's by agreement may take a coast with it; none of today's provinces stands there.)
    const now = ranked.filter(([zone, n]) => zone < nOld && n >= 8 && n >= most * 0.2).map(([zone]) => zone);
    for (const z of r.seas) if (!now.includes(z)) warn(`! ${r.name} no longer comes out on ${zones[z].name}`);
    continue;
  }
  const r = regionOfId(id);
  if (shown.includes(r.name)) console.log(`${r.name}: ${ranked.map(([zone, n]) => `${zones[zone].key ?? zones[zone].name} ${n}`).join(', ')}`);
  // Where the count of squares misleads, the seas file says which seas a province stands on ('shores'): each
  // must lap against it all the same.
  const own = seaData.shores?.[r.name];
  if (own) {
    for (const key of own) { const zone = index.get(key); if (zone === undefined || !m.has(zone)) warn(`! ${r.name} is given a shore on ${key}, which does not lap against it`); else r.seas.push(zone); }
    r.seas.sort((a, b) => a - b);
    for (const zone of r.seas) zones[zone].coast.push(id);
    continue;
  }
  for (const [zone, n] of ranked) if (n >= 8 && n >= most * 0.2) r.seas.push(zone);
  // An islet has a shore however small it is, and a port has a sea.
  if (!r.seas.length && (r.flags.port || r.flags.depot || r.flags.islet)) r.seas.push(ranked[0][0]);
  for (const zone of r.seas) zones[zone].coast.push(id);
}
for (const r of regions) if ((r.flags.port || r.flags.islet) && !r.seas.length) warn(`! ${r.name} is a port with no sea`);
for (const z of zones) z.coast.sort((a, b) => a - b);

for (const [a, b] of seaData.adjacent) {
  const ia = index.get(a), ib = index.get(b);
  if (ia === undefined || ib === undefined) { warn(`! bad sea link: ${a}, ${b}`); continue; }
  if (ia < nOld && ib < nOld) { warn(`! ${a} and ${b} are both seas of today: their link is not this file's to make`); continue; }
  if (!zones[ia].adj.includes(ib)) { zones[ia].adj.push(ib); zones[ib].adj.push(ia); }
}
// A link between two of today's seas that stood for water off the old chart is struck where the full chart shows
// the seas between ('unlink', by agreement with the rules): the Mozambique Channel no longer borders the Gulf of Aden.
const unlinked = [];
for (const [a, b] of seaData.unlink ?? []) {
  const ia = index.get(a), ib = index.get(b);
  if (ia === undefined || ib === undefined || !zones[ia].adj.includes(ib)) { warn(`! no link to strike: ${a}, ${b}`); continue; }
  zones[ia].adj = zones[ia].adj.filter((j) => j !== ib);
  zones[ib].adj = zones[ib].adj.filter((j) => j !== ia);
  unlinked.push(ia < ib ? `${ia}-${ib}` : `${ib}-${ia}`);
}
const gates = [...today.seas.gates, ...(seaData.gates ?? []).map((gt) => ({ a: index.get(gt.between[0]), b: index.get(gt.between[1]), name: gt.name, region: gt.region ?? null, nation: gt.nation ?? null, closed: !!gt.closed }))];

// The lines between the seas, traced along the grid where two meet and then smoothed. A line that reaches the edge
// of the chart ends there and begins again on the other side.
const segs = new Map();
const V = (c, r) => r * (COLS + 1) + c;
const add = (za, zb, v0, v1) => {
  const key = za < zb ? `${za}-${zb}` : `${zb}-${za}`;
  let list = segs.get(key);
  if (!list) segs.set(key, (list = []));
  list.push([v0, v1]);
};
for (let r = 0; r < ROWS; r++) {
  for (let c = 0; c < COLS; c++) {
    const z = zoneAt[r * COLS + c];
    if (z < 0) continue;
    { const o = zoneAt[r * COLS + (c + 1) % COLS]; if (o >= 0 && o !== z) add(z, o, V(c + 1, r), V(c + 1, r + 1)); }
    if (r + 1 < ROWS) { const o = zoneAt[(r + 1) * COLS + c]; if (o >= 0 && o !== z) add(z, o, V(c, r + 1), V(c + 1, r + 1)); }
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
    const pts = line.map((v) => [(v % (COLS + 1)) * CELL + gx0, Math.floor(v / (COLS + 1)) * CELL]);
    borders.push(smooth(simplify(pts, 5)).map(([x, y]) => [r1(x), r1(y)]));
  }
}
// Seas that meet on the chart but are not joined in the data, and the other way about.
const meets = new Set([...segs].filter(([, l]) => l.length >= 12).map(([k]) => k));
const told = new Set();
for (const z of zones) for (const j of z.adj) told.add(z.i < j ? `${z.i}-${j}` : `${j}-${z.i}`);
const nm = (k) => k.split('-').map((i) => zones[+i].id).join(' and ');
const fresh = (k) => k.split('-').some((i) => +i >= nOld);
const hints = [];
for (const k of meets) if (!told.has(k) && fresh(k)) hints.push(`? ${nm(k)} meet on the chart (${segs.get(k).length} cells) but are not adjacent`);
const through = new Set(gates.map((gt) => (gt.a < gt.b ? `${gt.a}-${gt.b}` : `${gt.b}-${gt.a}`)));
for (const [a, b] of seaData.passages ?? []) { const ia = index.get(a), ib = index.get(b); through.add(ia < ib ? `${ia}-${ib}` : `${ib}-${ia}`); }
for (const k of told) if (!meets.has(k) && !through.has(k) && fresh(k)) hints.push(`? ${nm(k)} are adjacent but do not meet on the chart (${segs.get(k)?.length ?? 0} cells)`);

// Which sea a point of the chart lies in, at half the detail: each row is [sea, cells, sea, cells...], -1 for land.
const LOOK = 2;
const grid = [];
for (let r = 0; r < ROWS; r += LOOK) {
  const row = [];
  let last = -2, run = 0;
  for (let c = 0; c < COLS; c += LOOK) {
    const z = zoneAt[r * COLS + c];
    if (z === last) { run++; continue; }
    if (run) row.push(last, run);
    last = z; run = 1;
  }
  row.push(last, run);
  grid.push(row);
}

// ---- The checks ----------------------------------------------------------------------------------------------

const checks = [];
const check = (ok, text) => { checks.push(`${ok ? 'ok  ' : 'FAIL'} ${text}`); if (!ok) process.exitCode = 1; };
// 1. Every square of today's chart that holds a sea holds the same sea.
{
  let wrong = 0, seen = 0, cut2 = 0;
  today.seas.grid.forEach((row, r) => {
    let c = 0;
    for (let i = 0; i < row.length; i += 2) for (let k = row[i + 1]; k > 0; k--, c++) { if (row[i] < 0) continue; seen++; const j = r * LOOK * COLS + c * LOOK; if (given[j]) cut2++; else if (zoneAt[j] !== row[i]) wrong++; }
  });
  check(wrong === 0, `today's seas: ${seen - cut2} squares of water hold the sea they hold today (${wrong} do not)${cut2 ? `, and ${cut2} are given to a new sea by agreement` : ''}`);
}
// Water of today's chart that holds no sea today (a lake, a fjord the flood never reached) gains none.
{
  let gained = 0;
  for (let r = 1; r < ROWS - 1; r++) for (let c = 1; c < CHART.cols - 1; c++) if (CHART.zoneAt[r * CHART.cols + c] < 0 && !CHART.isLand[r * CHART.cols + c] && zoneAt[r * COLS + c] >= 0) gained++;
  check(gained === 0, `today's waters without a sea stay without one (${gained} gained one)`);
}
// 2. The new provinces tile their land: no square of a charted land mass is left over, and none is claimed twice.
{
  const count = new Uint8Array(COLS * ROWS);
  const one = new Uint8Array(COLS * ROWS);
  for (const r of regions) {
    one.fill(0);
    for (const p of r.polys) fillRound(one, closed(p), 1);
    for (let k = 0; k < one.length; k++) if (one[k] && count[k] < 250) count[k]++;
  }
  const charted = new Uint8Array(COLS * ROWS);
  const whole = new Set();
  for (const [m, all] of onMass) {
    const mine = all.filter((s) => s.id >= 0);
    if (!mine.length) continue;
    if (all.some((s) => s.side >= 0)) { for (const k of new Set(mine.map((s) => s.side))) for (const p of cut(m, k)) fillRound(charted, p, 1); } else if (all.every((s) => s.id >= 0)) whole.add(m);
  }
  for (const m of whole) fillRound(charted, masses[m].poly, 1);
  masses.forEach((mass) => { if (mass.joined !== undefined) fillRound(charted, mass.poly, 1); });
  // Ground left to a seed 'beyond' the war is nobody's on purpose.
  let twice = 0, gap = 0, total = 0;
  const where = [];
  const place = (k) => { const c = k % COLS, r = (k - c) / COLS; let lon = (gx0 + (c + 0.5) * CELL) / K - 11; if (lon > 180) lon -= 360; return `${(NORTH - ((r + 0.5) * CELL) / 40).toFixed(2)}, ${lon.toFixed(2)}`; };
  for (let k = 0; k < count.length; k++) {
    if (count[k] > 1) { twice++; if (where.length < 12) where.push(`twice at ${place(k)}`); }
    if (charted[k]) { total++; if (!count[k]) { gap++; if (where.length < 12) where.push(`none at ${place(k)}`); } }
  }
  // A seed beyond the war takes its share of a side, which then shows as a gap: say how much of it is that.
  let beyond = 0;
  for (const s of seeds) if (s.id < 0 && s.mass >= 0 && s.cell) { try { const g = polygonClipping.intersection([s.cell], s.part); const one2 = new Uint8Array(COLS * ROWS); for (const p of g) fillRound(one2, p, 1); for (let k = 0; k < one2.length; k++) if (one2[k] && charted[k] && !count[k]) beyond++; } catch { /* counted as a gap */ } }
  // A square whose middle falls exactly on a border is counted by both sides, and one on the very shore by neither:
  // a few in ten thousand are the grid's rounding, more than that is a fault in the cut.
  check(twice <= Math.max(2, total * 0.0001) && gap - beyond <= total * 0.0005, `the new provinces tile their land: ${total} squares of charted land, ${twice} claimed twice, ${gap - beyond} left over${beyond ? ` (and ${beyond} left to the country beyond the war)` : ''}${where.length && where.length < 12 ? `: ${where.join('; ')}` : ''}`);
}
// 3. Every sea can be reached from every other.
{
  const seen = new Set([0]);
  const q = [0];
  for (let i = 0; i < q.length; i++) for (const j of zones[q[i]].adj) if (!seen.has(j)) { seen.add(j); q.push(j); }
  const lost = zones.filter((z) => !seen.has(z.i)).map((z) => z.name);
  check(lost.length === 0, `the seas connect: ${seen.size} of ${zones.length} reached from the ${zones[0].name}${lost.length ? ` (cut off: ${lost.join(', ')})` : ''}`);
  const dry = zones.filter((z) => z.i >= nOld && !zoneAt.includes(z.i)).map((z) => z.name);
  check(dry.length === 0, `every new sea has water${dry.length ? ` (none: ${dry.join(', ')})` : ''}`);
}
// 4. Today's provinces and seas stand as they are written.
check(JSON.stringify(zones.slice(0, nOld).map(({ i, adj, coast, ...z }) => z)) === JSON.stringify(today.seas.zones.map(({ adj, coast, ...z }) => z)), `today's ${nOld} seas keep their keys, names and places`);
check(zones.slice(0, nOld).every((z, i) => today.seas.zones[i].adj.every((j) => z.adj.includes(j) || unlinked.includes(i < j ? `${i}-${j}` : `${j}-${i}`)) && today.seas.zones[i].coast.every((j) => z.coast.includes(j))), `today's seas keep every neighbour and every coast they have${unlinked.length ? ` (but for ${unlinked.length} link struck by agreement)` : ''}`);
// 5. No new province has a neighbour that does not have it back, and every name is its own.
check(regions.every((r) => r.nb.every((j) => j >= first && regionOfId(j).nb.includes(r.id))), 'every border between new provinces is written on both sides');
check(new Set([...core.regions, ...today.regions, ...regions].map((r) => r.name)).size === first + regions.length, 'no two provinces share a name');
check(regions.every((r) => r.polys.length > 0), 'every new province has land');

// ---- The chart, written --------------------------------------------------------------------------------------

const stages = [];
{ let n = today.regions.length; stages.push(n); FAR_SLICES.forEach((_, k) => { n += seeds.filter((s) => s.id >= 0 && s.slice === k).length; stages.push(n); }); }
const out = {
  // The chart's edges in map units: the full circle, from today's west edge round to itself. Europe's own corner
  // stays at 0, 0.
  frame: { x0: gx0, y0: 0, x1: Math.round((gx0 + W) * 100) / 100, y1: Math.round(y1) },
  world: today.world,
  first: today.first,
  // How many provinces the chart had as each slice was added: today's, then each slice of the far side.
  stages,
  land: [...today.land, ...land.map((p) => p.map((r) => r.slice(0, -1).map(([x, y]) => [r1(x), r1(y)])))],
  lakes: [...today.lakes, ...lakes.map((l) => {
    const ring = l.poly[0].slice(0, -1);
    return { name: l.name, level: l.level, x: r1(ring.reduce((s, p) => s + p[0], 0) / ring.length), y: r1(ring.reduce((s, p) => s + p[1], 0) / ring.length), outline: ring.map(([x, y]) => [r1(x), r1(y)]) };
  })],
  regions: [...today.regions, ...regions],
  links: [...today.links, ...links],
  // Land borders between a province of today's chart (or of the map of Europe) and a new one: [old, new]. Neither
  // row's own list of neighbours has the other.
  joins,
  seas: { zones: zones.map(({ i, ...z }) => z), gates, borders, cell: CELL * LOOK, cols: Math.ceil(COLS / LOOK), grid },
};
check(JSON.stringify(out.regions.slice(0, today.regions.length)) === JSON.stringify(today.regions) && JSON.stringify(out.land.slice(0, today.land.length)) === JSON.stringify(today.land), `today's ${today.regions.length} provinces and ${today.land.length} coasts are written as they stand`);
fs.writeFileSync(new URL(`../src/generated/${arg('out') ?? 'world-next'}.json`, import.meta.url), JSON.stringify(out));

const pts = regions.reduce((s, r) => s + r.polys.reduce((t, p) => t + p.reduce((u, q) => u + q.length, 0), 0), 0);
console.log(`\nfar side: ${regions.length} new provinces (${first} to ${first + regions.length - 1}), ${pts} points, ${land.length} new coasts, ${zones.length - nOld} new seas (${nOld} to ${zones.length - 1}), ${out.frame.x0} to ${out.frame.x1} by ${out.frame.y1}, ${(JSON.stringify(out).length / 1024).toFixed(0)} KB`);
console.log(`${tabled} of ${regions.length} new provinces have a row in the tables`);
if (!quiet) {
  if (pieces.length) console.log(`pieces across the water: ${pieces.join(', ')}`);
  if (joined.length) console.log(`islands joined to: ${[...new Set(joined)].join(', ')}`);
  if (alone.length) console.log(`no neighbours: ${alone.join(', ')}`);
  if (joins.length) console.log(`borders with today's provinces: ${joins.map(([a, b]) => `${[...core.regions, ...today.regions][a].name} - ${regionOfId(b).name}`).join(', ')}`);
  if (oldOnNew.length) console.log(`today's provinces on new seas: ${oldOnNew.join(', ')}`);
  if (takings.length) console.log(`cut out of today's seas: ${takings.join(', ')}`);
}
for (const h of hints) console.log(h);
console.log(checks.join('\n'));
if (warnings.length) console.log(`${warnings.length} warning${warnings.length === 1 ? '' : 's'} above`);

// The list of the new provinces for whoever fills in their tables: --csv=path.
if (arg('csv')) {
  const q = (v) => (/[",\n]/.test(String(v)) ? `"${String(v).replace(/"/g, '""')}"` : String(v));
  const rows = [['id', 'name', 'slice', 'lat', 'lon', 'owner', 'terrain', 'vp', 'factories', 'port', 'islet', 'col', 'x', 'y', 'area', 'seas', 'neighbours']];
  for (const r of regions) {
    const s = seeds[r.id - first];
    const area = Math.round(r.polys.reduce((t, p) => t + Math.abs(ringArea(closed(p)[0])), 0));
    rows.push([r.id, r.name, FAR_SLICES[s.slice].name, s.lat, s.lon, r.owner, r.terrain, r.vp, r.fac, r.flags.port ? 1 : 0, r.flags.islet ? 1 : 0, r.flags.col ? 1 : 0, r.x, r.y, area, r.seas.map((z) => zones[z].name).join('; '), r.nb.map((j) => regionOfId(j).name).join('; ')]);
  }
  fs.writeFileSync(arg('csv'), rows.map((row) => row.map(q).join(',')).join('\n') + '\n');
}

// ---- The thicker chart ---------------------------------------------------------------------------------------

// The cities cut out of the provinces round them (carve-cities.mjs): written beside this chart, which stays as it is.
carveCities({ core, today, regions, first, tables, parentRows, nowRows, fromTable, tabledSoFar: () => tabled, warn, proj, W, gx0, COLS, ROWS, inPoly, boxOf, closed, r1, trim, fillRound, regionAt, zoneAt, STEPS, BORDER, joins, links, zones, index, stages, out, arg, quiet });
