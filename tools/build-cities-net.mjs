// Carries the rivers' crossings and the railways of the chart as handed over (world-next-rivers.json,
// world-next-rails.json) on to the chart with the cities (world-cities.json, written by build-geo-far.mjs), and
// writes world-cities-rivers.json and world-cities-rails.json in the same forms. Run after build-geo-far,
// build-rivers far and build-rails far.
//
// Nothing between two provinces that were not cut is looked at again: their crossings and their lines stand as they
// were handed over. Where a province was cut:
//  - a crossing is found among its pieces as it is everywhere (the road between two seeds, crossed by a river near
//    its middle), with every list of corrections there is, and CITY_RIVER_CUT and CITY_RIVER_ADD besides;
//  - a railway that ran between two provinces runs on between the same two where they still touch, and otherwise
//    by the easiest way through their pieces; every city is then joined to the net by its easiest border with a
//    province of its own country that is on it, unless it had no railway (CITY_NO_RAIL).
import fs from 'node:fs';
import { GROUND } from '../data/campaign/rails.mjs';
import { WORLD_RIVER_CUT, WORLD_RIVER_ADD } from '../data/campaign/world.mjs';
import { FAR_RIVER_CUT, FAR_RIVER_ADD } from '../data/campaign/far.mjs';
import { CITY_RIVER_CUT, CITY_RIVER_ADD, CITY_NO_RAIL, CITY_RAIL_LINES, CITY_RAIL_CUT, CITY_RAIL_RELAID } from '../data/campaign/cities.mjs';
import { moreRivers } from './more-rivers-net.mjs';

const read = (name) => JSON.parse(fs.readFileSync(new URL(`../src/generated/${name}.json`, import.meta.url), 'utf8'));
const map = read('world-cities');
const europe = read('campaign-map').regions;
const beforeRivers = read('world-next-rivers'), beforeRails = read('world-next-rails');
const all = [...europe, ...map.regions];
const at = new Map(all.map((r) => [r.id, r]));
const byName = new Map(all.map((r) => [r.name, r]));
const key = (a, b) => (a < b ? `${a}:${b}` : `${b}:${a}`);
const r1 = (v) => Math.round(v * 10) / 10;
const cut = new Set(map.regions.filter((r) => r.of !== undefined).map((r) => r.of));
const isPiece = (id) => cut.has(id) || at.get(id)?.of !== undefined;
const family = (id) => at.get(id).of ?? id;
// Every border of the chart, both ways: a province's own list, and the borders written apart ('joins').
const nb = new Map(all.map((r) => [r.id, new Set(r.id >= map.first ? r.nb : [])]));
for (const [a, b] of map.joins) { nb.get(a).add(b); nb.get(b).add(a); }
const touches = (a, b) => nb.get(a)?.has(b);
const ROUND = map.frame.x1 - map.frame.x0;
const beside = (r, t) => (Math.abs(t.x - r.x) > ROUND / 2 ? { ...t, x: t.x + (t.x < r.x ? ROUND : -ROUND) } : t);

// ---- Rivers --------------------------------------------------------------------------------------------------

const NEAR = 0.28;
function meet(a, b, c, d) {
  const rx = b[0] - a[0], ry = b[1] - a[1], sx = d[0] - c[0], sy = d[1] - c[1];
  const den = rx * sy - ry * sx;
  if (Math.abs(den) < 1e-9) return null;
  const t = ((c[0] - a[0]) * sy - (c[1] - a[1]) * sx) / den, u = ((c[0] - a[0]) * ry - (c[1] - a[1]) * rx) / den;
  return t > 0 && t < 1 && u >= 0 && u <= 1 ? [a[0] + t * rx, a[1] + t * ry, t] : null;
}
const rivers = beforeRivers.rivers;
const named = (list) => list.map(([a, b, river]) => [byName.get(a), byName.get(b), river]).filter(([a, b]) => a && b);
const struck = new Set(named([...WORLD_RIVER_CUT, ...FAR_RIVER_CUT, ...CITY_RIVER_CUT]).map(([a, b]) => key(a.id, b.id)));
for (const [a, b] of CITY_RIVER_CUT) if (!byName.has(a) || !byName.has(b) || !touches(byName.get(a).id, byName.get(b).id)) console.warn('bad cut', a, b);
const found = new Map();
for (const c of beforeRivers.crossings) if (!isPiece(c[0]) && !isPiece(c[1])) found.set(key(c[0], c[1]), c);
const kept = found.size;
let lostCrossings = 0;
for (const c of beforeRivers.crossings) if (isPiece(c[0]) || isPiece(c[1])) lostCrossings++;
for (const r of all) {
  for (const j of nb.get(r.id)) {
    if (j < r.id || !(isPiece(r.id) || isPiece(j)) || struck.has(key(r.id, j))) continue;
    const t = beside(r, at.get(j));
    let best = null;
    rivers.forEach((rv, k) => {
      const hits = [];
      for (let i = 0; i < rv.pts.length - 1; i++) { const p = meet([r.x, r.y], [t.x, t.y], rv.pts[i], rv.pts[i + 1]); if (p) hits.push(p); }
      const mid = hits.filter((h) => h[2] >= NEAR && h[2] <= 1 - NEAR);
      if (hits.length % 2 === 1 && mid.length && !best) best = { river: k, at: mid[0] };
    });
    if (best) found.set(key(r.id, j), [r.id, j, best.river, r1(best.at[0]), r1(best.at[1])]);
  }
}
const stale = [];
for (const [a, b, river] of [...WORLD_RIVER_ADD, ...FAR_RIVER_ADD, ...CITY_RIVER_ADD]) {
  const ra = byName.get(a), rb = byName.get(b), k = rivers.findIndex((rv) => rv.name === river);
  if (!ra || !rb || k < 0) { console.warn('bad crossing', a, b, river); continue; }
  if (!(isPiece(ra.id) || isPiece(rb.id))) continue;
  // A crossing named for two provinces that a city now lies between is the city's to carry.
  if (!touches(ra.id, rb.id)) { stale.push(`${a} - ${b} (${river})`); continue; }
  const [lo, hi] = ra.id < rb.id ? [ra, rb] : [rb, ra];
  const mx = (lo.x + hi.x) / 2, my = (lo.y + hi.y) / 2;
  let where = [mx, my], bd = 1e9;
  const pts = rivers[k].pts;
  for (let i = 0; i < pts.length - 1; i++) {
    const [ax, ay] = pts[i], [bx, by] = pts[i + 1];
    const t = Math.max(0, Math.min(1, ((mx - ax) * (bx - ax) + (my - ay) * (by - ay)) / ((bx - ax) ** 2 + (by - ay) ** 2)));
    const px = ax + t * (bx - ax), py = ay + t * (by - ay), d = Math.hypot(px - mx, py - my);
    if (d < bd) { bd = d; where = [px, py]; }
  }
  found.set(key(lo.id, hi.id), [lo.id, hi.id, k, r1(where[0]), r1(where[1])]);
}
const carried = found.size;
console.log(`rivers: ${rivers.length} rivers handed over, ${carried} crossings (${kept} as handed over, ${carried - kept} among the pieces of cut provinces, where ${lostCrossings} stood before)`);
if (stale.length) console.log(`named crossings a city now lies between: ${stale.join(', ')}`);
if (process.argv.includes('--list')) for (const [a, b, k] of found.values()) if (isPiece(a) || isPiece(b)) console.log(`  ${rivers[k].name}: ${at.get(a).name} - ${at.get(b).name}`);

// The rivers of the survey: the real courses, the rivers and streams the chart did not have, and their crossings.
const more = moreRivers({ map, all, at, byName, nb, handed: rivers, found, struck, key, beside, land: [...map.land, ...read('campaign-map').land] });
const crossings = [...found.values()].sort((p, q) => p[0] - q[0] || p[1] - q[1]);
fs.writeFileSync(new URL('../src/generated/world-cities-rivers.json', import.meta.url), JSON.stringify({ rivers: more.rivers, crossings }));
{
  const cls = (c) => more.rivers.filter((r) => r.class === c).length;
  console.log(`rivers of the survey: ${more.rivers.length} in all (${cls('great')} great, ${cls('middle')} middle, ${cls('small')} streams), ${crossings.length} crossings: ${carried - more.gone.length} carried on (${more.moved} set on the real course), ${more.fresh.length} new`);
  const pts = more.rivers.reduce((t, r) => t + r.pts.length, 0);
  console.log(`  ${pts} points in the courses; ${more.cutBack.length} courses cut back to the shore${more.cutBack.length ? ` (the furthest: ${[...more.cutBack].sort((p, q) => q.by - p.by).slice(0, 8).map((c) => `${c.name} ${c.by}`).join(', ')})` : ''}`);
  if (more.fromLake.length) console.log(`  ${more.fromLake.length} courses begin where they leave their lake: ${more.fromLake.map((c) => `${c.name} from ${c.lake} (${c.by} cut)`).join(', ')}`);
  if (more.toLake.length) console.log(`  ${more.toLake.length} courses end where they first come to their lake's shore: ${more.toLake.map((c) => `${c.name} at ${c.lake} (${c.by} cut)`).join(', ')}`);
  if (more.taken.length) console.log(`  lower courses taken by the river that falls in: ${more.taken.map((c) => `${c.name} from the ${c.from}`).join(', ')}`);
  if (more.met.length) console.log(`  feet laid where they meet another river: ${more.met.map((c) => `${c.name} to ${c.to} (${c.by} off)`).join(', ')}`);
  if (more.knots.length) console.log(`  ${more.knots.reduce((t, c) => t + c.by, 0)} knots under a unit from the one before dropped, on ${more.knots.length} courses`);
  if (more.runOn.length) console.log(`  ${more.runOn.length} courses run on to the shore the survey ends them short of: ${[...more.runOn].sort((p, q) => q.by - p.by).map((c) => `${c.name} ${c.by}`).join(', ')}`);
  if (more.gone.length) console.log(`crossings handed over on a reach where the river did not run in 1939, struck (${more.gone.length}): ${more.gone.map((o) => `${o.a} - ${o.b} (${o.river}, ${o.far} from its course)`).join(', ')}`);
  if (more.toMouth.length) console.log(`crossings moved from open water to their river's mouth (${more.toMouth.length}): ${more.toMouth.map((o) => `${o.a} - ${o.b} (${o.river}, ${o.far})`).join(', ')}`);
  if (more.ranOn.length) console.log(`rivers run on past the foot the coast gave them, through crossings by the land (${more.ranOn.length}): ${more.ranOn.map((o) => `${o.a} - ${o.b} (${o.river}, ${o.far})`).join(', ')}`);
  if (more.toRoad.length) console.log(`crossings moved onto their river where the road meets it (${more.toRoad.length}): ${more.toRoad.map((o) => `${o.a} - ${o.b} (${o.river}, ${o.far})`).join(', ')}`);
  if (more.adrift.length) console.log(`crossings left off their river as drawn (${more.adrift.length}): ${more.adrift.map((o) => `${o.a} - ${o.b} (${o.river}, ${o.far})`).join(', ')}`);
  if (more.off.length) console.log(`crossings handed over whose river's real course runs over 25 units away (${more.off.length}): ${more.off.map((o) => `${o.a} - ${o.b} (${o.river}, ${o.far})`).join(', ')}`);
  // The new crossings, countable by country and by river: for the rules' measure.
  const byCountry = {}, byRiver = {};
  const rows = more.fresh.map(([a, b, k]) => {
    const ra = at.get(a), rb = at.get(b), rv = more.rivers[k];
    for (const o of new Set([ra.owner, rb.owner])) byCountry[o] = (byCountry[o] ?? 0) + 1;
    byRiver[rv.name] = (byRiver[rv.name] ?? 0) + 1;
    return [ra.name, ra.owner, rb.name, rb.owner, rv.name, rv.class];
  });
  const sorted = (o) => Object.fromEntries(Object.entries(o).sort((p, q) => q[1] - p[1] || (p[0] < q[0] ? -1 : 1)));
  fs.writeFileSync(new URL('../src/generated/world-cities-crossings.json', import.meta.url), JSON.stringify({ carried, fresh: rows.length, byCountry: sorted(byCountry), byRiver: sorted(byRiver), crossings: rows }, null, 1));
  if (process.argv.includes('--list')) for (const [a, , b, , river] of rows) console.log(`  new: ${river}: ${a} - ${b}`);
}

// ---- Railways ------------------------------------------------------------------------------------------------

const straits = new Set(map.links.map(([a, b]) => key(a, b)));
const FERRY = 3;
const noRail = new Set(CITY_NO_RAIL);
const cost = (a, b) => { const t = beside(a, b); return Math.hypot(a.x - t.x, a.y - t.y) * ((GROUND[a.terrain] + GROUND[b.terrain]) / 2) * (straits.has(key(a.id, b.id)) ? FERRY : 1); };
const lines = new Set();
let through = 0;
const reroute = [];
// Lines handed over whose course is laid by hand (or that the historian struck) are not carried from piece to piece.
const relaid = new Set();
for (const [a, b] of CITY_RAIL_RELAID) {
  const p = byName.get(a), q = byName.get(b);
  if (!p || !q || !beforeRails.links.some(([i, j]) => key(i, j) === key(p.id, q.id))) { console.warn('bad relaid line', a, b); continue; }
  relaid.add(key(p.id, q.id));
}
for (const [a, b] of beforeRails.links) {
  if (relaid.has(key(a, b))) continue;
  if (touches(a, b)) { lines.add(key(a, b)); continue; }
  // The two no longer touch: a city lies between them. The line runs by the easiest way through their pieces.
  const pieces = new Set(all.filter((r) => r.id === a || r.id === b || r.of === a || r.of === b).filter((r) => !noRail.has(r.name)).map((r) => r.id));
  const d = new Map([[a, 0]]), prev = new Map(), done = new Set();
  for (;;) {
    let u = null;
    for (const [id, v] of d) if (!done.has(id) && (u === null || v < d.get(u))) u = id;
    if (u === null || u === b) break;
    done.add(u);
    for (const v of nb.get(u)) if (pieces.has(v)) { const w = d.get(u) + cost(at.get(u), at.get(v)); if (w < (d.get(v) ?? Infinity)) { d.set(v, w); prev.set(v, u); } }
  }
  if (!d.has(b)) { console.warn(`no way through for the line ${at.get(a).name} - ${at.get(b).name}`); continue; }
  const names = [at.get(b).name];
  for (let v = b; prev.has(v); v = prev.get(v)) { lines.add(key(v, prev.get(v))); names.push(at.get(prev.get(v)).name); }
  through++;
  reroute.push(names.reverse().join(' - '));
}
// Lines laid by hand and lines struck.
for (const chain of CITY_RAIL_LINES) for (let i = 0; i + 1 < chain.length; i++) {
  const p = byName.get(chain[i]), q = byName.get(chain[i + 1]);
  if (!p || !q || !touches(p.id, q.id)) { console.warn('bad line', chain[i], chain[i + 1]); continue; }
  lines.add(key(p.id, q.id));
}
for (const [a, b] of CITY_RAIL_CUT) {
  const p = byName.get(a), q = byName.get(b);
  if (!p || !q || !lines.has(key(p.id, q.id))) { console.warn('bad cut of a line', a, b); continue; }
  lines.delete(key(p.id, q.id));
}
// Every city is joined to the net by its easiest border with a province of its own country that is on it: a piece
// of its own province first, where one is on the net.
const onNet = () => new Set([...lines].flatMap((k) => k.split(':').map(Number)));
const cities = map.regions.filter((r) => r.of !== undefined && !noRail.has(r.name) && r.terrain !== 'desert');
const joinedUp = [];
for (;;) {
  const on = onNet();
  let best = null;
  for (const c of cities) {
    if (on.has(c.id)) continue;
    for (const j of nb.get(c.id)) {
      const o = at.get(j);
      if (!on.has(j) || o.owner !== c.owner || straits.has(key(c.id, j))) continue;
      const w = cost(c, o) * (family(j) === family(c.id) ? 1 : 1.5);
      if (!best || w < best.w || (w === best.w && c.id < best.c.id)) best = { c, o, w };
    }
  }
  if (!best) break;
  lines.add(key(best.c.id, best.o.id));
  joinedUp.push(`${best.c.name} - ${best.o.name}`);
}

const bridges = new Map(crossings.map(([a, b, , x, y]) => [key(a, b), [x, y]]));
const was = new Map(beforeRails.links.map(([a, b, x, y]) => [key(a, b), [x, y]]));
const edgesOf = (r) => r.polys.flat(1).flatMap((ring) => ring.map((p, i) => [p, ring[(i + 1) % ring.length]]));
function via(a, b0) {
  const k = key(a.id, b0.id);
  // A line between two provinces that were not cut crosses where it crossed.
  if (was.has(k) && !isPiece(a.id) && !isPiece(b0.id)) return was.get(k);
  if (bridges.has(k)) return bridges.get(k);
  const b = beside(a, b0), dx = b.x - b0.x;
  const mx = (a.x + b.x) / 2, my = (a.y + b.y) / 2;
  let best = null, bd = Infinity;
  const theirs = edgesOf(b0).map(([c, d]) => [[c[0] + dx, c[1]], [d[0] + dx, d[1]]]);
  for (const [p, q] of edgesOf(a)) {
    const len = Math.hypot(q[0] - p[0], q[1] - p[1]);
    if (len < 0.05) continue;
    const ux = (q[0] - p[0]) / len, uy = (q[1] - p[1]) / len;
    for (const [c, d] of theirs) {
      const off = (v) => Math.abs((v[0] - p[0]) * uy - (v[1] - p[1]) * ux), along = (v) => (v[0] - p[0]) * ux + (v[1] - p[1]) * uy;
      if (off(c) > 0.3 || off(d) > 0.3) continue;
      const lo = Math.max(0, Math.min(along(c), along(d))), hi = Math.min(len, Math.max(along(c), along(d)));
      if (hi - lo < 0.2) continue;
      const t = Math.max(lo, Math.min(hi, (mx - p[0]) * ux + (my - p[1]) * uy));
      const x = p[0] + t * ux, y = p[1] + t * uy, dist = Math.hypot(x - mx, y - my);
      if (dist < bd) { bd = dist; best = [r1(x), r1(y)]; }
    }
  }
  return best ?? [r1(mx), r1(my)];
}
const links = [...lines].map((k) => k.split(':').map(Number)).map(([a, b]) => [a, b, ...via(at.get(a), at.get(b))]).sort((p, q) => p[0] - q[0] || p[1] - q[1]);
fs.writeFileSync(new URL('../src/generated/world-cities-rails.json', import.meta.url), JSON.stringify({ links }) + '\n');
const on = onNet();
const wasOn = new Set(beforeRails.links.flatMap(([a, b]) => [a, b]));
const byHand = new Set(CITY_RAIL_RELAID.flat());
const offNow = [...wasOn].filter((id) => !on.has(id)).map((id) => at.get(id).name);
const dropped = offNow.filter((n) => !byHand.has(n));
console.log(`rails: ${links.length} lines (${beforeRails.links.length} as handed over, ${through} of them now running through a city), ${joinedUp.length} cities joined to the net; cities off it: ${map.regions.filter((r) => r.of !== undefined && !on.has(r.id)).map((r) => r.name).join(', ') || 'none'}`);
if (relaid.size) console.log(`lines handed over that are laid by hand or struck: ${CITY_RAIL_RELAID.map(([a, b]) => `${a} - ${b}`).join(', ')}${offNow.some((n) => byHand.has(n)) ? `; off the net with them: ${offNow.filter((n) => byHand.has(n)).join(', ')}` : ''}`);
if (dropped.length) console.log(`! provinces that were on the net and are off it: ${dropped.join(', ')}`);
if (process.argv.includes('--list')) { console.log(`through a city: ${reroute.join('; ')}`); console.log(`joined: ${joinedUp.join('; ')}`); }
