// A lake's shore made smooth for the chart (the art director, 17 November): no arm of water thinner than a river's
// line, and no corner sharper than 30 degrees. The shape is shrunk by a radius and grown back by it (what the eye
// calls an opening): every needle and thread of water thinner than twice the radius goes, every point of the shore
// is rounded, and nothing is added that was not water before. It is worked on a fine grid (a fifth of a unit), and
// the shore traced from the grid is thinned to the chart's grain, with any corner still sharper than the bar taken
// out.
import polygonClipping from 'polygon-clipping';

const H = 0.2;
const ringArea = (ring) => { let a = 0; for (let k = 0; k < ring.length; k++) { const [x1, y1] = ring[k], [x2, y2] = ring[(k + 1) % ring.length]; a += x1 * y2 - x2 * y1; } return a / 2; };
const closed = (ring) => [...ring, ring[0]];
const open = (ring) => (ring.length > 1 && ring[0][0] === ring[ring.length - 1][0] && ring[0][1] === ring[ring.length - 1][1] ? ring.slice(0, -1) : ring);

/** Thin a closed ring: no point dropped that stands more than tol from the line drawn without it. */
function thin(ring, tol) {
  if (ring.length <= 8) return ring;
  let far = 0, fd = -1;
  for (let i = 1; i < ring.length; i++) { const d = Math.hypot(ring[i][0] - ring[0][0], ring[i][1] - ring[0][1]); if (d > fd) { fd = d; far = i; } }
  const keep = new Set([0, far]);
  const walk = (a, b) => {
    const stack = [[a, b]];
    while (stack.length) {
      const [s, e] = stack.pop();
      let at = -1, worst = tol;
      const [px, py] = ring[s], [qx, qy] = ring[e % ring.length], dx = qx - px, dy = qy - py, L2 = dx * dx + dy * dy;
      for (let i = s + 1; i < e; i++) {
        const [x, y] = ring[i];
        const t = L2 ? Math.max(0, Math.min(1, ((x - px) * dx + (y - py) * dy) / L2)) : 0;
        const d = Math.hypot(x - px - t * dx, y - py - t * dy);
        if (d > worst) { worst = d; at = i; }
      }
      if (at >= 0) { keep.add(at); stack.push([s, at], [at, e]); }
    }
  };
  walk(0, far);
  walk(far, ring.length);
  return [...keep].filter((i) => i < ring.length).sort((x, y) => x - y).map((i) => ring[i]);
}
/** The angle at each corner of a ring, in degrees (180 is straight). */
export const corners = (ring) => ring.map((b, i) => {
  const a = ring[(i + ring.length - 1) % ring.length], c = ring[(i + 1) % ring.length];
  const v = [a[0] - b[0], a[1] - b[1]], w = [c[0] - b[0], c[1] - b[1]], L = Math.hypot(...v) * Math.hypot(...w);
  return L ? (Math.acos(Math.max(-1, Math.min(1, (v[0] * w[0] + v[1] * w[1]) / L))) * 180) / Math.PI : 0;
});
/** Take out every corner sharper than the bar, the sharpest first. */
function blunt(ring, bar) {
  let r = ring;
  for (let guard = 0; guard < 1000 && r.length > 4; guard++) {
    const a = corners(r);
    let i = -1, worst = bar;
    a.forEach((v, k) => { if (v < worst) { worst = v; i = k; } });
    if (i < 0) break;
    r = r.filter((_, k) => k !== i);
  }
  return r;
}

/**
 * @param multi  the lake as polygon-clipping's multipolygon (rings closed; holes are islands)
 * @param r      half the thinnest arm kept (1 unit: arms under 2 go)
 * @param o.tol  the grain the shore is thinned to; o.bar the sharpest corner left; o.r1 rounds a point
 * @returns the smooth lake as a multipolygon with closed rings (its largest sheet first), or [] if nothing is left
 */
export function smoothShore(multi, r = 1, { tol = 0.3, bar = 30, r1 = (v) => Math.round(v * 10) / 10 } = {}) {
  const rings = multi.flatMap((poly) => poly.map(open));
  let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
  for (const ring of rings) for (const [x, y] of ring) { x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y); }
  const R = Math.round(r / H), pad = R + 2;
  x0 -= pad * H; y0 -= pad * H;
  const nx = Math.ceil((x1 - x0) / H) + pad, ny = Math.ceil((y1 - y0) / H) + pad;
  // Water on the grid: a cell is water when its centre is (even-odd over every ring, so islands stay land).
  const wet = new Uint8Array(nx * ny);
  for (let j = 0; j < ny; j++) {
    const y = y0 + (j + 0.5) * H, xs = [];
    for (const ring of rings) for (let k = 0; k < ring.length; k++) {
      const [ax, ay] = ring[k], [bx, by] = ring[(k + 1) % ring.length];
      if (ay > y !== by > y) xs.push(ax + ((y - ay) * (bx - ax)) / (by - ay));
    }
    xs.sort((a, b) => a - b);
    for (let k = 0; k + 1 < xs.length; k += 2) {
      const i0 = Math.max(0, Math.ceil((xs[k] - x0) / H - 0.5)), i1 = Math.min(nx - 1, Math.floor((xs[k + 1] - x0) / H - 0.5));
      for (let i = i0; i <= i1; i++) wet[j * nx + i] = 1;
    }
  }
  const disc = [];
  for (let dj = -R; dj <= R; dj++) for (let di = -R; di <= R; di++) if (di * di + dj * dj <= R * R) disc.push([di, dj]);
  // Shrink: a cell stays water only if the whole disc round it is water.
  const core = new Uint8Array(nx * ny);
  for (let j = R; j < ny - R; j++) for (let i = R; i < nx - R; i++) {
    if (!wet[j * nx + i]) continue;
    let all = 1;
    for (const [di, dj] of disc) if (!wet[(j + dj) * nx + i + di]) { all = 0; break; }
    core[j * nx + i] = all;
  }
  // Grow back: every cell within the radius of what is left (and that was water) is water again.
  const kept = new Uint8Array(nx * ny);
  for (let j = R; j < ny - R; j++) for (let i = R; i < nx - R; i++) {
    if (!core[j * nx + i]) continue;
    const inner = core[(j - 1) * nx + i] && core[(j + 1) * nx + i] && core[j * nx + i - 1] && core[j * nx + i + 1];
    if (inner) { kept[j * nx + i] = 1; continue; }
    for (const [di, dj] of disc) kept[(j + dj) * nx + i + di] = 1;
  }
  for (let k = 0; k < kept.length; k++) kept[k] &= wet[k];
  // A narrows that joined two basins is kept and widened (to about the radius each side of it), so that a lake
  // with a waist stays one lake; a thin arm that leads nowhere goes.
  {
    const label = (grid, eight) => {
      const lab = new Int32Array(nx * ny), stack = [];
      let n = 0;
      for (let s = 0; s < grid.length; s++) {
        if (!grid[s] || lab[s]) continue;
        lab[s] = ++n; stack.push(s);
        while (stack.length) {
          const c = stack.pop(), ci = c % nx, cj = (c - ci) / nx;
          for (let dj = -1; dj <= 1; dj++) for (let di = -1; di <= 1; di++) {
            if ((!eight && di && dj) || (!di && !dj)) continue;
            const i = ci + di, j = cj + dj;
            if (i < 0 || j < 0 || i >= nx || j >= ny) continue;
            const t = j * nx + i;
            if (grid[t] && !lab[t]) { lab[t] = n; stack.push(t); }
          }
        }
      }
      return [lab, n];
    };
    const [basin, basins] = label(kept, false);
    if (basins > 1) {
      const thinWet = new Uint8Array(nx * ny);
      for (let k = 0; k < thinWet.length; k++) thinWet[k] = wet[k] && !kept[k] ? 1 : 0;
      const [arm, arms] = label(thinWet, true);
      const meets = Array.from({ length: arms + 1 }, () => new Set());
      for (let k = 0; k < arm.length; k++) {
        if (!arm[k]) continue;
        const ci = k % nx, cj = (k - ci) / nx;
        for (let dj = -1; dj <= 1; dj++) for (let di = -1; di <= 1; di++) { const i = ci + di, j = cj + dj; if (i >= 0 && j >= 0 && i < nx && j < ny && basin[j * nx + i]) meets[arm[k]].add(basin[j * nx + i]); }
      }
      const W = Math.max(1, Math.round(R * 0.6)), wide = [];
      for (let dj = -W; dj <= W; dj++) for (let di = -W; di <= W; di++) if (di * di + dj * dj <= W * W) wide.push([di, dj]);
      for (let k = 0; k < arm.length; k++) {
        if (!arm[k] || meets[arm[k]].size < 2) continue;
        const ci = k % nx, cj = (k - ci) / nx;
        for (const [di, dj] of wide) { const i = ci + di, j = cj + dj; if (i >= 0 && j >= 0 && i < nx && j < ny) kept[j * nx + i] = 1; }
      }
    }
  }
  // The water left as boxes of whole cells (runs along a row, joined down the rows where they match), joined into
  // one shape on the grid's own whole numbers, where the clipper is exact.
  const boxes = [], openBox = new Map();
  for (let j = 0; j <= ny; j++) {
    const runs = new Set();
    if (j < ny) for (let i = 0; i < nx; i++) {
      if (!kept[j * nx + i]) continue;
      let e = i;
      while (e + 1 < nx && kept[j * nx + e + 1]) e++;
      runs.add(`${i}:${e + 1}`);
      i = e;
    }
    for (const [k, top] of openBox) if (!runs.has(k)) { const [a, b] = k.split(':').map(Number); boxes.push([[[a, top], [b, top], [b, j], [a, j], [a, top]]]); openBox.delete(k); }
    for (const k of runs) if (!openBox.has(k)) openBox.set(k, j);
  }
  if (!boxes.length) return [];
  let level = boxes;
  while (level.length > 1) {
    const next = [];
    for (let i = 0; i < level.length; i += 8) next.push(polygonClipping.union(...level.slice(i, i + 8)));
    level = next;
  }
  const out = [];
  for (const poly of level[0]) {
    const rs = poly.map((ring) => {
      let o = open(ring).map(([i, j]) => [x0 + i * H, y0 + j * H]);
      o = blunt(thin(o, tol), bar).map(([x, y]) => [r1(x), r1(y)]);
      o = o.filter((p, i) => p[0] !== o[(i + o.length - 1) % o.length][0] || p[1] !== o[(i + o.length - 1) % o.length][1]);
      return blunt(o, bar);
    }).filter((ring) => ring.length >= 3 && Math.abs(ringArea(ring)) >= 1);
    if (rs.length && Math.abs(ringArea(rs[0])) >= 1) out.push(rs.map(closed));
  }
  return out.sort((a, b) => Math.abs(ringArea(open(b[0]))) - Math.abs(ringArea(open(a[0]))));
}
