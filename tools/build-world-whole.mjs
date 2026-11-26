// The whole wider world as the grand campaign plays it: the Americas with their cities, then the far side.
//
//   node tools/build-world-whole.mjs [--ids=path]
//
// The cities version (tools/build-world-americas.mjs) lays the Americas' 201 provinces at 375 to 575 and their 339
// cities straight after them, at 576 to 914. This tool lays the rest of the map builder's final chart
// (src/generated/world-cities.json) after those, in the chart's own order: the Pacific (chart ids 576 to 887), the
// globe (888 to 1254), and then every city cut out of a province of either (chart ids 1255 on, parent outside the
// Americas). So the chart still only grows at its end: a campaign saved on the cities version loads on this one, and
// every province it knew keeps its number (save.ts, settleNewGround). Agreed with the rules builder, 17 November 2026.
//
// What is taken from where:
//   the Americas, exactly as the cities version has them: world-americas.json (its provinces, ferries and lakes);
//   the far side's provinces, their outlines, worth, ground, coasts and neighbours: world-cities.json, renumbered;
//   the land borders between an older province (Europe's or the Americas') and a far one: world-cities.json's
//     'joins', renumbered and kept as a list (world.ts adds them to both provinces when the world is attached);
//   the sea zones with their grid, gates and borders, the land and the frame (the whole circle of the world, its
//     seam at 141 W): world-cities.json, each zone's coasts renumbered;
//   railways and rivers: the cities version's lists first, unchanged, then the far side's from world-cities-rails.json
//     and world-cities-rivers.json, renumbered, so every index the Americas had stays what it was.
//
// Writes src/generated/world-whole.json, world-whole-rails.json and world-whole-rivers.json and, given --ids=path,
// the table of numbers (chart id to game id, with names) for those who key rows by number; the game never reads
// it. Prints the checks: every id in order, no neighbour or
// coast outside the chart, every border both ways, names unique on the whole map, each country's provinces, victory
// points and factories on the far side against the chart, and the owner codes the far side carries.
import fs from 'node:fs';

const gen = (f) => new URL(`../src/generated/${f}`, import.meta.url);
const read = (f) => JSON.parse(fs.readFileSync(gen(f), 'utf8'));
const A = read('world-americas.json');
const AR = read('world-americas-rails.json');
const AV = read('world-americas-rivers.json');
const C = read('world-cities.json');
const CR = read('world-cities-rails.json');
const CV = read('world-cities-rivers.json');
const EU = read('campaign-map.json');
const idsOut = process.argv.find((a) => a.startsWith('--ids='))?.slice(6);

const first = A.first;
if (C.first !== first) throw new Error('the two charts do not start at the same id');
const problems = [];
const byId = new Map(C.regions.map((r) => [r.id, r]));
const top = (id) => { let r = byId.get(id); while (r.of !== undefined) r = byId.get(r.of); return r.id; };

// The Americas' numbers, as the cities version laid them: its 201 keep theirs, each city of one of them follows in
// the chart's order. Checked against world-americas.json province by province.
const PAC = 576; // the chart's first province beyond the Americas
const remap = new Map();
const americas = C.regions.filter((r) => r.id < PAC || (r.of !== undefined && top(r.id) < PAC));
const own201 = new Set(C.regions.filter((r) => r.id < PAC).map((r) => r.id));
let next = first;
for (const r of americas.filter((x) => own201.has(x.id))) remap.set(r.id, next++);
for (const r of americas.filter((x) => !own201.has(x.id)).sort((a, b) => a.id - b.id)) remap.set(r.id, next++);
if (next !== first + A.regions.length) problems.push(`the Americas: ${next - first} provinces here, ${A.regions.length} in world-americas.json`);
for (const r of americas) {
  const a = A.regions[remap.get(r.id) - first];
  if (!a || a.x !== r.x || a.y !== r.y || a.owner !== r.owner) problems.push(`the Americas: chart ${r.id} (${r.name}) is not game ${remap.get(r.id)} (${a?.name})`);
}
for (const r of C.regions) if (r.of !== undefined && byId.get(r.of).of !== undefined) problems.push(`${r.name}: a city cut out of a city (${byId.get(r.of).name})`);
// The far side: the Pacific, the globe, then their cities, each in the chart's order.
const FAR_FIRST = next;
const far = C.regions.filter((r) => top(r.id) >= PAC).sort((a, b) => (a.of !== undefined) - (b.of !== undefined) || a.id - b.id);
for (const r of far) remap.set(r.id, next++);
const inChart = (id) => id < first || remap.has(id);
const to = (id) => (id < first ? id : remap.get(id));

// The names: the chart's own. The clerk's tables give a name as of September 1939 for 95 far provinces (Seoul as
// Keijo, Peiping as "Peiping (Peking)"), but every row written for the far side so far (the seats, the China line,
// the first-day holders) names the chart's: they are listed here and not applied, for the rules builder to rule on.
const period = new Map();
for (const f of ['cities-parents.csv', 'cities.csv', 'pacific.csv', 'globe.csv']) {
  const [head, ...rows] = fs.readFileSync(new URL(`../data/campaign/tables/${f}`, import.meta.url), 'utf8').trim().split('\n');
  const cols = head.split(','), at = cols.indexOf('name'), was = cols.indexOf('period_name');
  for (const row of rows) {
    const v = row.split(',');
    if (v[was] && v[was] !== v[at] && !period.has(v[at])) period.set(v[at], v[was]);
  }
}
const renamed = [];

// The far provinces.
const farRegions = far.map((r) => {
  const out = { ...r, id: to(r.id), nb: r.nb.filter((j) => { if (!inChart(j)) problems.push(`${r.name}: neighbour ${j} outside the chart`); return inChart(j); }).map(to) };
  if (r.of !== undefined) out.of = to(r.of);
  if (period.has(r.name)) renamed.push(`${r.name} -> ${period.get(r.name)}`);
  return out;
});
// The Vichy mark (game.ts, the fall: ground so marked goes to Vichy France with its corps, the rest of a fallen
// country's colonies passes into a sea power's keeping). The chart gives it by hand to 41 provinces of French Africa
// and Madagascar. Two things more, found with the far side on (18 November):
//   - Indochina's eleven provinces are marked, as ruling 199 says (point 1), and so are the cities cut out of them;
//   - a city cut out of a marked province carries the mark, as it carries the colony mark: without it 19 cities of
//     French West Africa and Madagascar (Segou of Bamako, Antsirabe of Tananarive) passed into British keeping
//     while the ground round them went to Vichy.
const VICHY_199 = ['Hanoi', 'Vinh', 'Hue', 'Qui Nhon', 'Cam Ranh', 'Saigon', 'Phnom Penh', 'Battambang', 'Pakse', 'Vientiane', 'Luang Prabang'];
const vichyBy = { chart: 0, ruling199: 0, city: 0 };
for (const r of farRegions) if (r.flags?.vichy) vichyBy.chart++;
for (const name of VICHY_199) {
  const r = farRegions.find((x) => x.name === name);
  if (!r || r.owner !== 'FRA' || r.of !== undefined) { problems.push(`ruling 199: ${name} is not a province of France's on the chart`); continue; }
  if (!r.flags?.vichy) { r.flags = { ...r.flags, vichy: 1 }; vichyBy.ruling199++; }
}
const farAt = new Map(farRegions.map((r) => [r.id, r]));
for (const r of farRegions) {
  if (r.of === undefined || r.flags?.vichy) continue;
  const p = farAt.get(r.of);
  if (p?.flags?.vichy && r.owner === p.owner) { r.flags = { ...r.flags, vichy: 1 }; vichyBy.city++; }
}

// Oilfields the old chart already counts elsewhere (ruling 231, 1(d)): the oil of Iran's Ahwaz and Saudi Arabia's
// Dhahran is the Gulf route's (Britain's share of need) and Iraq's 12, as on the old chart, so their provinces carry
// no oil mark; with a yield of their own Britain would be paid twice for southern Iran. A lane that names its fields
// (ruling 194, points 2 to 4) may carry them later.
const UNMARK_231 = ['Ahwaz', 'Dhahran'];
for (const name of UNMARK_231) {
  const r = farRegions.find((x) => x.name === name);
  if (!r?.flags?.oil) { problems.push(`ruling 231: ${name} is not an oilfield of the chart`); continue; }
  const { oil, ...rest } = r.flags;
  r.flags = rest;
}

// Where the final chart has redrawn one of the Americas' provinces at its edge with the far side (the Alaska panhandle's
// mainland, taken from Whitehorse, Watson Lake and Prince Rupert on 17 November), the whole world takes its outline and
// its seas from the final chart; everything else, its neighbours above all, stays the cities version's. With the far
// side off the cities version's own chart is played, unchanged.
const redrawn = [];
const americasAt = new Map(americas.map((r) => [remap.get(r.id), r]));
const older = A.regions.map((a) => {
  const r = americasAt.get(a.id);
  const same = (k) => JSON.stringify(a[k]) === JSON.stringify(r[k]);
  if (same('polys') && same('seas')) return a;
  redrawn.push(`${a.name}${same('seas') ? '' : ` (seas ${JSON.stringify(a.seas ?? [])} to ${JSON.stringify(r.seas ?? [])})`}`);
  return { ...a, polys: r.polys, seas: r.seas };
});
const regions = [...older, ...farRegions];
regions.forEach((r, i) => { if (r.id !== first + i) problems.push(`id ${r.id} at place ${first + i}`); });

// The land borders across the stages: [older province, far province], both as the game numbers them.
const joins = (C.joins ?? []).map(([a, b]) => {
  if (!inChart(a) || !inChart(b)) { problems.push(`join ${a}-${b} outside the chart`); return null; }
  const [o, n] = [to(a), to(b)];
  if (n < FAR_FIRST || o >= FAR_FIRST) problems.push(`join ${a}-${b} is not an older province and a far one (${o}, ${n})`);
  return [o, n];
}).filter(Boolean);
// Every border both ways, joins counted.
const nbOf = (id) => (id < first ? EU.regions[id].nb : regions[id - first].nb);
const joined = new Set(joins.flatMap(([a, b]) => [`${a}-${b}`, `${b}-${a}`]));
for (const r of farRegions) for (const j of r.nb) {
  if (!nbOf(j).includes(r.id) && !joined.has(`${r.id}-${j}`)) problems.push(`${r.name} -> ${j < first ? EU.regions[j].name : regions[j - first].name} one way`);
}

// The seas: the final chart's, each zone's coasts renumbered.
const seas = { ...C.seas, zones: C.seas.zones.map((z, i) => {
  const a = A.seas.zones[i];
  if (a && (a.name !== z.name || a.x !== z.x || a.y !== z.y)) problems.push(`sea ${i} moved: ${a.name} / ${z.name}`);
  return { ...z, coast: z.coast.filter((id) => { if (!inChart(id)) problems.push(`${z.name}: coast ${id} outside the chart`); return inChart(id); }).map(to) };
}) };
// The Americas' coasts: the cities version's, but for a redrawn province that has left a sea or come onto one.
for (let i = 0; i < A.seas.zones.length; i++) {
  const was = A.seas.zones[i].coast.filter((id) => id >= first && (older[id - first].seas ?? []).includes(i));
  for (const r of older) if (r !== A.regions[r.id - first] && (r.seas ?? []).includes(i) && !was.includes(r.id)) was.push(r.id);
  const now = seas.zones[i].coast.filter((id) => id >= first && id < FAR_FIRST);
  if ([...was].sort((a, b) => a - b).join() !== [...now].sort((a, b) => a - b).join()) problems.push(`${A.seas.zones[i].name}: the Americas' coasts differ from the cities version's`);
}
for (const r of regions) for (const s of r.seas ?? []) if (!seas.zones[s]?.coast.includes(r.id)) problems.push(`${r.name} on sea ${s} but not in its coasts`);
for (const [i, z] of seas.zones.entries()) for (const id of z.coast) if (id >= first && !(regions[id - first].seas ?? []).includes(i)) problems.push(`${z.name} lists ${regions[id - first].name}, which does not name it`);

// The lakes: the cities version's, then every other lake of the final chart.
const lakes = [...A.lakes];
for (const l of C.lakes) if (!lakes.some((k) => k.name === l.name)) lakes.push(l);
// The ferries: the cities version's, then the far side's.
const has = (list, [a, b]) => list.some(([x, y]) => (x === a && y === b) || (x === b && y === a));
const links = [...A.links];
for (const [a, b] of C.links) {
  if (!inChart(a) || !inChart(b)) continue;
  const l = [to(a), to(b)];
  if (!has(links, l)) links.push(l);
}

const chart = { frame: C.frame, world: C.world, first, stages: [...A.stages, regions.length], land: C.land, lakes, regions, links, joins, seas };

// Railways: the cities version's links as they are, then every far link of the final chart.
const farId = (id) => id >= FAR_FIRST;
const rails = { links: [...AR.links] };
for (const [a, b, x, y] of CR.links) {
  if (!inChart(a) || !inChart(b)) { problems.push(`rail ${a}-${b} outside the chart`); continue; }
  const [p, q] = [to(a), to(b)];
  if (!farId(p) && !farId(q)) continue; // the Americas' and Europe's own are in the cities version's list
  rails.links.push([p, q, x, y]);
}
for (const [a, b] of rails.links) {
  if (!(a < first ? EU.regions[a].nb : regions[a - first].nb).includes(b) && !joined.has(`${a}-${b}`) && !nbOf(b).includes(a)) problems.push(`rail ${a}-${b} between provinces that do not touch`);
}
// Rivers: the cities version's in its order, then every river of the final chart it left out, in the chart's order;
// the crossings the same way. A river drawn round the seam is drawn as the chart draws it: the page shows the far
// end of the world beyond the seam, so its course past the frame's edge is in view there.
const sameRiver = (p, q) => p.name === q.name && p.pts.length === q.pts.length && p.pts.every(([x, y], k) => (Math.abs(x - q.pts[k][0]) < 0.01 || Math.abs(Math.abs(x - q.pts[k][0]) - C.world.width) < 0.01) && y === q.pts[k][1]);
const riverIndex = new Map(); // chart river index -> whole index
CV.rivers.forEach((r, i) => { const k = AV.rivers.findIndex((v) => sameRiver(r, v)); if (k >= 0) riverIndex.set(i, k); });
if (riverIndex.size !== AV.rivers.length) problems.push(`rivers: ${riverIndex.size} of the cities version's ${AV.rivers.length} found on the final chart`);
const rivers = { rivers: [...AV.rivers], crossings: [...AV.crossings] };
CV.rivers.forEach((r, i) => { if (!riverIndex.has(i)) { riverIndex.set(i, rivers.rivers.length); rivers.rivers.push(r); } });
for (const [a, b, river, x, y] of CV.crossings) {
  if (!inChart(a) || !inChart(b)) { problems.push(`crossing ${a}-${b} outside the chart`); continue; }
  const [p, q] = [to(a), to(b)];
  if (!farId(p) && !farId(q)) continue;
  rivers.crossings.push([p, q, riverIndex.get(river), x, y]);
}

// Names: unique on the whole map.
const names = new Map();
for (const r of [...EU.regions, ...regions]) { if (names.has(r.name)) problems.push(`two provinces named ${r.name} (${names.get(r.name)} and ${r.id})`); names.set(r.name, r.id); }

// Each country's provinces, victory points and factories on the far side: the chart's, unchanged.
const sum = (rs) => { const t = {}; for (const r of rs) { const o = (t[r.owner] ??= { vp: 0, fac: 0, n: 0 }); o.vp += r.vp ?? 0; o.fac += r.fac ?? 0; o.n++; } return t; };
const before = sum(far), after = sum(farRegions);
for (const k of Object.keys(before)) if (JSON.stringify(before[k]) !== JSON.stringify(after[k])) problems.push(`${k}: the far side's totals changed`);
const table = Object.entries(after).sort((a, b) => b[1].n - a[1].n).map(([k, t]) => `${k.padEnd(4)} provinces ${String(t.n).padStart(4)}   vp ${String(t.vp).padStart(3)}   factories ${String(t.fac).padStart(3)}`);

fs.writeFileSync(gen('world-whole.json'), JSON.stringify(chart));
fs.writeFileSync(gen('world-whole-rails.json'), JSON.stringify(rails));
fs.writeFileSync(gen('world-whole-rivers.json'), JSON.stringify(rivers));
if (idsOut) {
  const ids = { _about: 'The grand campaign\'s number for each province of the map builder\'s final chart (src/generated/world-cities.json): chart id -> game id. Europe\'s (0 to 374) and the Americas\' first 201 (375 to 575) keep theirs. Written by tools/build-world-whole.mjs.', farFirst: FAR_FIRST, ids: Object.fromEntries([...remap].sort((a, b) => a[0] - b[0]).map(([k, v]) => [k, { game: v, name: regions[v - first].name }])) };
  fs.writeFileSync(idsOut, JSON.stringify(ids, null, 1));
}
console.log(`the whole wider world: ${A.regions.length} provinces of the Americas (${first} to ${FAR_FIRST - 1}) and ${farRegions.length} beyond them (${FAR_FIRST} to ${first + regions.length - 1}); stages ${chart.stages.join(', ')}`);
console.log(`${joins.length} land borders across the stages, ${links.length} ferries, ${seas.zones.length} seas, ${lakes.length} lakes, ${rails.links.length} railway links, ${rivers.rivers.length} rivers with ${rivers.crossings.length} crossings`);
console.log(`the Vichy mark beyond the old chart: ${vichyBy.chart} provinces as the chart gives it, ${vichyBy.ruling199} of Indochina by ruling 199, ${vichyBy.city} cities from their province; oil marks taken off by ruling 231: ${UNMARK_231.join(", ")}; oilfields beyond the old chart: ${farRegions.filter((r) => r.flags?.oil).map((r) => r.name).join(", ")}`);
console.log(`the Americas redrawn by the final chart, outline and seas taken from it with the far side on (${redrawn.length}): ${redrawn.join(', ') || 'none'}`);
console.log(table.join('\n'));
console.log(`names as of September 1939 in the clerk's tables, NOT applied (${renamed.length}): ${renamed.join(', ') || 'none'}`);
console.log(problems.length ? `PROBLEMS (${problems.length}):\n${problems.slice(0, 60).join('\n')}` : 'checks: no problems');
