// The lakes the chart was not handed over with, cut out of the chart with the cities (called by carve-cities.mjs
// when the provinces are ready to be written). The chart as handed over is not touched.
//
// A lake (data/campaign/more-lakes.mjs) is cut out of the ground of every province it lies in and out of the land,
// and listed with the chart's lakes, each with its water ('fresh', 'salt' or 'pan'). A dry salt pan is ground, not
// a gap (design director, 17 November): it is listed for the painter and cut out of nothing, its provinces keep
// their ground and touch across it, and an army can cross it. No province loses its chief
// place: a town on the shore keeps a foot of ground (the shore is drawn round it). A border that lay on dry land
// and now lies wholly under a lake is struck (design director, 17 November): the two no longer touch, and the tool
// counts the steps of the way round by land.
//
// Every shore is made smooth (tools/lake-shore.mjs: no arm under 2 units, no corner under 30 degrees), and the
// eleven lakes the chart was handed over with are redrawn from the same survey (REDRAWN_LAKES): where the new shore
// reaches past the old, the water is cut out of the provinces and the land as for a new lake; where the old shore
// reached past the new, the ground is given back to the land and shared among the provinces beside it, each cell of
// it to the province nearest. No border is struck for the eleven (the design director): one the new water would
// cover whole is reported and left.
import polygonClipping from 'polygon-clipping';
import { MORE_LAKES, REDRAWN_LAKES } from '../data/campaign/more-lakes.mjs';
import { OLD_LAKE_WATER } from '../data/campaign/detail.mjs';
import { smoothShore } from './lake-shore.mjs';

const ringArea = (ring) => { let a = 0; for (let k = 0; k < ring.length; k++) { const [x1, y1] = ring[k], [x2, y2] = ring[(k + 1) % ring.length]; a += x1 * y2 - x2 * y1; } return Math.abs(a) / 2; };
const polyArea = (poly) => ringArea(poly[0]) - poly.slice(1).reduce((t, h) => t + ringArea(h), 0);
const boxOf = (ring) => { let a = 1e9, b = 1e9, c = -1e9, d = -1e9; for (const [x, y] of ring) { if (x < a) a = x; if (y < b) b = y; if (x > c) c = x; if (y > d) d = y; } return [a, b, c, d]; };
const boxesMeet = (p, q, pad = 0) => p[0] <= q[2] + pad && q[0] <= p[2] + pad && p[1] <= q[3] + pad && q[1] <= p[3] + pad;
const inRing = (ring, x, y) => { let inside = false; for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) { const [xi, yi] = ring[i], [xj, yj] = ring[j]; if (yi > y !== yj > y && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) inside = !inside; } return inside; };
const toRing = (ring, x, y) => { let best = Infinity; for (let i = 0; i < ring.length; i++) { const [ax, ay] = ring[i], [bx, by] = ring[(i + 1) % ring.length], L2 = (bx - ax) ** 2 + (by - ay) ** 2; const t = L2 ? Math.max(0, Math.min(1, ((x - ax) * (bx - ax) + (y - ay) * (by - ay)) / L2)) : 0; best = Math.min(best, Math.hypot(ax + t * (bx - ax) - x, ay + t * (by - ay) - y)); } return best; };
const closed = (poly) => poly.map((ring) => [...ring, ring[0]]);

/**
 * @param o.out    the chart about to be written: its regions' polys, its land and its lakes are changed here
 * @param o.core   the map of Europe (a lake must not lie on it)
 * @param o.proj   [lon, lat] to the chart
 * @returns lines to print
 */
export function cutLakes({ out, core, proj, warn }) {
  const r1 = (v) => Math.round(v * 10) / 10;
  const open = (multi) => multi.map((poly) => poly.map((ring) => ring.slice(0, -1).map(([x, y]) => [r1(x), r1(y)])).filter((ring) => ring.length >= 3)).filter((poly) => poly.length && polyArea(poly) >= 1);
  const SHORE = 3, FOOT = 4.5;
  const disc = (x, y, r) => { const ring = []; for (let k = 0; k < 16; k++) ring.push([x + r * Math.cos((k * Math.PI) / 8), y + r * Math.sin((k * Math.PI) / 8)]); return [[...ring, ring[0]]]; };
  const lakes = out.lakes.map((l) => ({ ...l, water: OLD_LAKE_WATER[l.name] ?? 'fresh' }));
  const names = new Set(lakes.map((l) => l.name));
  const lines = [], drowned = [], shores = [], thinned = [], apart = [], offLand = [], pans = [];
  const key = (a, b) => (a < b ? `${a}:${b}` : `${b}:${a}`);
  const touched = new Map();
  let cutArea = 0;
  const snap = (multi) => multi.map((poly) => poly.map((ring) => ring.map(([x, y]) => [r1(x), r1(y)])));
  // (Where the clipper stalls on edges that fall just so, the second shape is nudged by a hundredth of a unit and
  // the answer set back on the grid of a tenth.)
  const nudge = (multi, dx, dy) => multi.map((poly) => poly.map((ring) => ring.map(([x, y]) => [x + dx, y + dy])));
  const clip = (op, a, b) => {
    for (const [dx, dy] of [[0, 0], [0.013, 0.007], [-0.011, 0.017], [0.019, -0.013]]) { try { return snap(polygonClipping[op](a, dx || dy ? nudge(b, dx, dy) : b)); } catch (e) { if (dx === 0.019) throw e; } }
    return [];
  };
  const largest = (multi) => [...multi].sort((a, b) => polyArea(b.map((r) => r.slice(0, -1))) - polyArea(a.map((r) => r.slice(0, -1)))).slice(0, 1);
  const redrawn = [], unredrawn = [], partedOld = [];
  // ---- The eleven handed over, redrawn ----
  for (const [name, shore] of REDRAWN_LAKES) {
    const L = lakes.find((l) => l.name === name);
    if (!L) { warn(`! no lake called ${name} to redraw`); continue; }
    const ring = shore.map(([lat, lon]) => proj([lon, lat]));
    // (The clipper can stall on a shore whose points fall just so: the shore is then thinned to another grain.)
    const tries = [0.3, 0.35, 0.25, 0.4];
    for (const [t, tol] of tries.entries()) {
    let lake = largest(smoothShore([[[...ring, ring[0]]]], 1, { tol }));
    if (!lake.length) { unredrawn.push(`${name} (nothing left once smooth)`); break; }
    const oldRing = L.outline, old = [[[...oldRing, oldRing[0]]]];
    const ob = boxOf(oldRing), nb0 = boxOf(lake[0][0]), box = [Math.min(ob[0], nb0[0]), Math.min(ob[1], nb0[1]), Math.max(ob[2], nb0[2]), Math.max(ob[3], nb0[3])];
    const near = out.regions.filter((r) => r.polys.some((poly) => boxesMeet(boxOf(poly[0]), box, FOOT)));
    const feet = [];
    for (const r of near) if (inRing(lake[0][0], r.x, r.y) || toRing(lake[0][0], r.x, r.y) < SHORE) { lake = snap(polygonClipping.difference(lake, [disc(r.x, r.y, FOOT)])); feet.push(r.name); }
    lake = largest(lake);
    // All of it is worked out first and written only if every step goes through.
    let step = 'the new water';
    try {
      const add = snap(polygonClipping.difference(lake, old)), give = snap(polygonClipping.difference(old, lake));
      const polys = new Map(near.map((r) => [r.id, r.polys.map((p) => p.map((q) => [...q, q[0]]))]));
      // The water that is new: out of every province it lies on.
      for (const r of near) {
        step = `cutting ${r.name}`;
        const after = clip('difference', polys.get(r.id), add);
        if (!after.some((poly) => inRing(poly[0].slice(0, -1), r.x, r.y))) throw new Error(`${r.name} would lose its chief place`);
        polys.set(r.id, after);
      }
      // The ground given back: each cell of a fifth of a unit to the province whose ground is nearest, the shares
      // cut out of the ground exactly.
      const H = 0.2;
      for (const piece of give) {
        const pr = piece[0].slice(0, -1), pb = boxOf(pr);
        const gap = (r) => Math.min(...r.polys.filter((poly) => boxesMeet(boxOf(poly[0]), pb, 12)).flatMap((poly) => [...poly[0].map(([x, y]) => toRing(pr, x, y)), ...pr.map(([x, y]) => toRing(poly[0], x, y))]), Infinity);
        const gaps = near.map((r) => [r, gap(r)]).sort((a, b) => a[1] - b[1]);
        let cand = gaps.filter(([, d]) => d < 3).map(([r]) => r);
        if (!cand.length && gaps.length && gaps[0][1] < 12) cand = [gaps[0][0]];
        if (!cand.length) throw new Error(`ground given back beside no province (${Math.round(polyArea(piece.map((q) => q.slice(0, -1))))} square units)`);
        let owners;
        if (cand.length === 1) owners = new Map([[cand[0].id, [piece]]]);
        else {
          const segs = cand.map((r) => r.polys.flatMap((poly) => poly[0].map((p, i) => [p, poly[0][(i + 1) % poly[0].length]])).filter(([a, b]) => boxesMeet(boxOf([a, b]), pb, 3)));
          const x0 = Math.floor(pb[0] / H) * H - H, y0 = Math.floor(pb[1] / H) * H - H;
          const nx = Math.ceil((pb[2] - x0) / H) + 2, ny = Math.ceil((pb[3] - y0) / H) + 2;
          const who = new Int16Array(nx * ny);
          for (let j = 0; j < ny; j++) for (let i = 0; i < nx; i++) {
            const x = x0 + (i + 0.5) * H, y = y0 + (j + 0.5) * H;
            let best = 0, bd = Infinity;
            segs.forEach((ss, k) => { for (const [[ax, ay], [bx, by]] of ss) { const L2 = (bx - ax) ** 2 + (by - ay) ** 2, t = L2 ? Math.max(0, Math.min(1, ((x - ax) * (bx - ax) + (y - ay) * (by - ay)) / L2)) : 0, d = Math.hypot(ax + t * (bx - ax) - x, ay + t * (by - ay) - y); if (d < bd) { bd = d; best = k; } } });
            who[j * nx + i] = best;
          }
          owners = new Map();
          cand.forEach((r, k) => {
            const boxes = [];
            for (let j = 0; j < ny; j++) for (let i = 0; i < nx; i++) {
              if (who[j * nx + i] !== k) continue;
              let e = i; while (e + 1 < nx && who[j * nx + e + 1] === k) e++;
              boxes.push([[[i, j], [e + 1, j], [e + 1, j + 1], [i, j + 1], [i, j]]]);
              i = e;
            }
            if (!boxes.length) return;
            let level = boxes;
            while (level.length > 1) { const next = []; for (let q = 0; q < level.length; q += 8) next.push(polygonClipping.union(...level.slice(q, q + 8))); level = next; }
            const region = snap(level[0].map((poly) => poly.map((rg) => rg.map(([i, j]) => [x0 + i * H, y0 + j * H]))));
            step = `the share of ${r.name}`;
            const share = clip('intersection', [piece], region);
            if (share.length) owners.set(r.id, share);
          });
        }
        for (const [id, share] of owners) { step = `giving ground to ${near.find((x) => x.id === id).name}`; polys.set(id, clip('union', polys.get(id), share)); }
      }
      // The land: the old hole filled, the new water cut.
      const landNear = out.land.map((poly, i) => [poly, i]).filter(([poly]) => boxesMeet(boxOf(poly[0]), box, FOOT));
      step = 'the land';
      let land = snap(polygonClipping.union(...landNear.map(([poly]) => closed(poly)), old));
      land = open(polygonClipping.difference(land, lake));
      // Write it.
      for (const r of near) {
        const after = open(polys.get(r.id));
        if (!after.some((poly) => inRing(poly[0], r.x, r.y) && !poly.slice(1).some((h) => inRing(h, r.x, r.y)))) throw new Error(`${r.name} would lose its chief place`);
        r._next = after;
      }
      // (A border the new water covers whole is reported, not struck.)
      for (const r of near) for (const j of r.nb) {
        if (j < r.id) continue;
        const o = near.find((x) => x.id === j);
        if (!o) continue;
        const pts = r.polys.flatMap((p) => p[0]).filter(([x, y]) => o.polys.some((q) => toRing(q[0], x, y) < 0.35));
        if (pts.length >= 2 && pts.every(([x, y]) => inRing(lake[0][0], x, y))) partedOld.push(`${r.name} - ${o.name} (${name})`);
      }
      for (const r of near) { r.polys = r._next; delete r._next; }
      const drop = new Set(landNear.map(([, i]) => i));
      out.land = [...out.land.filter((_, i) => !drop.has(i)), ...land];
      const outline = open(lake)[0][0];
      Object.assign(L, { outline, x: r1(outline.reduce((t, p) => t + p[0], 0) / outline.length), y: r1(outline.reduce((t, p) => t + p[1], 0) / outline.length) });
      for (const f of feet) shores.push(`${f} (${name})`);
      redrawn.push(`${name} ${Math.round(ringArea(oldRing))} to ${Math.round(ringArea(outline))} (${oldRing.length} to ${outline.length} points; ${Math.round(add.reduce((u, p) => u + polyArea(p.map((q) => q.slice(0, -1))), 0))} new water, ${Math.round(give.reduce((u, p) => u + polyArea(p.map((q) => q.slice(0, -1))), 0))} given back)`);
      break;
    } catch (e) {
      for (const r of near) delete r._next;
      if (t < tries.length - 1) continue;
      unredrawn.push(`${name} (${step}: ${e.message})`);
    }
    }
  }
  for (const [name, water, level, shore] of MORE_LAKES) {
    if (names.has(name)) { warn(`! two lakes are called ${name}`); continue; }
    names.add(name);
    const ring = shore.map(([lat, lon]) => proj([lon, lat]));
    let lake = [[[...ring, ring[0]]]];
    const box = boxOf(ring);
    if (core.regions.some((r) => r.polys.some((poly) => boxesMeet(boxOf(poly[0]), box) && ring.some(([x, y]) => inRing(poly[0], x, y))))) { warn(`! ${name} lies on the map of Europe: not cut`); continue; }
    const near = out.regions.filter((r) => r.polys.some((poly) => boxesMeet(boxOf(poly[0]), box, FOOT)));
    if (water === 'pan') {
      try { lake = largest(smoothShore(lake, 1)); } catch (e) { warn(`! ${name}: could not make the shore smooth: ${e.message}`); }
      const outline = open(lake)[0][0];
      const on = near.filter((r) => r.polys.some((poly) => outline.some(([x, y]) => inRing(poly[0], x, y))));
      lakes.push({ name, level, x: r1(outline.reduce((t, p) => t + p[0], 0) / outline.length), y: r1(outline.reduce((t, p) => t + p[1], 0) / outline.length), outline, water });
      pans.push(name);
      lines.push(`  ${name.padEnd(20)} ${water.padEnd(5)} ${String(Math.round(polyArea([outline]))).padStart(5)} square units painted on the ground of ${on.map((r) => r.name).join(', ') || 'no province'}`);
      continue;
    }
    // A chief place in the water or on the very shore keeps a foot of ground.
    for (const r of near) {
      if (!(inRing(ring, r.x, r.y) || toRing(ring, r.x, r.y) < SHORE)) continue;
      try { lake = polygonClipping.difference(lake, [disc(r.x, r.y, FOOT)]); } catch (e) { warn(`! ${name}: could not draw the shore round ${r.name}: ${e.message}`); }
      shores.push(`${r.name} (${name})`);
    }
    lake = lake.sort((a, b) => polyArea(open([b])[0] ?? [[]]) - polyArea(open([a])[0] ?? [[]])).slice(0, 1);
    if (!lake.length) { warn(`! nothing is left of ${name}`); continue; }
    // The shore made smooth (and the shore drawn again round a chief place the smoothing brought near the water).
    try {
      lake = largest(smoothShore(lake, 1));
      for (const r of near) if (inRing(lake[0][0].slice(0, -1), r.x, r.y) || toRing(lake[0][0].slice(0, -1), r.x, r.y) < SHORE - 1) lake = largest(snap(polygonClipping.difference(lake, [disc(r.x, r.y, FOOT)])));
    } catch (e) { warn(`! ${name}: could not make the shore smooth: ${e.message}`); }
    if (!lake.length) { warn(`! nothing is left of ${name} once smooth`); continue; }
    const size = polyArea(open(lake)[0]);
    let taken = 0;
    const from = [];
    for (const r of near) {
      const before = r.polys.reduce((t, p) => t + polyArea(p), 0), was = r.polys;
      let after;
      try { after = open(polygonClipping.difference(r.polys.map(closed), lake)); } catch (e) { warn(`! ${name}: could not cut ${r.name}: ${e.message}`); continue; }
      const left = after.reduce((t, p) => t + polyArea(p), 0);
      if (before - left < 0.5) continue;
      if (!after.length || !after.some((poly) => inRing(poly[0], r.x, r.y) && !poly.slice(1).some((h) => inRing(h, r.x, r.y)))) { drowned.push(`${r.name} (${name})`); continue; }
      r.polys = after;
      taken += before - left;
      from.push(r);
      (touched.get(r.id) ?? touched.set(r.id, { r, before, was, lakes: [] }).get(r.id)).lakes.push(name);
    }
    if (taken < size * 0.6) offLand.push(`${name} (${Math.round((100 * taken) / size)}% of it on charted ground)`);
    for (let i = 0; i < out.land.length; i++) {
      const poly = out.land[i];
      if (!boxesMeet(boxOf(poly[0]), box)) continue;
      try { const after = open(polygonClipping.difference([closed(poly)], lake)); if (after.length) out.land.splice(i, 1, ...after), (i += after.length - 1); } catch (e) { warn(`! ${name}: could not cut the land: ${e.message}`); }
    }
    const outline = open(lake)[0][0];
    lakes.push({ name, level, x: r1(outline.reduce((s, p) => s + p[0], 0) / outline.length), y: r1(outline.reduce((s, p) => s + p[1], 0) / outline.length), outline, water });
    cutArea += taken;
    lines.push(`  ${name.padEnd(20)} ${water.padEnd(5)} ${String(Math.round(size)).padStart(5)} square units out of ${from.map((r) => r.name).join(', ') || 'no province'}`);
  }
  // What the lakes took: a province that lost much of its ground, and two that no longer meet on dry land.
  for (const { r, before, lakes: ls } of touched.values()) {
    const left = r.polys.reduce((t, p) => t + polyArea(p), 0);
    if (left < before * 0.65 || left < 700) thinned.push(`${r.name} ${Math.round(before)} to ${Math.round(left)} (${ls.join(', ')})`);
  }
  const near1 = (pa, pb) => pa.some((p) => p.some((ra) => pb.some((q) => q.some((rb) => boxesMeet(boxOf(ra), boxOf(rb), 0.5) && ra.some(([x, y]) => toRing(rb, x, y) < 0.35)))));
  const meets = (pa, pb) => near1(pa, pb) || near1(pb, pa);
  const seen = new Set();
  for (const { r, was } of touched.values()) for (const j of r.nb) {
    const o = out.regions.find((x) => x.id === j);
    if (!o || seen.has(key(r.id, j))) continue;
    seen.add(key(r.id, j));
    // (Only a border that lay on dry land before the lakes were cut.)
    if (meets(was, touched.get(j)?.was ?? o.polys) && !meets(r.polys, o.polys)) apart.push([r, o]);
  }
  for (const [r, o] of apart) { r.nb = r.nb.filter((j) => j !== o.id); o.nb = o.nb.filter((j) => j !== r.id); }
  const byId = new Map(out.regions.map((r) => [r.id, r]));
  const steps = (a, b) => {
    const d = new Map([[a, 0]]), queue = [a];
    for (let i = 0; i < queue.length; i++) { const u = queue[i]; if (u === b) return d.get(u); for (const v of byId.get(u)?.nb ?? []) if (!d.has(v)) { d.set(v, d.get(u) + 1); queue.push(v); } }
    return Infinity;
  };
  const struck = apart.map(([r, o]) => { const n = steps(r.id, o.id); if (n > 3) warn(`! ${r.name} and ${o.name} no longer touch across a lake, and the way round by land is ${n === Infinity ? 'cut' : `${n} steps`}`); return `${r.name} - ${o.name} (${n} steps round by land)`; });
  out.lakes = lakes;
  const notes = [`lakes: ${lakes.length} on the chart (${lakes.length - lines.length} as handed over, ${lines.length - pans.length} cut now: ${Math.round(cutArea)} square units of ground; ${pans.length} dry pans painted on the ground and cut out of nothing: ${pans.join(', ')})`];
  if (shores.length) notes.push(`the shore is drawn round ${shores.length} chief places: ${shores.join(', ')}`);
  if (thinned.length) notes.push(`provinces that lost over a third of their ground, or are left under 700 square units: ${thinned.join('; ')}`);
  if (struck.length) notes.push(`borders struck, the whole of them under a lake: ${struck.join('; ')}`);
  if (redrawn.length) notes.push(`the eleven lakes handed over, redrawn from the survey (${redrawn.length}): ${redrawn.join('; ')}`);
  if (unredrawn.length) notes.push(`! lakes handed over left as they were: ${unredrawn.join('; ')}`);
  if (partedOld.length) notes.push(`! borders a redrawn lake covers whole (left as borders, for the design director): ${partedOld.join('; ')}`);
  if (offLand.length) notes.push(`! lakes that lie mostly off the charted ground: ${offLand.join('; ')}`);
  if (drowned.length) notes.push(`! provinces a lake would have drowned (left as they were): ${drowned.join('; ')}`);
  return { notes, lines };
}
