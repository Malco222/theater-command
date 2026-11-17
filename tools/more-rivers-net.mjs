// The rivers of the survey on the chart with the cities (called by build-cities-net.mjs, after the crossings handed
// over have been carried on).
//
//  - The 60 rivers handed over keep their numbers, their names and every crossing they had. Each is drawn on its
//    real course (data/campaign/more-rivers.mjs, REAL_COURSES), and its crossings are set on that course.
//  - The rivers and streams the chart did not have follow them (MORE_RIVERS), in the order of that list.
//  - A river is 'great', 'middle' or 'small'. What is drawn as a river is a river in the rules: a great or a middle
//    river makes a crossing wherever it runs between the chief places of two provinces that touch, near the middle
//    of the road between them, as the rivers of Europe do. A small one is a stream: it is drawn, and it is no
//    obstacle.
//  - No crossing handed over is struck or moved to another river. A border that had one keeps it.
import { REAL_COURSES, MORE_RIVERS } from '../data/campaign/more-rivers.mjs';
import { MORE_RIVER_CUT, MORE_RIVER_ADD } from '../data/campaign/detail.mjs';

const K = Math.cos((50 * Math.PI) / 180) * 40;
const NEAR = 0.28;

function meet(a, b, c, d) {
  const rx = b[0] - a[0], ry = b[1] - a[1], sx = d[0] - c[0], sy = d[1] - c[1];
  const den = rx * sy - ry * sx;
  if (Math.abs(den) < 1e-9) return null;
  const t = ((c[0] - a[0]) * sy - (c[1] - a[1]) * sx) / den, u = ((c[0] - a[0]) * ry - (c[1] - a[1]) * rx) / den;
  return t > 0 && t < 1 && u >= 0 && u <= 1 ? [a[0] + t * rx, a[1] + t * ry, t] : null;
}
function nearestOn(pts, x, y) {
  let best = { d: Infinity, p: [x, y] };
  for (let i = 0; i + 1 < pts.length; i++) {
    const [ax, ay] = pts[i], [bx, by] = pts[i + 1], L2 = (bx - ax) ** 2 + (by - ay) ** 2;
    const t = L2 ? Math.max(0, Math.min(1, ((x - ax) * (bx - ax) + (y - ay) * (by - ay)) / L2)) : 0;
    const px = ax + t * (bx - ax), py = ay + t * (by - ay), d = Math.hypot(px - x, py - y);
    if (d < best.d) best = { d, p: [px, py] };
  }
  return best;
}

/**
 * @param o.map      the chart with the cities
 * @param o.all      every province, Europe's first
 * @param o.at       province by id
 * @param o.byName   province by name
 * @param o.nb       every border of the chart, both ways
 * @param o.handed   the rivers handed over ({ name, pts, ll })
 * @param o.found    the crossings carried on so far, by key(a, b): added to here
 * @param o.struck   borders whose crossing the lists strike
 */
export function moreRivers({ map, all, at, byName, nb, handed, found, struck, key, beside }) {
  const ROUND = map.frame.x1 - map.frame.x0;
  const r1 = (v) => Math.round(v * 10) / 10, r2 = (v) => Math.round(v * 1000) / 1000;
  const x0 = map.frame.x0;
  // A course on the chart: the first point where the chart has it, the rest running on from it without a leap
  // (a river that crosses the chart's edge runs off the sheet, as the Yukon handed over does).
  const course = (ll) => {
    const out = [];
    let prev = null;
    for (const [lat, lon] of ll) {
      let x = (lon + 11) * K;
      if (prev === null) { while (x < x0) x += ROUND; while (x >= x0 + ROUND) x -= ROUND; }
      else { while (x - prev > ROUND / 2) x -= ROUND; while (prev - x > ROUND / 2) x += ROUND; }
      prev = x;
      out.push([x, (71.4 - lat) * 40]);
    }
    // (And it lies on the turn of the chart that holds most of it.)
    const inside = (w) => out.filter(([x]) => x + w >= x0 && x + w < x0 + ROUND).length;
    const w = [0, ROUND, -ROUND].sort((p, q) => inside(q) - inside(p))[0];
    return out.map(([x, y]) => [r2(x + w), r2(y)]);
  };
  const rivers = handed.map((rv, k) => {
    const [name, cls, into, ll, more] = REAL_COURSES[k];
    if (name !== rv.name) throw new Error(`more-rivers.mjs is out of step with the rivers handed over at ${k}: ${name}, ${rv.name}`);
    return { name, pts: ll ? course(ll) : rv.pts, ll: ll ?? rv.ll, class: cls, into, ...(more?.seasonal ? { seasonal: true } : {}), real: !!ll };
  });
  for (const [name, cls, into, ll, more] of MORE_RIVERS) {
    if (rivers.some((r) => r.name === name)) throw new Error(`two rivers called ${name}`);
    rivers.push({ name, pts: course(ll), ll, class: cls, into, ...(more?.seasonal ? { seasonal: true } : {}), real: true });
  }
  for (const r of rivers) {
    let a = Infinity, b = -Infinity, c = Infinity, d = -Infinity;
    for (const [x, y] of r.pts) { if (x < a) a = x; if (x > b) b = x; if (y < c) c = y; if (y > d) d = y; }
    r.box = [a, b, c, d];
  }
  /** Where the road between two chief places meets a river: every meeting, on whichever turn of the chart. */
  const hitsOf = (rv, ax, ay, bx, by) => {
    const hits = [];
    for (const w of [0, ROUND, -ROUND]) {
      const [a, b, c, d] = rv.box;
      if (Math.max(ax, bx) < a + w || Math.min(ax, bx) > b + w || Math.max(ay, by) < c || Math.min(ay, by) > d) continue;
      for (let i = 0; i + 1 < rv.pts.length; i++) {
        const p = rv.pts[i], q = rv.pts[i + 1];
        const m = meet([ax, ay], [bx, by], [p[0] + w, p[1]], [q[0] + w, q[1]]);
        if (m) hits.push(m);
      }
    }
    return hits;
  };

  // The crossings carried on: each set on the real course of its river, where the road between the two meets it
  // or the course runs near; left where it was handed over otherwise.
  const off = [];
  let moved = 0;
  for (const [k, c] of found) {
    const rv = rivers[c[2]];
    if (!rv.real) continue;
    const a = at.get(c[0]), b = beside(a, at.get(c[1]));
    const hits = hitsOf(rv, a.x, a.y, b.x, b.y).sort((p, q) => Math.abs(p[2] - 0.5) - Math.abs(q[2] - 0.5));
    if (hits.length) { found.set(k, [c[0], c[1], c[2], r1(hits[0][0]), r1(hits[0][1])]); moved++; continue; }
    let best = { d: Infinity };
    for (const w of [0, ROUND, -ROUND]) { const n = nearestOn(rv.pts, c[3] - w, c[4]); if (n.d < best.d) best = { d: n.d, p: [n.p[0] + w, n.p[1]] }; }
    if (best.d <= 40) { found.set(k, [c[0], c[1], c[2], r1(best.p[0]), r1(best.p[1])]); moved++; }
    if (best.d > 25) off.push({ a: a.name, b: at.get(c[1]).name, river: rv.name, far: Math.round(best.d) });
  }

  // The crossings the real courses make, on every border outside the map of Europe that has none.
  const named = (list) => list.map(([a, b, river]) => [byName.get(a), byName.get(b), river]);
  for (const [a, b, river] of [...MORE_RIVER_CUT, ...MORE_RIVER_ADD]) if (!byName.has(a) || !byName.has(b) || !nb.get(byName.get(a).id)?.has(byName.get(b).id) || (river && !rivers.some((r) => r.name === river))) console.warn('bad crossing in detail.mjs', a, b, river);
  const cut = new Set(named(MORE_RIVER_CUT).filter(([a, b]) => a && b).map(([a, b]) => key(a.id, b.id)));
  const straits = new Set(map.links.map(([a, b]) => key(a, b)));
  const order = rivers.map((r, k) => k).filter((k) => rivers[k].class !== 'small').sort((p, q) => (rivers[p].class === 'great' ? 0 : 1) - (rivers[q].class === 'great' ? 0 : 1) || p - q);
  const before = new Set(found.keys());
  const fresh = [];
  for (const r of all) {
    for (const j of nb.get(r.id)) {
      const k = key(r.id, j);
      if (j < r.id || (r.id < map.first && j < map.first) || found.has(k) || struck.has(k) || cut.has(k) || straits.has(k)) continue;
      const t = beside(r, at.get(j));
      for (const n of order) {
        const hits = hitsOf(rivers[n], r.x, r.y, t.x, t.y);
        const mid = hits.filter((h) => h[2] >= NEAR && h[2] <= 1 - NEAR);
        if (hits.length % 2 === 1 && mid.length) { found.set(k, [r.id, j, n, r1(mid[0][0]), r1(mid[0][1])]); break; }
      }
    }
  }
  for (const [a, b, river] of named(MORE_RIVER_ADD)) {
    const n = rivers.findIndex((r) => r.name === river);
    if (!a || !b || n < 0 || !nb.get(a.id).has(b.id)) continue;
    const [lo, hi] = a.id < b.id ? [a, b] : [b, a], t = beside(lo, hi);
    let best = { d: Infinity };
    for (const w of [0, ROUND, -ROUND]) { const m = nearestOn(rivers[n].pts, (lo.x + t.x) / 2 - w, (lo.y + t.y) / 2); if (m.d < best.d) best = { d: m.d, p: [m.p[0] + w, m.p[1]] }; }
    found.set(key(lo.id, hi.id), [lo.id, hi.id, n, r1(best.p[0]), r1(best.p[1])]);
  }
  for (const [k, c] of found) if (!before.has(k)) fresh.push(c);
  fresh.sort((p, q) => p[0] - q[0] || p[1] - q[1]);
  for (const r of rivers) { delete r.box; delete r.real; }
  return { rivers, fresh, moved, off };
}
