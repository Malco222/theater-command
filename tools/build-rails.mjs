// Lays the railway net of the campaign map: a line to every province outside the desert by the easiest country
// (a spanning tree), main lines between the cities that lie nearest each other, and whatever it takes to join
// each country's own net where its provinces touch. Each line is written as
// [region, region, x, y]: the two provinces it joins and the point where it crosses from one into the other.
// Run after build-geo-campaign and build-rivers.
// With 'world' it lays the net of the wider world the same way, on the provinces of world-map.json (run after
// build-geo-world and build-rivers world), written to world-rails.json. With 'far' it lays the net of the far
// side on the new provinces of world-next.json (run after build-geo-far and build-rivers far) and writes
// world-next-rails.json: today's lines, then the new ones. The Pacific slice's net is laid as the new world's is;
// the globe slice's is laid by hand (FAR_RAIL_LINES), and may cross the old sheet's edge into today's provinces.
import fs from 'node:fs';
import { GROUND, NEAREST } from '../data/campaign/rails.mjs';

const far = process.argv.includes('far');
const world = !far && process.argv.includes('world');
const { ADD, CUT, NO_RAIL, LINES = [] } = far
  ? await import('../data/campaign/far.mjs').then((m) => ({ ADD: m.FAR_RAIL_ADD, CUT: m.FAR_RAIL_CUT, NO_RAIL: new Set(m.FAR_NO_RAIL), LINES: m.FAR_RAIL_LINES }))
  : world
    ? await import('../data/campaign/world.mjs').then((m) => ({ ADD: m.WORLD_RAIL_ADD, CUT: m.WORLD_RAIL_CUT, NO_RAIL: new Set(m.WORLD_NO_RAIL) }))
    : await import('../data/campaign/rails.mjs').then((m) => ({ ADD: m.ADD, CUT: m.CUT, NO_RAIL: new Set() }));
const file = far ? 'world-next' : world ? 'world' : 'campaign';
const map = JSON.parse(fs.readFileSync(new URL(`../src/generated/${far ? 'world-next' : `${file}-map`}.json`, import.meta.url), 'utf8'));
// The provinces as a list, and by their numbers: the new world's are numbered on from Europe's. On the far side
// the net is laid over the new provinces only, and today's lines are kept as they stand in world-rails.json.
const L = far ? map.regions.filter((r) => r.id >= map.first + map.stages[0]) : map.regions;
const before = far ? JSON.parse(fs.readFileSync(new URL('../src/generated/world-rails.json', import.meta.url), 'utf8')).links : [];
const R = [];
for (const r of L) R[r.id] = r;
const byName = new Map(L.map((r) => [r.name, r]));
const key = (a, b) => (a < b ? `${a}:${b}` : `${b}:${a}`);
// None runs in the desert, and a strait is crossed by train ferry only where an island has no other line.
const straits = new Set(map.links.map(([a, b]) => key(a, b)));
const FERRY = 3;
// (On the far side the net is grown over the Pacific slice only: the globe slice's lines are laid by hand below.)
const byHand = far ? map.first + map.stages[1] : Infinity;
const railable = (r) => r.id < byHand && r.terrain !== 'desert' && !NO_RAIL.has(r.name);
const cost = (a, b) => Math.hypot(a.x - b.x, a.y - b.y) * ((GROUND[a.terrain] + GROUND[b.terrain]) / 2) * (straits.has(key(a.id, b.id)) ? FERRY : 1);
const edges = [];
for (const r of L) for (const j of r.nb) if (j > r.id && R[j] && railable(r) && railable(R[j])) edges.push({ a: r.id, b: j, w: cost(r, R[j]) });
edges.sort((p, q) => p.w - q.w || p.a - q.a || p.b - q.b);

const lines = new Set();
// 1. A line to every province: the cheapest tree over each stretch of land.
const boss = Array.from({ length: R.length }, (_, i) => i);
const find = (x) => { while (boss[x] !== x) { boss[x] = boss[boss[x]]; x = boss[x]; } return x; };
for (const e of edges) { const p = find(e.a), q = find(e.b); if (p !== q) { boss[p] = q; lines.add(key(e.a, e.b)); } }

// 2. Main lines: every city to the cities nearest it, by the easiest way.
const city = (r) => railable(r) && (r.flags?.cap || r.terrain === 'urban' || r.fac >= 2 || r.vp >= 2);
const nbs = Array.from({ length: R.length }, () => []);
for (const e of edges) { nbs[e.a].push([e.b, e.w]); nbs[e.b].push([e.a, e.w]); }
function paths(from) {
  const d = new Float64Array(R.length).fill(Infinity), prev = new Int32Array(R.length).fill(-1), done = new Uint8Array(R.length);
  d[from] = 0;
  for (;;) {
    let u = -1;
    for (let i = 0; i < R.length; i++) if (!done[i] && d[i] < Infinity && (u < 0 || d[i] < d[u])) u = i;
    if (u < 0) break;
    done[u] = 1;
    for (const [v, w] of nbs[u]) if (d[u] + w < d[v]) { d[v] = d[u] + w; prev[v] = u; }
  }
  return { d, prev };
}
const cities = L.filter(city);
for (const c of cities) {
  const { d, prev } = paths(c.id);
  const near = cities.filter((o) => o.id !== c.id && d[o.id] < Infinity).sort((p, q) => d[p.id] - d[q.id] || p.id - q.id).slice(0, NEAREST);
  for (const o of near) for (let at = o.id; prev[at] >= 0; at = prev[at]) lines.add(key(at, prev[at]));
}

// 3. No country's own net is left in pieces where its provinces touch: a country built its lines to serve itself,
// not to run through a neighbour. The pieces are joined by the easiest border between them.
const joined = [];
for (const owner of new Set(L.map((r) => r.owner))) {
  for (;;) {
    const piece = new Map();
    for (const r of L) {
      if (r.owner !== owner || !railable(r) || piece.has(r.id)) continue;
      const q = [r.id];
      piece.set(r.id, r.id);
      for (let i = 0; i < q.length; i++) for (const j of R[q[i]].nb) if (!piece.has(j) && R[j].owner === owner && lines.has(key(q[i], j))) { piece.set(j, r.id); q.push(j); }
    }
    const e = edges.find((x) => R[x.a].owner === owner && R[x.b].owner === owner && piece.get(x.a) !== piece.get(x.b));
    if (!e) break;
    lines.add(key(e.a, e.b));
    joined.push(`${R[e.a].name} - ${R[e.b].name}`);
  }
}

// 4. The corrections.
const named = (list, what) => list.map(([a, b]) => { const p = byName.get(a), q = byName.get(b); if (!p || !q || !p.nb.includes(q.id)) { console.warn(`bad ${what}`, a, b); return null; } return key(p.id, q.id); }).filter(Boolean);
for (const k of named(ADD, 'add')) lines.add(k);
for (const k of named(CUT, 'cut')) lines.delete(k);

// 5. The lines laid by hand: each a chain of neighbouring provinces. A province of today's map in a chain carries
// the line across the old sheet's edge, where the new province borders it ('joins').
const across = [];
if (LINES.length) {
  const olds = [...JSON.parse(fs.readFileSync(new URL('../src/generated/campaign-map.json', import.meta.url), 'utf8')).regions, ...map.regions.filter((r) => r.id < map.first + map.stages[0])];
  const oldByName = new Map(olds.map((r) => [r.name, r]));
  const joins = new Set((map.joins ?? []).map(([a, b]) => key(a, b)));
  for (const chain of LINES) {
    for (let i = 0; i + 1 < chain.length; i++) {
      const p = byName.get(chain[i]), q = byName.get(chain[i + 1]);
      if (p && q && p.nb.includes(q.id)) { lines.add(key(p.id, q.id)); continue; }
      const o = oldByName.get(chain[i]) ?? oldByName.get(chain[i + 1]), n = p ?? q;
      if (o && n && !(p && q) && joins.has(key(o.id, n.id))) { across.push([o, n]); continue; }
      console.warn('bad line', chain[i], chain[i + 1]);
    }
  }
}

// Where each line crosses from one province into the next: over the bridge where a river runs between them,
// otherwise at the point of their common border nearest the straight road.
const bridges = new Map(JSON.parse(fs.readFileSync(new URL(`../src/generated/${file}-rivers.json`, import.meta.url), 'utf8')).crossings.map(([a, b, , x, y]) => [key(a, b), [x, y]]));
const r1 = (v) => Math.round(v * 10) / 10;
function via(a, b) {
  const bridge = bridges.get(key(a.id, b.id));
  if (bridge) return bridge;
  const mx = (a.x + b.x) / 2, my = (a.y + b.y) / 2;
  const theirs = new Set(b.polys.flat(2).map((p) => p.join(',')));
  let best = [mx, my], bd = Infinity;
  const tryPoint = (x, y) => { const d = Math.hypot(x - mx, y - my); if (d < bd) { bd = d; best = [x, y]; } };
  for (const ring of a.polys.flat(1)) {
    for (let i = 0; i < ring.length; i++) {
      const p = ring[i], q = ring[(i + 1) % ring.length];
      const sp = theirs.has(p.join(',')), sq = theirs.has(q.join(','));
      // The middle of a shared stretch of border is a better place for the line than its corners, where three provinces meet.
      if (sp && sq) tryPoint((p[0] + q[0]) / 2, (p[1] + q[1]) / 2);
    }
  }
  return [r1(best[0]), r1(best[1])];
}
// A line across the old sheet's edge crosses where the two provinces' borders run together: the shapes of the two
// maps share no corners, so the stretch they have in common is found edge by edge.
function viaEdge(o, n) {
  const bridge = bridges.get(key(o.id, n.id));
  if (bridge) return bridge;
  const mx = (o.x + n.x) / 2, my = (o.y + n.y) / 2;
  let best = null, bd = Infinity;
  const edgesOf = (r) => r.polys.flat(1).flatMap((ring) => ring.map((p, i) => [p, ring[(i + 1) % ring.length]]));
  const theirs = edgesOf(o);
  for (const [p, q] of edgesOf(n)) {
    const len = Math.hypot(q[0] - p[0], q[1] - p[1]);
    if (len < 0.05) continue;
    const ux = (q[0] - p[0]) / len, uy = (q[1] - p[1]) / len;
    for (const [c, d] of theirs) {
      // How far c and d lie off the line of pq, and where along it.
      const off = (v) => Math.abs((v[0] - p[0]) * uy - (v[1] - p[1]) * ux), along = (v) => (v[0] - p[0]) * ux + (v[1] - p[1]) * uy;
      if (off(c) > 0.3 || off(d) > 0.3) continue;
      const lo = Math.max(0, Math.min(along(c), along(d))), hi = Math.min(len, Math.max(along(c), along(d)));
      if (hi - lo < 0.2) continue;
      const t = Math.max(lo, Math.min(hi, (mx - p[0]) * ux + (my - p[1]) * uy));
      const x = p[0] + t * ux, y = p[1] + t * uy, dist = Math.hypot(x - mx, y - my);
      if (dist < bd) { bd = dist; best = [r1(x), r1(y)]; }
    }
  }
  if (!best) console.warn('no common border found for', o.name, n.name);
  return best ?? [r1(mx), r1(my)];
}
const links = [
  ...across.map(([o, n]) => [o.id, n.id, ...viaEdge(o, n)]),
  ...[...lines].map((k) => k.split(':').map(Number)).map(([a, b]) => [a, b, ...via(R[a], R[b])]),
].sort((p, q) => p[0] - q[0] || p[1] - q[1]);
fs.writeFileSync(new URL(`../src/generated/${file}-rails.json`, import.meta.url), JSON.stringify({ links: [...before, ...links] }) + '\n');
const on = new Set(links.flatMap(([a, b]) => [a, b]));
console.log(`rails: ${links.length} lines of ${edges.length} borders, ${on.size} provinces on the net, ${cities.length} cities; off the net: ${L.filter((r) => r.id < byHand && !on.has(r.id)).map((r) => r.name).join(', ') || 'none'}`);
if (LINES.length) console.log(`laid by hand: ${links.filter(([a, b]) => b >= byHand).length} lines (${across.length} across the old sheet's edge: ${across.map(([o, n]) => `${o.name} - ${n.name}`).join(', ')}); on the net by hand: ${L.filter((r) => r.id >= byHand && on.has(r.id)).length} of ${L.filter((r) => r.id >= byHand).length} provinces`);
if (joined.length) console.log(`joined within a country: ${joined.join(', ')}`);
if (process.argv.includes('--list')) for (const [a, b] of links) console.log(`${R[a]?.name ?? a} - ${R[b].name}`);
