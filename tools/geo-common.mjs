// Shared helpers for turning Natural Earth land polygons (public domain, via the
// world-atlas package) into the small map files the prototypes ship with.
import fs from 'node:fs';
import { feature } from 'topojson-client';
import polygonClipping from 'polygon-clipping';

export function loadLand(res) {
  const topo = JSON.parse(fs.readFileSync(new URL(`../node_modules/world-atlas/land-${res}.json`, import.meta.url)));
  const fc = feature(topo, topo.objects.land);
  // Collect every polygon as [ring, ring...] in lon/lat.
  const polys = [];
  for (const f of fc.features ?? [fc]) {
    const g = f.geometry;
    if (g.type === 'Polygon') polys.push(g.coordinates);
    else if (g.type === 'MultiPolygon') polys.push(...g.coordinates);
  }
  return polys.map((p) => p.map(unwrapRing));
}

// Eurasia's outline crosses the 180th meridian in Chukotka, where the longitudes jump from
// +180 to -180. Left alone, that jump is a false edge running right across the map between
// 65 and 69 degrees north, which turns Lapland into sea. Carry the ring on past 180 instead.
function unwrapRing(ring) {
  let shift = 0, jumped = false, sum = 0;
  const out = ring.map(([x, y], i) => {
    if (i > 0) {
      const d = x - ring[i - 1][0];
      if (d > 180) { shift -= 360; jumped = true; } else if (d < -180) { shift += 360; jumped = true; }
    }
    sum += x + shift;
    return [x + shift, y];
  });
  if (!jumped) return ring;
  const back = -360 * Math.round(sum / ring.length / 360);
  return out.map(([x, y]) => [x + back, y]);
}

export function clipToBox(polys, [w, s, e, n]) {
  const box = [[[w, s], [e, s], [e, n], [w, n], [w, s]]];
  const out = [];
  for (const p of polys) {
    // quick reject
    let minx = 1e9, maxx = -1e9, miny = 1e9, maxy = -1e9;
    for (const [x, y] of p[0]) { if (x < minx) minx = x; if (x > maxx) maxx = x; if (y < miny) miny = y; if (y > maxy) maxy = y; }
    if (maxx < w || minx > e || maxy < s || miny > n) continue;
    let r;
    try { r = polygonClipping.intersection([p], [box]); } catch { continue; }
    for (const poly of r) out.push(poly);
  }
  return out;
}

// Douglas-Peucker on a closed ring (first point repeated at the end).
export function simplifyRing(ring, tol) {
  if (ring.length < 5) return ring;
  const pts = ring.slice(0, -1);
  const keep = new Uint8Array(pts.length);
  const n = pts.length;
  // split at the two farthest-apart points to handle the closed ring
  let a = 0, b = 0, best = -1;
  for (let i = 0; i < n; i++) {
    const d = (pts[i][0] - pts[0][0]) ** 2 + (pts[i][1] - pts[0][1]) ** 2;
    if (d > best) { best = d; b = i; }
  }
  keep[a] = keep[b] = 1;
  const dp = (i0, i1) => {
    const stack = [[i0, i1]];
    while (stack.length) {
      const [s, e] = stack.pop();
      let maxD = 0, idx = -1;
      const [x1, y1] = pts[s % n], [x2, y2] = pts[e % n];
      const dx = x2 - x1, dy = y2 - y1, len2 = dx * dx + dy * dy || 1e-12;
      for (let i = s + 1; i < e; i++) {
        const [px, py] = pts[i % n];
        const t = Math.max(0, Math.min(1, ((px - x1) * dx + (py - y1) * dy) / len2));
        const d = Math.hypot(px - (x1 + t * dx), py - (y1 + t * dy));
        if (d > maxD) { maxD = d; idx = i; }
      }
      if (maxD > tol && idx >= 0) { keep[idx % n] = 1; stack.push([s, idx], [idx, e]); }
    }
  };
  dp(a, b); dp(b, n + a);
  const out = pts.filter((_, i) => keep[i]);
  if (out.length < 3) return null;
  out.push(out[0]);
  return out;
}

export function ringArea(r) {
  let a = 0;
  for (let i = 0; i < r.length - 1; i++) a += r[i][0] * r[i + 1][1] - r[i + 1][0] * r[i][1];
  return a / 2;
}
