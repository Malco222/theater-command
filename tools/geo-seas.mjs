// Works out the sea zones for the campaign map: which water belongs to which zone, which
// regions have a coast on it, and the lines to draw between zones.
import fs from 'node:fs';

export const CELL = 3;

/** Marks every grid cell whose centre lies inside the polygon (outer ring and holes, even-odd). */
export function fill(grid, cols, rows, poly, value) {
  let y0 = 1e9, y1 = -1e9;
  for (const [, y] of poly[0]) { if (y < y0) y0 = y; if (y > y1) y1 = y; }
  const r0 = Math.max(0, Math.floor(y0 / CELL)), r1 = Math.min(rows - 1, Math.ceil(y1 / CELL));
  for (let r = r0; r <= r1; r++) {
    const y = (r + 0.5) * CELL;
    const xs = [];
    for (const ring of poly) {
      for (let i = 0, j = ring.length - 1; i < ring.length; j = i++) {
        const [xi, yi] = ring[i], [xj, yj] = ring[j];
        if (yi > y !== yj > y) xs.push(((xj - xi) * (y - yi)) / (yj - yi) + xi);
      }
    }
    xs.sort((a, b) => a - b);
    for (let k = 0; k + 1 < xs.length; k += 2) {
      const c0 = Math.max(0, Math.ceil(xs[k] / CELL - 0.5)), c1 = Math.min(cols - 1, Math.floor(xs[k + 1] / CELL - 0.5));
      for (let c = c0; c <= c1; c++) grid[r * cols + c] = value;
    }
  }
}

/** Douglas-Peucker on an open line. */
export function simplify(pts, tol) {
  if (pts.length < 3) return pts;
  const keep = new Uint8Array(pts.length);
  keep[0] = keep[pts.length - 1] = 1;
  const stack = [[0, pts.length - 1]];
  while (stack.length) {
    const [s, e] = stack.pop();
    const [x1, y1] = pts[s], [x2, y2] = pts[e];
    const dx = x2 - x1, dy = y2 - y1, len2 = dx * dx + dy * dy || 1e-12;
    let far = 0, idx = -1;
    for (let i = s + 1; i < e; i++) {
      const t = Math.max(0, Math.min(1, ((pts[i][0] - x1) * dx + (pts[i][1] - y1) * dy) / len2));
      const d = Math.hypot(pts[i][0] - (x1 + t * dx), pts[i][1] - (y1 + t * dy));
      if (d > far) { far = d; idx = i; }
    }
    if (far > tol && idx > 0) { keep[idx] = 1; stack.push([s, idx], [idx, e]); }
  }
  return pts.filter((_, i) => keep[i]);
}

/** Rounds the corners of a line (Chaikin), keeping its two ends where they are. */
export function smooth(pts) {
  if (pts.length < 3) return pts;
  const out = [pts[0]];
  for (let i = 0; i < pts.length - 1; i++) {
    const [ax, ay] = pts[i], [bx, by] = pts[i + 1];
    if (i > 0) out.push([ax * 0.75 + bx * 0.25, ay * 0.75 + by * 0.25]);
    if (i < pts.length - 2) out.push([ax * 0.25 + bx * 0.75, ay * 0.25 + by * 0.75]);
  }
  out.push(pts[pts.length - 1]);
  return out;
}

export function buildSeas({ land, regions, proj, width, height }) {
  const data = JSON.parse(fs.readFileSync(new URL('../data/campaign/seas.json', import.meta.url)));
  const cols = Math.ceil(width / CELL), rows = Math.ceil(height / CELL);
  const r1 = (v) => Math.round(v * 10) / 10;

  // Land, and which region each piece of land belongs to.
  const isLand = new Uint8Array(cols * rows);
  for (const p of land) fill(isLand, cols, rows, p, 1);
  const regionAt = new Int16Array(cols * rows).fill(-1);
  for (const r of regions) for (const p of r.polys) fill(regionAt, cols, rows, p.map((ring) => [...ring, ring[0]]), r.id);

  const zones = data.zones.map((z, i) => {
    const at = z.label ?? z.at;
    const [x, y] = proj([at[1], at[0]]);
    return { i, id: z.id, name: z.name, x: r1(x), y: r1(y), air: z.air ?? 1, ...(z.narrow ? { narrow: true } : {}), adj: [], coast: [] };
  });
  const index = new Map(zones.map((z) => [z.id, z.i]));

  // Every stretch of open water belongs to the zone whose anchor is nearest by sea, so that a
  // headland or a peninsula divides two seas the way it does for a ship.
  const zoneAt = new Int8Array(cols * rows).fill(-1);
  const dist = new Float64Array(cols * rows).fill(1e9);
  const heap = []; // binary heap of [distance, cell]
  const push = (d, k) => {
    heap.push([d, k]);
    for (let i = heap.length - 1; i > 0;) { const p = (i - 1) >> 1; if (heap[p][0] <= heap[i][0]) break; [heap[p], heap[i]] = [heap[i], heap[p]]; i = p; }
  };
  const pop = () => {
    const top = heap[0], last = heap.pop();
    if (heap.length) {
      heap[0] = last;
      for (let i = 0; ;) {
        const l = 2 * i + 1, r = l + 1; let m = i;
        if (l < heap.length && heap[l][0] < heap[m][0]) m = l;
        if (r < heap.length && heap[r][0] < heap[m][0]) m = r;
        if (m === i) break;
        [heap[m], heap[i]] = [heap[i], heap[m]]; i = m;
      }
    }
    return top;
  };
  const seed = (lat, lon, zone, what) => {
    const [x, y] = proj([lon, lat]);
    const c = Math.floor(x / CELL), r = Math.floor(y / CELL);
    if (c < 0 || r < 0 || c >= cols || r >= rows || isLand[r * cols + c]) { console.warn(`! sea anchor on land or off the map: ${what} (${lat}, ${lon})`); return; }
    dist[r * cols + c] = 0; zoneAt[r * cols + c] = zone; push(0, r * cols + c);
  };
  data.zones.forEach((z, i) => { seed(z.at[0], z.at[1], i, z.name); for (const w of z.waters ?? []) seed(w[0], w[1], i, z.name); });
  const STEPS = [[1, 0, 1], [-1, 0, 1], [0, 1, 1], [0, -1, 1], [1, 1, Math.SQRT2], [1, -1, Math.SQRT2], [-1, 1, Math.SQRT2], [-1, -1, Math.SQRT2]];
  while (heap.length) {
    const [d, k] = pop();
    if (d > dist[k]) continue;
    const c = k % cols, r = (k - c) / cols;
    for (const [dc, dr, w] of STEPS) {
      const cc = c + dc, rr = r + dr;
      // The outermost ring of cells is the edge of the map, not water.
      if (cc < 1 || rr < 1 || cc >= cols - 1 || rr >= rows - 1) continue;
      const j = rr * cols + cc;
      if (isLand[j] || d + w >= dist[j]) continue;
      // A ship cannot cut the corner between two pieces of land.
      if (dc && dr && (isLand[r * cols + cc] || isLand[rr * cols + c])) continue;
      dist[j] = d + w; zoneAt[j] = zoneAt[k]; push(d + w, j);
    }
  }

  // A region has a coast on a zone when open water of that zone laps against it.
  const touch = new Map(); // region id -> Map(zone -> cells)
  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) {
      const zone = zoneAt[r * cols + c];
      if (zone < 0) continue;
      for (const [dc, dr] of STEPS) {
        const cc = c + dc, rr = r + dr;
        if (cc < 0 || rr < 0 || cc >= cols || rr >= rows) continue;
        const id = regionAt[rr * cols + cc];
        if (id < 0) continue;
        let m = touch.get(id);
        if (!m) touch.set(id, (m = new Map()));
        m.set(zone, (m.get(zone) ?? 0) + 1);
      }
    }
  }
  const seaOf = regions.map(() => []);
  for (const [id, m] of touch) {
    const most = Math.max(...m.values());
    // A corner of coast on a second sea does not make a region a port on it.
    for (const [zone, n] of [...m].sort((a, b) => b[1] - a[1])) if (n >= 8 && n >= most * 0.2) { seaOf[id].push(zone); zones[zone].coast.push(id); }
    if (!seaOf[id].length) { const zone = [...m].sort((a, b) => b[1] - a[1])[0][0]; if (regions[id].flags.port || regions[id].flags.depot) { seaOf[id].push(zone); zones[zone].coast.push(id); } }
  }
  for (const r of regions) if (r.flags.port && !seaOf[r.id].length) console.warn(`! ${r.name} is a port with no sea`);
  for (const z of zones) z.coast.sort((a, b) => a - b);

  for (const [a, b] of data.adjacent) {
    const ia = index.get(a), ib = index.get(b);
    if (ia === undefined || ib === undefined) { console.warn('bad sea link', a, b); continue; }
    zones[ia].adj.push(ib); zones[ib].adj.push(ia);
  }
  const gates = data.gates.map((gt) => ({ a: index.get(gt.between[0]), b: index.get(gt.between[1]), name: gt.name, region: gt.region ?? null, nation: gt.nation ?? null, closed: !!gt.closed }));

  // The lines between zones, traced along the grid where two zones meet and then smoothed.
  const segs = new Map(); // "a-b" -> list of [vertex, vertex], vertices numbered on the grid corners
  const V = (c, r) => r * (cols + 1) + c;
  const add = (za, zb, v0, v1) => {
    const key = za < zb ? `${za}-${zb}` : `${zb}-${za}`;
    let list = segs.get(key);
    if (!list) segs.set(key, (list = []));
    list.push([v0, v1]);
  };
  for (let r = 0; r < rows; r++) {
    for (let c = 0; c < cols; c++) {
      const z = zoneAt[r * cols + c];
      if (z < 0) continue;
      if (c + 1 < cols) { const o = zoneAt[r * cols + c + 1]; if (o >= 0 && o !== z) add(z, o, V(c + 1, r), V(c + 1, r + 1)); }
      if (r + 1 < rows) { const o = zoneAt[(r + 1) * cols + c]; if (o >= 0 && o !== z) add(z, o, V(c, r + 1), V(c + 1, r + 1)); }
    }
  }
  const borders = [];
  for (const list of segs.values()) {
    const at = new Map(); // vertex -> segments touching it
    list.forEach((sg, i) => { for (const v of sg) { let l = at.get(v); if (!l) at.set(v, (l = [])); l.push(i); } });
    const used = new Uint8Array(list.length);
    const walk = (start) => {
      const line = [start];
      let v = start;
      for (;;) {
        const next = (at.get(v) ?? []).find((i) => !used[i]);
        if (next === undefined) break;
        used[next] = 1;
        v = list[next][0] === v ? list[next][1] : list[next][0];
        line.push(v);
      }
      return line;
    };
    const lines = [];
    for (const [v, l] of at) if (l.length === 1) { const line = walk(v); if (line.length > 1) lines.push(line); }
    for (const [v] of at) { const line = walk(v); if (line.length > 1) lines.push(line); }
    for (const line of lines) {
      if (line.length < 6) continue; // a few cells where two zones touch behind an island
      const pts = line.map((v) => [(v % (cols + 1)) * CELL, Math.floor(v / (cols + 1)) * CELL]);
      borders.push(smooth(simplify(pts, 5)).map(([x, y]) => [r1(x), r1(y)]));
    }
  }
  // The same picture at half the detail, for telling which sea a click on the map fell in:
  // one string per row of the grid ('.' is land).
  const LOOK = 2;
  const grid = [];
  for (let r = 0; r < rows; r += LOOK) {
    // Run-length coded: a letter and how many cells of it follow, as in "a12.30b5".
    let row = '', last = '', run = 0;
    for (let c = 0; c < cols; c += LOOK) {
      const z = zoneAt[r * cols + c];
      const ch = z < 0 ? '.' : String.fromCharCode(97 + z);
      if (ch === last) { run++; continue; }
      if (run) row += last + run;
      last = ch; run = 1;
    }
    grid.push(row + last + run);
  }
  // The last four are for the map of the wider world, which is laid around this one (build-geo-world.mjs).
  return { zones: zones.map(({ i, ...z }) => z), gates, borders, seaOf, cell: CELL * LOOK, grid, zoneAt, dist, cols, rows };
}
