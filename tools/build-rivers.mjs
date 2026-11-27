// Builds the rivers of the campaign map: projects each course in data/campaign/rivers.mjs onto the map, and finds
// the crossings: pairs of neighbouring regions whose centres lie on opposite banks. Run after build-geo-campaign.
// With 'world' it does the same for the wider world: the rivers of data/campaign/world.mjs on the provinces of
// world-map.json (run after build-geo-world), written to world-rivers.json.
// With 'far' it adds the rivers of the far side (data/campaign/far.mjs) on the new provinces of world-next.json
// (run after build-geo-far), and writes world-next-rivers.json: today's rivers and crossings as they stand in
// world-rivers.json, then the new ones.
import fs from 'node:fs';

const far = process.argv.includes('far');
const world = !far && process.argv.includes('world');
const { RIVERS, CUT, ADD } = far
  ? await import('../data/campaign/far.mjs').then((m) => ({ RIVERS: m.FAR_RIVERS, CUT: m.FAR_RIVER_CUT, ADD: m.FAR_RIVER_ADD }))
  : world
    ? await import('../data/campaign/world.mjs').then((m) => ({ RIVERS: m.WORLD_RIVERS, CUT: m.WORLD_RIVER_CUT, ADD: m.WORLD_RIVER_ADD }))
    : await import('../data/campaign/rivers.mjs');
const map = JSON.parse(fs.readFileSync(new URL(`../src/generated/${far ? 'world-next' : world ? 'world-map' : 'campaign-map'}.json`, import.meta.url), 'utf8'));
// On the far side only the new provinces are looked at: today's crossings are kept as they are.
const fresh = far ? map.first + map.stages[0] : -1;
const before = far ? JSON.parse(fs.readFileSync(new URL('../src/generated/world-rivers.json', import.meta.url), 'utf8')) : { rivers: [], crossings: [] };
// A province by its number: the new world's are numbered on from Europe's. On the far side a new province can
// border one of the map of Europe across the old sheet's edge ('joins'), so Europe's provinces are known too.
const europe = far ? JSON.parse(fs.readFileSync(new URL('../src/generated/campaign-map.json', import.meta.url), 'utf8')).regions.map((r) => ({ ...r, nb: [] })) : [];
const all = [...europe, ...map.regions];
const at = new Map(all.map((r) => [r.id, r]));
// The same projection as tools/build-geo-campaign.mjs. The far side counts its longitudes eastward from the
// chart's west edge (141 W), as tools/build-geo-far.mjs does.
const W = -11, N = 71.4, SCALE = 40, KX = Math.cos((50 * Math.PI) / 180);
const proj = ([lat, lon]) => [((far && lon < -141 ? lon + 360 : lon) - W) * KX * SCALE, (N - lat) * SCALE];
/** The far side's chart runs the full circle: a province across its edge is met the short way round. */
const ROUND = 360 * KX * SCALE;
const beside = (r, t) => (far && Math.abs(t.x - r.x) > ROUND / 2 ? { ...t, x: t.x + (t.x < r.x ? ROUND : -ROUND) } : t);
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

const byName = new Map(all.map((r) => [r.name, r]));
// A course is drawn unbroken: where it crosses the chart's edge (141 W) its points follow each other round, and
// the whole course lies at the end of the chart where most of it runs.
function unbroken(course) {
  if (!far) return course.map(proj);
  const lons = [];
  for (const [, lon] of course) { let l = lon; if (lons.length) { const last = lons[lons.length - 1]; while (l - last > 180) l -= 360; while (last - l > 180) l += 360; } lons.push(l); }
  const mean = lons.reduce((s, l) => s + l, 0) / lons.length, turn = mean < -141 ? 360 : mean >= 219 ? -360 : 0;
  return course.map(([lat], i) => [(lons[i] + turn - W) * KX * SCALE, (N - lat) * SCALE]);
}
const rivers = RIVERS.map(([name, course]) => ({ name, ll: course, pts: unbroken(course).map(([x, y]) => [r1(x), r1(y)]) }));
const key = (a, b) => (a < b ? `${a}:${b}` : `${b}:${a}`);
const cut = new Set(CUT.map(([a, b]) => { if (!byName.has(a) || !byName.has(b)) console.warn('bad cut', a, b); return key(byName.get(a)?.id, byName.get(b)?.id); }));
const found = new Map();
// On the far side a new province's borders with today's provinces stand apart in the chart ('joins').
const joined = new Map();
for (const [a, b] of map.joins ?? []) (joined.get(a) ?? joined.set(a, []).get(a)).push(b);
for (const r of all) {
  for (const j of [...r.nb, ...(joined.get(r.id) ?? [])]) {
    if (j < r.id || j < fresh) continue;
    if (!at.has(j)) continue;
    const t = beside(r, at.get(j));
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
  if (!ra || !rb || k < 0 || !(ra.nb.includes(rb.id) || (joined.get(ra.id) ?? []).includes(rb.id) || (joined.get(rb.id) ?? []).includes(ra.id))) { console.warn('bad crossing', a, b, river); continue; }
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
const out = {
  rivers: [...before.rivers, ...rivers.map((rv) => ({ name: rv.name, pts: rv.pts, ll: rv.ll }))],
  crossings: [...before.crossings, ...crossings.map(([a, b, k, x, y]) => [a, b, k + before.rivers.length, x, y])],
};
fs.writeFileSync(new URL(`../src/generated/${far ? 'world-next' : world ? 'world' : 'campaign'}-rivers.json`, import.meta.url), JSON.stringify(out));
const per = rivers.map((rv, k) => `${rv.name} ${crossings.filter((c) => c[2] === k).length}`);
console.log(`rivers: ${rivers.length} rivers, ${crossings.length} crossings (${per.join(', ')})`);
if (process.argv.includes('--list')) for (const [a, b, k] of crossings) console.log(`  ${rivers[k].name}: ${at.get(a).name} (${at.get(a).owner}) - ${at.get(b).name} (${at.get(b).owner})`);
