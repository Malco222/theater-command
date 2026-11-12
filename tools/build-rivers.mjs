// Builds the rivers of the campaign map: projects each course in data/campaign/rivers.mjs onto the map, and finds
// the crossings: pairs of neighbouring regions whose centres lie on opposite banks. Run after build-geo-campaign.
import fs from 'node:fs';
import { RIVERS, CUT, ADD } from '../data/campaign/rivers.mjs';

const map = JSON.parse(fs.readFileSync(new URL('../src/generated/campaign-map.json', import.meta.url), 'utf8'));
// The same projection as tools/build-geo-campaign.mjs.
const W = -11, N = 71.4, SCALE = 40, KX = Math.cos((50 * Math.PI) / 180);
const proj = ([lat, lon]) => [(lon - W) * KX * SCALE, (N - lat) * SCALE];
const r1 = (v) => Math.round(v * 10) / 10;
/** How near the middle of the road between two regions a river must cross it to count as running between them. */
const NEAR = 0.28;

/** Where segment ab meets segment cd, or null. */
function meet(a, b, c, d) {
  const rx = b[0] - a[0], ry = b[1] - a[1], sx = d[0] - c[0], sy = d[1] - c[1];
  const den = rx * sy - ry * sx;
  if (Math.abs(den) < 1e-9) return null;
  const t = ((c[0] - a[0]) * sy - (c[1] - a[1]) * sx) / den, u = ((c[0] - a[0]) * ry - (c[1] - a[1]) * rx) / den;
  return t > 0 && t < 1 && u >= 0 && u <= 1 ? [a[0] + t * rx, a[1] + t * ry, t] : null;
}

const byName = new Map(map.regions.map((r) => [r.name, r]));
const rivers = RIVERS.map(([name, course]) => ({ name, ll: course, pts: course.map(proj).map(([x, y]) => [r1(x), r1(y)]) }));
const key = (a, b) => (a < b ? `${a}:${b}` : `${b}:${a}`);
const cut = new Set(CUT.map(([a, b]) => { if (!byName.has(a) || !byName.has(b)) console.warn('bad cut', a, b); return key(byName.get(a)?.id, byName.get(b)?.id); }));
const found = new Map();
for (const r of map.regions) {
  for (const j of r.nb) {
    if (j < r.id) continue;
    const t = map.regions[j];
    if (cut.has(key(r.id, j))) continue;
    // A road that crosses a river an even number of times ends on the bank it started from.
    let best = null;
    rivers.forEach((rv, k) => {
      const hits = [];
      for (let i = 0; i < rv.pts.length - 1; i++) { const p = meet([r.x, r.y], [t.x, t.y], rv.pts[i], rv.pts[i + 1]); if (p) hits.push(p); }
      // The border between two regions lies midway between their centres. A river that crosses the road near one
      // end runs through that region's own country, not along the border, and makes no crossing between the two.
      const mid = hits.filter((h) => h[2] >= NEAR && h[2] <= 1 - NEAR);
      if (hits.length % 2 === 1 && mid.length && !best) best = { river: k, at: mid[0] };
    });
    if (best) found.set(key(r.id, j), [r.id, j, best.river, r1(best.at[0]), r1(best.at[1])]);
  }
}
for (const [a, b, river] of ADD) {
  const ra = byName.get(a), rb = byName.get(b), k = rivers.findIndex((rv) => rv.name === river);
  if (!ra || !rb || k < 0 || !ra.nb.includes(rb.id)) { console.warn('bad crossing', a, b, river); continue; }
  const [lo, hi] = ra.id < rb.id ? [ra, rb] : [rb, ra];
  // The bridge is drawn where the river comes nearest the middle of the road between the two.
  const mx = (lo.x + hi.x) / 2, my = (lo.y + hi.y) / 2;
  let at = [mx, my], bd = 1e9;
  const pts = rivers[k].pts;
  for (let i = 0; i < pts.length - 1; i++) {
    const [ax, ay] = pts[i], [bx, by] = pts[i + 1];
    const t = Math.max(0, Math.min(1, ((mx - ax) * (bx - ax) + (my - ay) * (by - ay)) / ((bx - ax) ** 2 + (by - ay) ** 2)));
    const px = ax + t * (bx - ax), py = ay + t * (by - ay), d = Math.hypot(px - mx, py - my);
    if (d < bd) { bd = d; at = [px, py]; }
  }
  found.set(key(lo.id, hi.id), [lo.id, hi.id, k, r1(at[0]), r1(at[1])]);
}
const crossings = [...found.values()].sort((p, q) => p[0] - q[0] || p[1] - q[1]);
const out = { rivers: rivers.map((rv) => ({ name: rv.name, pts: rv.pts, ll: rv.ll })), crossings };
fs.writeFileSync(new URL('../src/generated/campaign-rivers.json', import.meta.url), JSON.stringify(out));
const per = rivers.map((rv, k) => `${rv.name} ${crossings.filter((c) => c[2] === k).length}`);
console.log(`rivers: ${rivers.length} rivers, ${crossings.length} crossings (${per.join(', ')})`);
if (process.argv.includes('--list')) for (const [a, b, k] of crossings) console.log(`  ${rivers[k].name}: ${map.regions[a].name} (${map.regions[a].owner}) - ${map.regions[b].name} (${map.regions[b].owner})`);
