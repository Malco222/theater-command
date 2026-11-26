// The cities version's chart: the Americas as the grand campaign plays them, with their cities.
//
//   node tools/build-world-americas.mjs
//
// The world map builder's final chart (src/generated/world-cities.json, with -rails and -rivers) holds the whole
// world: the Americas' 201 provinces of world-map.json first (ids 375 to 575), the far side next (576 to 1254), and
// last the cities cut out of both (1255 on). The grand campaign plays the Americas alone until step 13 brings the far
// side (the design director, 17 November 2026: "the cities version for the Americas, step 13 and the globe's stage
// for the far side"). So this tool lays the Americas' cities straight after the Americas' own provinces, keeping
// their order: a city of the Americas takes the next free number after 575. Nothing of the map builder's files is
// changed, and the chart still only grows at its end: a campaign saved on the 201 provinces loads on this one (the
// new cities begin as on the first day, save.ts), and the far side will be laid after these at step 13.
//
// What is taken from where:
//   provinces, their outlines, worth, ground, coasts and neighbours: world-cities.json, renumbered;
//   the sea zones, their grid, gates and borders, the land and the frame: world-map.json (the Americas' seas are
//     unchanged on the final chart; only each zone's list of coasts gains the cities on it, read from world-cities);
//   the lakes: world-map.json's six and every lake of world-cities.json in the Americas;
//   the ferries: world-cities.json's links within the Americas;
//   railways and rivers: world-cities-rails.json and world-cities-rivers.json, the Americas' alone, renumbered.
//
// Writes src/generated/world-americas.json, world-americas-rails.json and world-americas-rivers.json, and prints the
// checks: every id in order, no neighbour or coast outside the chart, names unique against Europe's and each other,
// and every country's victory points and factories on the 201 provinces against the cities chart.
import fs from 'node:fs';

const gen = (f) => new URL(`../src/generated/${f}`, import.meta.url);
const read = (f) => JSON.parse(fs.readFileSync(gen(f), 'utf8'));
const A = read('world-map.json');
const C = read('world-cities.json');
const CR = read('world-cities-rails.json');
const CV = read('world-cities-rivers.json');
const EU = read('campaign-map.json');

const first = A.first, last = first + A.regions.length - 1;
if (C.first !== first) throw new Error('the two charts do not start at the same id');
const byId = new Map(C.regions.map((r) => [r.id, r]));
// The Americas: the 201, then every city whose parent is one of them, in the chart's order.
const own = new Set(A.regions.map((r) => r.id));
const cities = C.regions.filter((r) => r.of !== undefined && own.has(r.of)).sort((a, b) => a.id - b.id);
const remap = new Map();
for (const r of A.regions) remap.set(r.id, r.id);
cities.forEach((r, i) => remap.set(r.id, last + 1 + i));
const inChart = (id) => id < first || remap.has(id);
const to = (id) => (id < first ? id : remap.get(id));
const problems = [];

// The name each shows: the name as of September 1939 where the clerk gives one (the rules builder's ruling of 17
// November: Coos Bay was Marshfield until 1944). Read from the clerk's tables, cities-parents.csv before cities.csv.
const period = new Map();
for (const f of ['cities-parents.csv', 'cities.csv']) {
  const [head, ...rows] = fs.readFileSync(new URL(`../data/campaign/tables/${f}`, import.meta.url), 'utf8').trim().split('\n');
  const cols = head.split(','), at = cols.indexOf('name'), was = cols.indexOf('period_name');
  for (const row of rows) {
    const v = row.split(',');
    if (v[was] && v[was] !== v[at] && !period.has(v[at])) period.set(v[at], v[was]);
  }
}
const renamed = [];

// The provinces.
const regions = [...A.regions.map((r) => byId.get(r.id)), ...cities].map((r) => {
  const out = { ...r, id: to(r.id), nb: r.nb.filter((j) => { if (!inChart(j)) problems.push(`${r.name}: neighbour ${j} outside the chart`); return inChart(j); }).map(to) };
  if (r.of !== undefined) out.of = to(r.of);
  if (period.has(r.name)) { out.name = period.get(r.name); renamed.push(`${r.name} -> ${out.name}`); }
  return out;
});
regions.forEach((r, i) => { if (r.id !== first + i) problems.push(`id ${r.id} at place ${first + i}`); });
for (const r of regions) for (const j of r.nb) {
  if (j >= first && !regions[j - first].nb.includes(r.id)) problems.push(`${r.name} -> ${regions[j - first].name} one way`);
}
for (const r of regions) for (const s of r.seas ?? []) if (s >= A.seas.zones.length) problems.push(`${r.name}: sea ${s} beyond the Americas' seas`);

// The seas: world-map.json's, each zone's coasts as on the final chart within the Americas (and Europe's as they were).
const seas = { ...A.seas, zones: A.seas.zones.map((z, i) => {
  const c = C.seas.zones[i];
  if (c.name !== z.name || c.x !== z.x || c.y !== z.y) problems.push(`sea ${i} moved: ${z.name} / ${c.name}`);
  return { ...z, coast: c.coast.filter((id) => inChart(id)).map(to) };
}) };
// Each coast a province names is in its sea's list, and the other way round.
for (const r of regions) for (const s of r.seas ?? []) if (!seas.zones[s].coast.includes(r.id) && r.id >= first) problems.push(`${r.name} on ${seas.zones[s].name} but not in its coasts`);
for (const z of seas.zones) for (const id of z.coast) if (id >= first && !(regions[id - first].seas ?? []).includes(seas.zones.indexOf(z))) problems.push(`${z.name} lists ${regions[id - first].name}, which does not name it`);

// The lakes of the Americas.
const westOf = (x) => x < -100;
const lakes = [...A.lakes];
for (const l of C.lakes) if (westOf(l.x) && !lakes.some((k) => k.name === l.name)) lakes.push(l);
// The ferries.
const links = C.links.filter(([a, b]) => inChart(a) && inChart(b) && (a >= first || b >= first)).map(([a, b]) => [to(a), to(b)]);

const chart = { frame: A.frame, world: A.world, first, stages: [A.regions.length, regions.length], land: A.land, lakes, regions, links, seas };

// Railways.
const rails = { links: CR.links.filter(([a, b]) => inChart(a) && inChart(b) && (a >= first || b >= first)).map(([a, b, x, y]) => [to(a), to(b), x, y]) };
for (const [a, b] of rails.links) if (!regions[a - first]?.nb.includes(b) && !(regions[b - first]?.nb.includes(a))) problems.push(`rail ${a}-${b} between provinces that do not touch`);
// Rivers: those of the Americas, in the final chart's order, and their crossings. A river some crossing of the
// Americas is over is kept wherever it runs; a river drawn round the seam (the Yukon, from Alaska on the far side of
// the chart) is brought back west by the width of the world.
const used = new Set(CV.crossings.filter(([a, b]) => remap.has(a) && remap.has(b)).map((c) => c[2]));
const W = A.world.width;
const keep = CV.rivers.map((r, i) => [r, i]).filter(([r, i]) => used.has(i) || r.pts.every(([x]) => westOf(x)))
  .map(([r, i]) => [r.pts.some(([x]) => x > A.frame.x1) ? { ...r, pts: r.pts.map(([x, y]) => [x > A.frame.x1 ? +(x - W).toFixed(3) : x, y]) } : r, i]);
const riverAt = new Map(keep.map(([, i], k) => [i, k]));
const crossings = [];
for (const [a, b, river, x, y] of CV.crossings) {
  if (!(remap.has(a) && remap.has(b))) continue;
  if (!riverAt.has(river)) { problems.push(`crossing ${a}-${b} on a river not kept (${CV.rivers[river].name})`); continue; }
  crossings.push([to(a), to(b), riverAt.get(river), x, y]);
}
// A river whose foot the final chart cut back to the sea's shore (more-rivers-net.mjs), short of crossings it still
// has, runs on through them, nearest first: a bridge stands on its river. (The St Lawrence below Quebec: the chart's
// coast takes the estuary from 33 units above its last crossing; the art director and the 3D builder, 18 November.)
const OFF = 2;
const offLine = (pts, x, y) => {
  let d = Infinity;
  for (let i = 0; i + 1 < pts.length; i++) {
    const [ax, ay] = pts[i], [bx, by] = pts[i + 1], L2 = (bx - ax) ** 2 + (by - ay) ** 2;
    const t = L2 ? Math.max(0, Math.min(1, ((x - ax) * (bx - ax) + (y - ay) * (by - ay)) / L2)) : 0;
    d = Math.min(d, Math.hypot(ax + t * (bx - ax) - x, ay + t * (by - ay) - y));
  }
  return d;
};
const K = Math.cos((50 * Math.PI) / 180) * 40;
const llOf = ([x, y]) => [Math.round((71.4 - y / 40) * 10000) / 10000, Math.round((x / K - 11) * 10000) / 10000];
keep.forEach(([r], k) => {
  const away = crossings.filter((c) => c[2] === k && offLine(r.pts, c[3], c[4]) > OFF).map((c) => [c[3], c[4]]);
  if (!away.length) return;
  const pts = [...r.pts], ll = r.ll ? [...r.ll] : undefined;
  while (away.length) {
    const foot = pts[pts.length - 1], head = pts[0];
    let at = 0;
    away.forEach((p, i) => { if (Math.hypot(p[0] - foot[0], p[1] - foot[1]) < Math.hypot(away[at][0] - foot[0], away[at][1] - foot[1])) at = i; });
    const p = away.splice(at, 1)[0];
    if (Math.hypot(p[0] - head[0], p[1] - head[1]) < Math.hypot(p[0] - foot[0], p[1] - foot[1])) { problems.push(`a crossing of the ${r.name} lies off its line nearer its head (${p})`); continue; }
    if (offLine(pts, p[0], p[1]) <= OFF) continue;
    pts.push(p);
    ll?.push(llOf(p));
  }
  keep[k][0] = { ...r, pts, ...(ll ? { ll } : {}) };
  console.log(`the ${r.name} runs on through ${pts.length - r.pts.length} crossing(s) past the foot the chart gave it`);
});
const rivers = { rivers: keep.map(([r]) => r), crossings };

// Names: unique on the whole map.
const names = new Map();
for (const r of [...EU.regions, ...regions]) { if (names.has(r.name)) problems.push(`two provinces named ${r.name}`); names.set(r.name, r.id); }

// Ruling 210: every country's victory points and factories held.
const sum = (rs) => { const t = {}; for (const r of rs) { const o = (t[r.owner] ??= { vp: 0, fac: 0, n: 0 }); o.vp += r.vp ?? 0; o.fac += r.fac ?? 0; o.n++; } return t; };
const before = sum(A.regions), after = sum(regions);
const table = Object.keys({ ...before, ...after }).sort().map((k) => {
  const b = before[k] ?? { vp: 0, fac: 0, n: 0 }, a = after[k] ?? { vp: 0, fac: 0, n: 0 };
  return `${k.padEnd(4)} provinces ${String(b.n).padStart(3)} -> ${String(a.n).padStart(3)}   vp ${String(b.vp).padStart(3)} -> ${String(a.vp).padStart(3)}   factories ${String(b.fac).padStart(3)} -> ${String(a.fac).padStart(3)}${b.vp !== a.vp || b.fac !== a.fac ? '   CHANGED' : ''}`;
});

fs.writeFileSync(gen('world-americas.json'), JSON.stringify(chart));
fs.writeFileSync(gen('world-americas-rails.json'), JSON.stringify(rails));
fs.writeFileSync(gen('world-americas-rivers.json'), JSON.stringify(rivers));
console.log(`the Americas: ${A.regions.length} provinces and ${cities.length} cities, ids ${first} to ${first + regions.length - 1}; ${lakes.length} lakes, ${links.length} ferries, ${rails.links.length} railway links, ${rivers.rivers.length} rivers with ${crossings.length} crossings`);
console.log(table.join('\n'));
console.log(`names as of September 1939: ${renamed.join(', ') || 'none'}`);
console.log(problems.length ? `PROBLEMS (${problems.length}):\n${problems.slice(0, 40).join('\n')}` : 'checks: no problems');
