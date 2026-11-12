// Lays the railway net of the campaign map: a line to every province outside the desert by the easiest country
// (a spanning tree), main lines between the cities that lie nearest each other, and whatever it takes to join
// each country's own net where its provinces touch. Each line is written as
// [region, region, x, y]: the two provinces it joins and the point where it crosses from one into the other.
// Run after build-geo-campaign and build-rivers.
import fs from 'node:fs';
import { GROUND, NEAREST, ADD, CUT } from '../data/campaign/rails.mjs';

const map = JSON.parse(fs.readFileSync(new URL('../src/generated/campaign-map.json', import.meta.url), 'utf8'));
const R = map.regions;
const byName = new Map(R.map((r) => [r.name, r]));
const key = (a, b) => (a < b ? `${a}:${b}` : `${b}:${a}`);
// None runs in the desert, and a strait is crossed by train ferry only where an island has no other line.
const straits = new Set(map.links.map(([a, b]) => key(a, b)));
const FERRY = 3;
const railable = (r) => r.terrain !== 'desert';
const cost = (a, b) => Math.hypot(a.x - b.x, a.y - b.y) * ((GROUND[a.terrain] + GROUND[b.terrain]) / 2) * (straits.has(key(a.id, b.id)) ? FERRY : 1);
const edges = [];
for (const r of R) for (const j of r.nb) if (j > r.id && railable(r) && railable(R[j])) edges.push({ a: r.id, b: j, w: cost(r, R[j]) });
edges.sort((p, q) => p.w - q.w || p.a - q.a || p.b - q.b);

const lines = new Set();
// 1. A line to every province: the cheapest tree over each stretch of land.
const boss = R.map((r) => r.id);
const find = (x) => { while (boss[x] !== x) { boss[x] = boss[boss[x]]; x = boss[x]; } return x; };
for (const e of edges) { const p = find(e.a), q = find(e.b); if (p !== q) { boss[p] = q; lines.add(key(e.a, e.b)); } }

// 2. Main lines: every city to the cities nearest it, by the easiest way.
const city = (r) => railable(r) && (r.flags?.cap || r.terrain === 'urban' || r.fac >= 2 || r.vp >= 2);
const nbs = R.map(() => []);
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
const cities = R.filter(city);
for (const c of cities) {
  const { d, prev } = paths(c.id);
  const near = cities.filter((o) => o.id !== c.id && d[o.id] < Infinity).sort((p, q) => d[p.id] - d[q.id] || p.id - q.id).slice(0, NEAREST);
  for (const o of near) for (let at = o.id; prev[at] >= 0; at = prev[at]) lines.add(key(at, prev[at]));
}

// 3. No country's own net is left in pieces where its provinces touch: a country built its lines to serve itself,
// not to run through a neighbour. The pieces are joined by the easiest border between them.
const joined = [];
for (const owner of new Set(R.map((r) => r.owner))) {
  for (;;) {
    const piece = new Map();
    for (const r of R) {
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

// Where each line crosses from one province into the next: over the bridge where a river runs between them,
// otherwise at the point of their common border nearest the straight road.
const bridges = new Map(JSON.parse(fs.readFileSync(new URL('../src/generated/campaign-rivers.json', import.meta.url), 'utf8')).crossings.map(([a, b, , x, y]) => [key(a, b), [x, y]]));
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
const links = [...lines].map((k) => k.split(':').map(Number)).sort((p, q) => p[0] - q[0] || p[1] - q[1]).map(([a, b]) => [a, b, ...via(R[a], R[b])]);
fs.writeFileSync(new URL('../src/generated/campaign-rails.json', import.meta.url), JSON.stringify({ links }) + '\n');
const on = new Set(links.flatMap(([a, b]) => [a, b]));
console.log(`rails: ${links.length} lines of ${edges.length} borders, ${on.size} provinces on the net, ${cities.length} cities; off the net: ${R.filter((r) => !on.has(r.id)).map((r) => r.name).join(', ') || 'none'}`);
if (joined.length) console.log(`joined within a country: ${joined.join(', ')}`);
if (process.argv.includes('--list')) for (const [a, b] of links) console.log(`${R[a].name} - ${R[b].name}`);
