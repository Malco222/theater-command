// The field as a staff map: the campaign map's paper and inks, the ground painted soft underneath, roads, water and
// works drawn crisp on top, the relief in contour lines. Painted once a battle into two pictures the screen pans over:
// the map itself, and the detail that comes in only from the closer zooms (the furrows of the fields, the woods' single
// crowns), so the whole-field view stays quiet (the art director's review of the first slice).
// TODO(polish): the art director's own ground swatches and a hand-drawn pass on towns and works.

import { GROUNDS, type Ground, type LandField } from '../land/field';
import type { Terrain } from '../land/field';

/** Pixels a metre in the painted picture. */
export const PAINT = 1;

const INK = '#2a2d2f';
/** The soft wash under everything, by ground. */
const WASH: Record<Ground, string> = {
  open: '#e4dabf', field: '#e8dab0', woods: '#bcc196', rough: '#d8ceb1', marsh: '#c9d0be', water: '#9db2bd', road: '#e4dabf',
  bridge: '#e4dabf', buildings: '#d6caac', rock: '#cbc5b6', bunker: '#d8ceb1', works: '#dccfae', blown: '#9db2bd',
  // A city's own, plainly (the design director, b511(c): blocks, streets and cover enough to read, no more).
  suburb: '#dcd3b4', industry: '#cfc4a9', park: '#cfd2a6',
};
const SAND: Partial<Record<Ground, string>> = { open: '#ead8a9', rough: '#dcc79a', field: '#e7d3a1', road: '#ead8a9', bunker: '#dcc79a' };

/** A small seeded stream for the ornament (trees, houses), so the same field always looks the same. */
function stream(seed: number): () => number {
  let s = seed >>> 0 || 1;
  return () => { s ^= s << 13; s ^= s >>> 17; s ^= s << 5; return ((s >>> 0) % 100000) / 100000; };
}

const sheet = (w: number, h: number): [HTMLCanvasElement, CanvasRenderingContext2D] => {
  const c = document.createElement('canvas');
  c.width = w; c.height = h;
  return [c, c.getContext('2d')!];
};

/** The cells of one kind in patches of at least 'least' cells (joined side to side): the rest are strays. */
function patches(f: LandField, test: (c: number, r: number) => boolean, least: number): Uint8Array {
  const keep = new Uint8Array(f.cols * f.rows), seen = new Uint8Array(f.cols * f.rows);
  for (let i = 0; i < keep.length; i++) {
    if (seen[i] || !test(i % f.cols, Math.floor(i / f.cols))) continue;
    const got: number[] = [], todo = [i];
    seen[i] = 1;
    while (todo.length) {
      const j = todo.pop()!, c = j % f.cols, r = Math.floor(j / f.cols);
      got.push(j);
      for (const [dc, dr] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
        const cc = c + dc, rr = r + dr, k = rr * f.cols + cc;
        if (cc < 0 || rr < 0 || cc >= f.cols || rr >= f.rows || seen[k] || !test(cc, rr)) continue;
        seen[k] = 1; todo.push(k);
      }
    }
    if (got.length >= least) for (const j of got) keep[j] = 1;
  }
  return keep;
}

/**
 * A smooth shape over the cells that pass: the cell grid drawn up, blurred and cut at half, so a patch has rounded
 * edges and not the grid's stair steps (the art director: "less edges, more smooth").
 */
function smoothMask(f: LandField, W: number, H: number, test: (c: number, r: number) => boolean, blur: number): HTMLCanvasElement {
  const [small, s] = sheet(f.cols, f.rows);
  s.fillStyle = '#fff';
  for (let r = 0; r < f.rows; r++) for (let c = 0; c < f.cols; c++) if (test(c, r)) s.fillRect(c, f.rows - 1 - r, 1, 1);
  const [big, b] = sheet(W, H);
  b.imageSmoothingEnabled = true;
  b.filter = `blur(${blur}px)`;
  b.drawImage(small, 0, 0, W, H);
  b.filter = 'none';
  const img = b.getImageData(0, 0, W, H), d = img.data;
  // Cut at half, with a pixel or two of ramp so the edge is not jagged.
  for (let i = 3; i < d.length; i += 4) d[i] = Math.max(0, Math.min(255, (d[i] - 112) * 8));
  b.putImageData(img, 0, 0);
  return big;
}

/** A painted shape blurred and cut at half, its colour kept: rounded edges that need no grid under them. */
function blurCut(cv: HTMLCanvasElement, blur: number): HTMLCanvasElement {
  const [out, o] = sheet(cv.width, cv.height);
  o.filter = `blur(${blur}px)`;
  o.drawImage(cv, 0, 0);
  o.filter = 'none';
  const img = o.getImageData(0, 0, cv.width, cv.height), d = img.data;
  for (let i = 3; i < d.length; i += 4) d[i] = Math.max(0, Math.min(255, (d[i] - 112) * 8));
  o.putImageData(img, 0, 0);
  return out;
}

/** The field painted: the map, and the detail laid over it from the closer zooms. */
/** The painted map, the closer zooms' detail, and the bunkers' places in field metres: the bunker sign is drawn on the
 *  screen at a fixed size, as a label is, so it never grows heavier than the town when the view comes closer. */
export interface Painted { base: HTMLCanvasElement; detail: HTMLCanvasElement; forts: [number, number][] }

export function paintField(f: LandField, terrain: Terrain, seed: number): Painted {
  const W = Math.round(f.w * PAINT), H = Math.round(f.h * PAINT), cs = f.cell * PAINT;
  const [out, x] = sheet(W, H);
  const [detail, dx] = sheet(W, H);
  // A bunker's cell is painted as the ground it was dug into: the sign alone marks it, drawn on the screen (the art director).
  const raw = (c: number, r: number): Ground => GROUNDS[f.ground[Math.max(0, Math.min(f.rows - 1, r)) * f.cols + Math.max(0, Math.min(f.cols - 1, c))]];
  const g = (c: number, r: number): Ground => {
    const i = Math.max(0, Math.min(f.rows - 1, r)) * f.cols + Math.max(0, Math.min(f.cols - 1, c));
    const under = f.beneath?.[i];
    return GROUNDS[under !== undefined ? under : f.ground[i]];
  };
  const hgt = (c: number, r: number): number => f.height[Math.max(0, Math.min(f.rows - 1, r)) * f.cols + Math.max(0, Math.min(f.cols - 1, c))];
  // Field row r is metres north; the picture's row 0 is the north edge.
  const px = (c: number): number => c * cs;
  const py = (r: number): number => H - (r + 1) * cs;
  const rnd = stream(seed ^ 0x9a1d);
  const wash = (k: Ground): string => (terrain === 'desert' ? SAND[k] ?? WASH[k] : WASH[k]);

  // 1. The wash: one pixel a cell, then drawn up soft.
  const small = document.createElement('canvas');
  small.width = f.cols; small.height = f.rows;
  const s = small.getContext('2d')!;
  for (let r = 0; r < f.rows; r++) for (let c = 0; c < f.cols; c++) {
    const k = g(c, r);
    // Woods are drawn as their canopy below, so they leave no halo in the wash; marsh and rough ground as their own
    // soft-edged patches just after, so their edges wander and never follow the grid (the art director, b408 review).
    s.fillStyle = wash(k === 'water' || k === 'blown' || k === 'bridge' || k === 'woods' || k === 'marsh' || k === 'rough' ? 'open' : k);
    s.fillRect(c, f.rows - 1 - r, 1, 1);
  }
  x.imageSmoothingEnabled = true;
  x.drawImage(small, 0, 0, W, H);
  // Blur the grid, cut it at half, then soften: the same edge the fields have. A lone cell or a pair would blur away
  // under the cut, so each cell also keeps a small round floor of its own: the ground is never there and unseen.
  const soft = (k: Ground): HTMLCanvasElement => {
    const m = smoothMask(f, W, H, (c, r) => g(c, r) === k, cs * 0.55), mc = m.getContext('2d')!;
    mc.fillStyle = '#fff'; mc.beginPath();
    for (let r = 0; r < f.rows; r++) for (let c = 0; c < f.cols; c++) {
      if (g(c, r) !== k) continue;
      const X = px(c) + cs / 2, Y = py(r) + cs / 2;
      mc.moveTo(X + cs * 0.36, Y); mc.arc(X, Y, cs * 0.36, 0, Math.PI * 2);
    }
    mc.fill();
    return m;
  };
  const masks: Partial<Record<Ground, HTMLCanvasElement>> = { marsh: soft('marsh'), rough: soft('rough') };
  for (const k of ['marsh', 'rough'] as Ground[]) {
    const [lv, l2] = sheet(W, H);
    l2.fillStyle = wash(k); l2.fillRect(0, 0, W, H);
    l2.globalCompositeOperation = 'destination-in';
    l2.drawImage(masks[k]!, 0, 0);
    x.filter = `blur(${(cs * 0.12).toFixed(1)}px)`; x.drawImage(lv, 0, 0); x.filter = 'none';
  }

  // 2. Hill shade, faint, one light from the north-west: faces turned to the north-west lit warm, faces turned to the
  // south-east shaded, as the 3D map's relief (the art director, b418 review: it was lit from the south-east, darker
  // than it was lit, and taken cell by cell, so the mountains' slopes read as smoke). Slopes are read across five cells,
  // so the shade follows the hills and not every knoll. The map's relief is in its contours.
  const shade = document.createElement('canvas');
  shade.width = f.cols; shade.height = f.rows;
  const sh = shade.getContext('2d')!;
  let relief = 0;
  for (let i = 0; i < f.height.length; i++) relief = Math.max(relief, f.height[i]);
  const k = 2.2 / Math.max(6, f.cell);
  const slope = (c: number, r: number, dc: number, dr: number): number => {
    let s = 0;
    for (let j = -1; j <= 1; j++) s += (hgt(c + 2 * dc + j * dr, r + 2 * dr + j * dc) - hgt(c - 2 * dc + j * dr, r - 2 * dr + j * dc)) / 4;
    return s / 3;
  };
  for (let r = 0; r < f.rows; r++) for (let c = 0; c < f.cols; c++) {
    const dx = slope(c, r, 1, 0), dy = slope(c, r, 0, 1);
    // Rising to the east (dx > 0) the face turns west; rising to the north (dy > 0) it turns south.
    const lit = Math.max(-1, Math.min(1, (dx - dy) * k));
    sh.fillStyle = lit > 0 ? `rgba(255,246,220,${(lit * 0.34).toFixed(3)})` : `rgba(84,66,44,${(-lit * 0.24).toFixed(3)})`;
    sh.fillRect(c, f.rows - 1 - r, 1, 1);
  }
  x.drawImage(shade, 0, 0, W, H);

  // 3. Woods: one soft canopy for each wood of five cells or more, a muted green with a slightly darker scalloped
  // edge, drawn on a layer of its own so overlaps do not darken (the art director, 2). Strays show only from closer in.
  // A lone cell of other ground inside a wood (a clearing of one cell, or a hidden bunker) is not cut out of the
  // canopy: an empty star with nothing in it reads as a mark (the art director, b408 review). Roads, water and
  // buildings still cut it, and a bunker that shows has its sign.
  const woody = (c: number, r: number): boolean => g(c, r) === 'woods';
  const isWood = (c: number, r: number): boolean => woody(c, r) || (!['road', 'water', 'bridge', 'blown', 'buildings', 'industry'].includes(g(c, r))
    && c > 0 && r > 0 && c < f.cols - 1 && r < f.rows - 1 && woody(c - 1, r) && woody(c + 1, r) && woody(c, r - 1) && woody(c, r + 1));
  const woods = patches(f, isWood, 5);
  // Each crown is nudged off its cell's middle and sized a little up or down, from a stream of its own so the rest of
  // the ornament is where it was: the edge of a wood wanders and never follows the grid (the art director, b411 review).
  const jit = stream(seed ^ 0x3c0d);
  const nudge = new Float32Array(woods.length * 3);
  for (let i = 0; i < woods.length; i++) if (woods[i]) { nudge[i * 3] = (jit() - 0.5) * 0.3; nudge[i * 3 + 1] = (jit() - 0.5) * 0.3; nudge[i * 3 + 2] = 0.9 + jit() * 0.24; }
  const canopy = (color: string, rad: number): HTMLCanvasElement => {
    const [cv, cx2] = sheet(W, H);
    cx2.fillStyle = color;
    cx2.beginPath();
    for (let i = 0; i < woods.length; i++) {
      if (!woods[i]) continue;
      const X = px(i % f.cols) + cs * (0.5 + nudge[i * 3]), Y = py(Math.floor(i / f.cols)) + cs * (0.5 + nudge[i * 3 + 1]), R = rad * nudge[i * 3 + 2];
      cx2.moveTo(X + R, Y); cx2.arc(X, Y, R, 0, Math.PI * 2);
    }
    cx2.fill();
    // Blurred, cut at half and softened, as the marsh and the fields are.
    return blurCut(cv, cs * 0.3);
  };
  {
    const [cv, c2] = sheet(W, H);
    // The inner disc reaches past the corners of its cell, so the canopy is one solid shape; the outer one rims its edge.
    c2.drawImage(canopy('#a3aa7c', cs * 0.92), 0, 0);
    c2.drawImage(canopy('#bcc196', cs * 0.76), 0, 0);
    x.globalAlpha = 0.92; x.filter = `blur(${(cs * 0.08).toFixed(1)}px)`; x.drawImage(cv, 0, 0); x.filter = 'none'; x.globalAlpha = 1;
  }
  // The fields: furrows over a smooth patch, in the detail only (at the whole-field view their hatch is a mottle).
  {
    const [hv, h2] = sheet(W, H);
    h2.strokeStyle = 'rgba(120,96,52,.22)'; h2.lineWidth = 1.2;
    h2.beginPath();
    for (let y = -W * 0.2; y < H + W * 0.2; y += 6) { h2.moveTo(0, y); h2.lineTo(W, y + W * 0.08); }
    h2.stroke();
    h2.globalCompositeOperation = 'destination-in';
    h2.drawImage(smoothMask(f, W, H, (c, r) => g(c, r) === 'field', cs * 0.45), 0, 0);
    dx.drawImage(hv, 0, 0);
  }
  // Ornament by cell: single crowns (detail only, no ring and no glow), tussocks, reeds, scree.
  const [stipple, st] = sheet(W, H);
  for (let r = 0; r < f.rows; r++) for (let c = 0; c < f.cols; c++) {
    const kd = g(c, r), X = px(c), Y = py(r);
    if (kd === 'woods') {
      for (let i = 0; i < 3; i++) {
        const tx = X + 4 + rnd() * (cs - 8), ty = Y + 4 + rnd() * (cs - 8), tr = 4.5 + rnd() * 3.5;
        dx.fillStyle = 'rgba(104,116,68,.42)'; dx.beginPath(); dx.arc(tx, ty, tr, 0, Math.PI * 2); dx.fill();
      }
    } else if (kd === 'rough') {
      // Its stipple on a sheet of its own, cut to the patch's soft edge below: no soft squares.
      st.fillStyle = 'rgba(80,66,46,.27)';
      for (let i = 0; i < 5; i++) { st.beginPath(); st.arc(X + (rnd() * 1.4 - 0.2) * cs, Y + (rnd() * 1.4 - 0.2) * cs, 1.1, 0, Math.PI * 2); st.fill(); }
    } else if (kd === 'marsh') {
      // At the whole-field view the marsh is its wash alone; its water and tufts come in with the detail, a sparse
      // scatter rather than a mark in every cell, so no rows show (the art director, b408 review).
      const roll = rnd(), mx = X + 3 + rnd() * (cs - 12), my = Y + 5 + rnd() * (cs - 10);
      if (roll < 0.22) { dx.strokeStyle = 'rgba(63,110,140,.5)'; dx.lineWidth = 1.1; dx.beginPath(); dx.moveTo(mx, my); dx.lineTo(mx + 7, my); dx.stroke(); }
      else if (roll < 0.4) { dx.strokeStyle = 'rgba(70,84,50,.45)'; dx.lineWidth = 1.1; dx.beginPath(); dx.moveTo(mx, my); dx.lineTo(mx - 2, my - 5); dx.moveTo(mx, my); dx.lineTo(mx + 2, my - 5); dx.stroke(); }
    } else if (kd === 'rock') {
      x.strokeStyle = 'rgba(70,64,56,.42)'; x.lineWidth = 1;
      for (let i = 0; i < 4; i++) { const mx = X + rnd() * cs, my = Y + rnd() * cs; x.beginPath(); x.moveTo(mx, my); x.lineTo(mx + 3, my - 4); x.lineTo(mx + 6, my); x.stroke(); }
    }
  }

  st.globalCompositeOperation = 'destination-in';
  st.drawImage(masks.rough!, 0, 0);
  x.drawImage(stipple, 0, 0);

  // 4. Contours from the heights: every few metres, every fifth one heavier.
  const step = relief > 120 ? 25 : relief > 40 ? 10 : 4;
  const lv = (c: number, r: number): number => hgt(c, r);
  const cx = (c: number): number => px(c) + cs / 2, cy = (r: number): number => py(r) + cs / 2;
  for (let h = step; h < relief; h += step) {
    const heavy = Math.round(h / step) % 5 === 0;
    x.strokeStyle = heavy ? 'rgba(110,84,52,.55)' : 'rgba(110,84,52,.28)';
    x.lineWidth = heavy ? 1.6 : 1;
    x.beginPath();
    for (let r = 0; r < f.rows - 1; r++) for (let c = 0; c < f.cols - 1; c++) {
      const a = lv(c, r), b = lv(c + 1, r), d = lv(c, r + 1), e = lv(c + 1, r + 1);
      const pts: [number, number][] = [];
      const edge = (h0: number, h1: number, x0: number, y0: number, x1: number, y1: number): void => {
        if ((h0 < h) !== (h1 < h)) { const t = (h - h0) / (h1 - h0); pts.push([x0 + (x1 - x0) * t, y0 + (y1 - y0) * t]); }
      };
      edge(a, b, cx(c), cy(r), cx(c + 1), cy(r));
      edge(b, e, cx(c + 1), cy(r), cx(c + 1), cy(r + 1));
      edge(e, d, cx(c + 1), cy(r + 1), cx(c), cy(r + 1));
      edge(d, a, cx(c), cy(r + 1), cx(c), cy(r));
      for (let i = 0; i + 1 < pts.length; i += 2) { x.moveTo(pts[i][0], pts[i][1]); x.lineTo(pts[i + 1][0], pts[i + 1][1]); }
    }
    x.stroke();
  }

  // 5. Water: each wet cell joined to its wet neighbours with a broad round stroke, so the river runs smooth.
  const wet = (kd: Ground): boolean => kd === 'water' || kd === 'blown' || kd === 'bridge';
  const joined = (test: (kd: Ground) => boolean, width: number, color: string): void => {
    x.strokeStyle = color; x.lineWidth = width; x.lineCap = 'round'; x.lineJoin = 'round';
    x.beginPath();
    for (let r = 0; r < f.rows; r++) for (let c = 0; c < f.cols; c++) {
      if (!test(g(c, r))) continue;
      x.moveTo(cx(c), cy(r)); x.lineTo(cx(c) + 0.01, cy(r));
      for (const [dc, dr] of [[1, 0], [0, 1], [1, 1], [1, -1]]) {
        if (c + dc >= f.cols || r + dr < 0 || r + dr >= f.rows || !test(g(c + dc, r + dr))) continue;
        // A diagonal step only where no square step joins the two already: no little triangles at the corners.
        if (dc && dr && (test(g(c + dc, r)) || test(g(c, r + dr)))) continue;
        x.moveTo(cx(c), cy(r)); x.lineTo(cx(c + dc), cy(r + dr));
      }
    }
    x.stroke();
  };
  // Water as the union of a disc on each wet cell, on a layer of its own so the pale bank does not darken where discs
  // overlap; then the river's own line down the middle of it, column by column.
  const layer = (color: string, r: number, alpha: number): void => {
    const c2 = document.createElement('canvas');
    c2.width = W; c2.height = H;
    const y2 = c2.getContext('2d')!;
    y2.fillStyle = color;
    y2.beginPath();
    for (let rr = 0; rr < f.rows; rr++) for (let c = 0; c < f.cols; c++) if (wet(g(c, rr))) { y2.moveTo(cx(c) + r, cy(rr)); y2.arc(cx(c), cy(rr), r, 0, Math.PI * 2); }
    y2.fill();
    x.globalAlpha = alpha; x.drawImage(c2, 0, 0); x.globalAlpha = 1;
  };
  layer('#eef1ee', cs * 1.05, 0.55);
  layer('#9db2bd', cs * 0.82, 1);
  const mid: [number, number][] = [];
  // Where the river bends away from the lanes of the corps that do not cross, the field gives its line (b464(5)): a
  // column's middle would cut the corner where it turns down to the attacker's edge.
  // A city's own river (b526) gives its line too, and a second meeting it its own; a shore has none, only its water.
  const lines: [number, number][][] = [];
  if (f.stream && f.stream.lie !== 'shore') for (const run of [f.stream.course, ...(f.stream.join ? [f.stream.join.course] : [])]) lines.push(run.filter((_, i) => i % 2 === 0).map(([ax, ay]) => [ax * PAINT, H - ay * PAINT]));
  else if (f.stream) { /* a shore: its water alone */ }
  else if (f.course) for (let i = 0; i < f.course.length; i += 2) mid.push([f.course[i][0] * PAINT, H - f.course[i][1] * PAINT]);
  else for (let c = 0; c < f.cols; c++) {
    let sum = 0, n = 0;
    for (let rr = 0; rr < f.rows; rr++) if (wet(g(c, rr))) { sum += rr; n++; }
    if (n) mid.push([cx(c), cy(sum / n)]);
  }
  if (mid.length > 1) lines.push(mid);
  for (const line of lines) {
    if (line.length < 2) continue;
    x.strokeStyle = 'rgba(63,127,174,.6)'; x.lineWidth = 3; x.lineCap = 'round'; x.lineJoin = 'round';
    x.beginPath(); x.moveTo(line[0][0], line[0][1]);
    for (let i = 1; i < line.length - 1; i++) x.quadraticCurveTo(line[i][0], line[i][1], (line[i][0] + line[i + 1][0]) / 2, (line[i][1] + line[i + 1][1]) / 2);
    x.lineTo(line[line.length - 1][0], line[line.length - 1][1]);
    x.stroke();
  }

  // 6. Roads: a pale bed between ink edges.
  const roadish = (kd: Ground): boolean => kd === 'road' || kd === 'bridge' || kd === 'blown';
  joined(roadish, 11, 'rgba(90,74,52,.75)');
  joined(roadish, 8, '#f4ecd6');

  // 7. Bridges standing and blown, houses, bunkers and fieldworks.
  for (let r = 0; r < f.rows; r++) for (let c = 0; c < f.cols; c++) {
    const kd = g(c, r), X = px(c), Y = py(r);
    if (kd === 'bridge') {
      // Laid along its deck: across the cell where the road runs on east and west of it and not north or south (a
      // bridge over a city's river that runs up the field, b526), up the cell otherwise. A step in a deck has road both
      // ways: there the river's own line says which way the bridge runs (across one that runs up the field).
      const deckAt = (cc: number, rr: number): boolean => cc >= 0 && rr >= 0 && cc < f.cols && rr < f.rows && (g(cc, rr) === 'road' || g(cc, rr) === 'bridge');
      const ew = deckAt(c - 1, r) || deckAt(c + 1, r), ns = deckAt(c, r - 1) || deckAt(c, r + 1);
      const riverRunsUp = (): boolean => {
        if (!f.stream || f.stream.lie === 'shore') return false;
        const mx = c * f.cell + f.cell / 2, my = r * f.cell + f.cell / 2;
        let best = Infinity, up = false;
        for (const run of [f.stream.course, ...(f.stream.join ? [f.stream.join.course] : [])]) for (let k = 1; k < run.length; k++) {
          const [ax, ay] = run[k - 1], [bx, by] = run[k], d = Math.hypot((ax + bx) / 2 - mx, (ay + by) / 2 - my);
          if (d < best) { best = d; up = Math.abs(by - ay) > Math.abs(bx - ax); }
        }
        return up;
      };
      if (ew && (!ns || riverRunsUp())) {
        x.fillStyle = '#f4ecd6'; x.fillRect(X, Y + cs / 2 - 6, cs, 12);
        x.strokeStyle = INK; x.lineWidth = 1.6; x.beginPath(); x.moveTo(X, Y + cs / 2 - 7); x.lineTo(X + cs, Y + cs / 2 - 7); x.moveTo(X, Y + cs / 2 + 7); x.lineTo(X + cs, Y + cs / 2 + 7); x.stroke();
        continue;
      }
      x.fillStyle = '#f4ecd6'; x.fillRect(X + cs / 2 - 6, Y, 12, cs);
      x.strokeStyle = INK; x.lineWidth = 1.6; x.beginPath(); x.moveTo(X + cs / 2 - 7, Y); x.lineTo(X + cs / 2 - 7, Y + cs); x.moveTo(X + cs / 2 + 7, Y); x.lineTo(X + cs / 2 + 7, Y + cs); x.stroke();
    } else if (kd === 'blown') {
      x.strokeStyle = '#c22a12'; x.lineWidth = 2.4; x.lineCap = 'round';
      x.beginPath(); x.moveTo(X + 5, Y + 5); x.lineTo(X + cs - 5, Y + cs - 5); x.moveTo(X + cs - 5, Y + 5); x.lineTo(X + 5, Y + cs - 5); x.stroke();
    } else if (kd === 'buildings') {
      x.fillStyle = 'rgba(92,80,64,.82)'; x.strokeStyle = 'rgba(42,33,24,.9)'; x.lineWidth = 0.8;
      const n = 2 + Math.floor(rnd() * 3);
      for (let i = 0; i < n; i++) {
        const bw = 5 + rnd() * 7, bh = 4 + rnd() * 6, bx = X + rnd() * (cs - bw), by = Y + rnd() * (cs - bh);
        x.fillRect(bx, by, bw, bh); x.strokeRect(bx, by, bw, bh);
      }
    } else if (kd === 'suburb') {
      // Houses with gardens: one or two small roofs a cell, a hedge line now and then.
      x.fillStyle = 'rgba(112,98,78,.7)'; x.strokeStyle = 'rgba(42,33,24,.75)'; x.lineWidth = 0.7;
      const n = 1 + Math.floor(rnd() * 2);
      for (let i = 0; i < n; i++) {
        const bw = 4 + rnd() * 3, bx = X + 2 + rnd() * (cs - bw - 4), by = Y + 2 + rnd() * (cs - bw - 4);
        x.fillRect(bx, by, bw, bw * 0.8); x.strokeRect(bx, by, bw, bw * 0.8);
      }
    } else if (kd === 'industry') {
      // Sheds and yards: one long roof a cell.
      x.fillStyle = 'rgba(120,112,100,.85)'; x.strokeStyle = 'rgba(42,33,24,.9)'; x.lineWidth = 0.9;
      x.fillRect(X + 2, Y + 4, cs - 4, cs * 0.5); x.strokeRect(X + 2, Y + 4, cs - 4, cs * 0.5);
    } else if (kd === 'park') {
      x.fillStyle = 'rgba(120,140,90,.55)';
      for (let i = 0; i < 2; i++) { x.beginPath(); x.arc(X + 4 + rnd() * (cs - 8), Y + 4 + rnd() * (cs - 8), 3, 0, Math.PI * 2); x.fill(); }
    } else if (kd === 'works') {
      x.strokeStyle = 'rgba(92,70,40,.85)'; x.lineWidth = 1.5;
      x.beginPath(); x.moveTo(X + 2, Y + cs / 2);
      for (let i = 0; i < 4; i++) x.lineTo(X + 2 + (i + 0.5) * (cs - 4) / 4, Y + cs / 2 + (i % 2 ? 4 : -4));
      x.lineTo(X + cs - 2, Y + cs / 2); x.stroke();
    }
  }

  // Bunkers: the campaign map's own fortress sign, one for each bunker however many cells it covers (the art director,
  // 3). Only their places here: drawFrame draws the sign at its whole-field size on the screen.
  const forts: [number, number][] = [];
  {
    const isBunker = (c: number, r: number): boolean => raw(c, r) === 'bunker';
    const seen = new Uint8Array(f.cols * f.rows);
    for (let i = 0; i < seen.length; i++) {
      const c0 = i % f.cols, r0 = Math.floor(i / f.cols);
      if (seen[i] || !isBunker(c0, r0)) continue;
      let sx = 0, sy = 0, n = 0;
      const todo = [i]; seen[i] = 1;
      while (todo.length) {
        const j = todo.pop()!, c = j % f.cols, r = Math.floor(j / f.cols);
        sx += (c + 0.5) * f.cell; sy += (r + 0.5) * f.cell; n++;
        for (const [dc, dr] of [[1, 0], [-1, 0], [0, 1], [0, -1], [1, 1], [-1, -1], [1, -1], [-1, 1]]) {
          const cc = c + dc, rr = r + dr, q = rr * f.cols + cc;
          if (cc < 0 || rr < 0 || cc >= f.cols || rr >= f.rows || seen[q] || !isBunker(cc, rr)) continue;
          seen[q] = 1; todo.push(q);
        }
      }
      forts.push([sx / n, sy / n]);
    }
  }

  // 8. The paper's grain and a ruled border, as on the campaign map's sheets.
  x.fillStyle = 'rgba(80,60,30,.035)';
  for (let i = 0; i < (W * H) / 900; i++) x.fillRect(rnd() * W, rnd() * H, 1.5, 1.5);
  x.strokeStyle = 'rgba(42,45,47,.6)'; x.lineWidth = 3; x.strokeRect(1.5, 1.5, W - 3, H - 3);
  return { base: out, detail, forts };
}
