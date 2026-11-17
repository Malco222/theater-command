// The lakes the chart was not handed over with, cut out of the chart with the cities (called by carve-cities.mjs
// when the provinces are ready to be written). The chart as handed over is not touched.
//
// A lake (data/campaign/more-lakes.mjs) is cut out of the ground of every province it lies in and out of the land,
// and listed with the chart's lakes, each with its water ('fresh', 'salt' or 'pan'). No province loses its chief
// place: a town on the shore keeps a foot of ground (the shore is drawn round it). No border is struck: two
// provinces that touched still touch, by rule, even where a lake now lies along their border.
import polygonClipping from 'polygon-clipping';
import { MORE_LAKES } from '../data/campaign/more-lakes.mjs';
import { OLD_LAKE_WATER } from '../data/campaign/detail.mjs';

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
  const lines = [], drowned = [], shores = [], thinned = [], apart = [], offLand = [];
  const key = (a, b) => (a < b ? `${a}:${b}` : `${b}:${a}`);
  const touched = new Map();
  let cutArea = 0;
  for (const [name, water, level, shore] of MORE_LAKES) {
    if (names.has(name)) { warn(`! two lakes are called ${name}`); continue; }
    names.add(name);
    const ring = shore.map(([lat, lon]) => proj([lon, lat]));
    let lake = [[[...ring, ring[0]]]];
    const box = boxOf(ring);
    if (core.regions.some((r) => r.polys.some((poly) => boxesMeet(boxOf(poly[0]), box) && ring.some(([x, y]) => inRing(poly[0], x, y))))) { warn(`! ${name} lies on the map of Europe: not cut`); continue; }
    const near = out.regions.filter((r) => r.polys.some((poly) => boxesMeet(boxOf(poly[0]), box, FOOT)));
    // A chief place in the water or on the very shore keeps a foot of ground.
    for (const r of near) {
      if (!(inRing(ring, r.x, r.y) || toRing(ring, r.x, r.y) < SHORE)) continue;
      try { lake = polygonClipping.difference(lake, [disc(r.x, r.y, FOOT)]); } catch (e) { warn(`! ${name}: could not draw the shore round ${r.name}: ${e.message}`); }
      shores.push(`${r.name} (${name})`);
    }
    lake = lake.sort((a, b) => polyArea(open([b])[0] ?? [[]]) - polyArea(open([a])[0] ?? [[]])).slice(0, 1);
    if (!lake.length) { warn(`! nothing is left of ${name}`); continue; }
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
    if (meets(was, touched.get(j)?.was ?? o.polys) && !meets(r.polys, o.polys)) apart.push(`${r.name} - ${o.name}`);
  }
  out.lakes = lakes;
  const notes = [`lakes: ${lakes.length} on the chart (${lakes.length - lines.length} as handed over, ${lines.length} cut now: ${Math.round(cutArea)} square units of ground)`];
  if (shores.length) notes.push(`the shore is drawn round ${shores.length} chief places: ${shores.join(', ')}`);
  if (thinned.length) notes.push(`provinces that lost over a third of their ground, or are left under 700 square units: ${thinned.join('; ')}`);
  if (apart.length) notes.push(`borders that now lie under a lake (the two still touch by rule): ${apart.join('; ')}`);
  if (offLand.length) notes.push(`! lakes that lie mostly off the charted ground: ${offLand.join('; ')}`);
  if (drowned.length) notes.push(`! provinces a lake would have drowned (left as they were): ${drowned.join('; ')}`);
  return { notes, lines };
}
