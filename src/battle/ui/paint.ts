// The field as a staff map: the campaign map's paper and inks, the ground painted soft underneath, roads, water and
// works drawn crisp on top, the relief in contour lines. Painted once a battle into a picture the screen pans over.
// TODO(polish): the art director's own ground swatches and a hand-drawn pass on woods, towns and works.

import { GROUNDS, type Ground, type LandField } from '../land/field';
import type { Terrain } from '../land/field';

/** Pixels a metre in the painted picture. */
export const PAINT = 1;

const INK = '#2a2d2f';
/** The soft wash under everything, by ground. */
const WASH: Record<Ground, string> = {
  open: '#e4dabf', field: '#e8dab0', woods: '#bcc196', rough: '#d8ceb1', marsh: '#c9d0be', water: '#9db2bd', road: '#e4dabf',
  bridge: '#e4dabf', buildings: '#d6caac', rock: '#cbc5b6', bunker: '#d8ceb1', works: '#dccfae', blown: '#9db2bd',
};
const SAND: Partial<Record<Ground, string>> = { open: '#ead8a9', rough: '#dcc79a', field: '#e7d3a1', road: '#ead8a9', bunker: '#dcc79a' };

/** A small seeded stream for the ornament (trees, houses), so the same field always looks the same. */
function stream(seed: number): () => number {
  let s = seed >>> 0 || 1;
  return () => { s ^= s << 13; s ^= s >>> 17; s ^= s << 5; return ((s >>> 0) % 100000) / 100000; };
}

export function paintField(f: LandField, terrain: Terrain, seed: number): HTMLCanvasElement {
  const W = Math.round(f.w * PAINT), H = Math.round(f.h * PAINT), cs = f.cell * PAINT;
  const out = document.createElement('canvas');
  out.width = W; out.height = H;
  const x = out.getContext('2d')!;
  const g = (c: number, r: number): Ground => GROUNDS[f.ground[Math.max(0, Math.min(f.rows - 1, r)) * f.cols + Math.max(0, Math.min(f.cols - 1, c))]];
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
    s.fillStyle = wash(k === 'water' || k === 'blown' || k === 'bridge' ? 'open' : k);
    s.fillRect(c, f.rows - 1 - r, 1, 1);
  }
  x.imageSmoothingEnabled = true;
  x.drawImage(small, 0, 0, W, H);

  // 2. Hill shade, lit from the north-west, faint: the map's relief is in its contours.
  const shade = document.createElement('canvas');
  shade.width = f.cols; shade.height = f.rows;
  const sh = shade.getContext('2d')!;
  let relief = 0;
  for (let i = 0; i < f.height.length; i++) relief = Math.max(relief, f.height[i]);
  const k = 2.2 / Math.max(6, f.cell);
  for (let r = 0; r < f.rows; r++) for (let c = 0; c < f.cols; c++) {
    const dx = (hgt(c + 1, r) - hgt(c - 1, r)) / 2, dy = (hgt(c, r + 1) - hgt(c, r - 1)) / 2;
    const lit = Math.max(-1, Math.min(1, (-dx + dy) * k));
    sh.fillStyle = lit > 0 ? `rgba(255,250,235,${(lit * 0.32).toFixed(3)})` : `rgba(60,46,30,${(-lit * 0.34).toFixed(3)})`;
    sh.fillRect(c, f.rows - 1 - r, 1, 1);
  }
  x.drawImage(shade, 0, 0, W, H);

  // 3. Ornament by cell: furrows, trees, tussocks, reeds, scree.
  for (let r = 0; r < f.rows; r++) for (let c = 0; c < f.cols; c++) {
    const kd = g(c, r), X = px(c), Y = py(r);
    if (kd === 'field') {
      x.strokeStyle = 'rgba(120,96,52,.20)'; x.lineWidth = 1.2;
      const lean = (c * 7 + r * 3) % 2 ? 0.35 : -0.35;
      for (let i = 4; i < cs; i += 6) { x.beginPath(); x.moveTo(X, Y + i); x.lineTo(X + cs, Y + i + lean * 6); x.stroke(); }
    } else if (kd === 'woods') {
      for (let i = 0; i < 3; i++) {
        const tx = X + 4 + rnd() * (cs - 8), ty = Y + 4 + rnd() * (cs - 8), tr = 4.5 + rnd() * 3.5;
        x.fillStyle = 'rgba(112,124,74,.55)'; x.beginPath(); x.arc(tx, ty, tr, 0, Math.PI * 2); x.fill();
        x.strokeStyle = 'rgba(52,58,34,.55)'; x.lineWidth = 0.9; x.stroke();
      }
    } else if (kd === 'rough') {
      x.fillStyle = 'rgba(80,66,46,.38)';
      for (let i = 0; i < 5; i++) { x.beginPath(); x.arc(X + rnd() * cs, Y + rnd() * cs, 1.1, 0, Math.PI * 2); x.fill(); }
    } else if (kd === 'marsh') {
      x.strokeStyle = 'rgba(63,110,140,.55)'; x.lineWidth = 1.1;
      for (let i = 0; i < 3; i++) { const mx = X + 3 + rnd() * (cs - 12), my = Y + 4 + rnd() * (cs - 8); x.beginPath(); x.moveTo(mx, my); x.lineTo(mx + 8, my); x.stroke(); }
      x.strokeStyle = 'rgba(70,84,50,.5)';
      const mx = X + rnd() * cs, my = Y + rnd() * cs; x.beginPath(); x.moveTo(mx, my); x.lineTo(mx - 2, my - 5); x.moveTo(mx, my); x.lineTo(mx + 2, my - 5); x.stroke();
    } else if (kd === 'rock') {
      x.strokeStyle = 'rgba(70,64,56,.42)'; x.lineWidth = 1;
      for (let i = 0; i < 4; i++) { const mx = X + rnd() * cs, my = Y + rnd() * cs; x.beginPath(); x.moveTo(mx, my); x.lineTo(mx + 3, my - 4); x.lineTo(mx + 6, my); x.stroke(); }
    }
  }

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
  for (let c = 0; c < f.cols; c++) {
    let sum = 0, n = 0;
    for (let rr = 0; rr < f.rows; rr++) if (wet(g(c, rr))) { sum += rr; n++; }
    if (n) mid.push([cx(c), cy(sum / n)]);
  }
  if (mid.length > 1) {
    x.strokeStyle = 'rgba(63,127,174,.6)'; x.lineWidth = 3; x.lineCap = 'round'; x.lineJoin = 'round';
    x.beginPath(); x.moveTo(mid[0][0], mid[0][1]);
    for (let i = 1; i < mid.length - 1; i++) x.quadraticCurveTo(mid[i][0], mid[i][1], (mid[i][0] + mid[i + 1][0]) / 2, (mid[i][1] + mid[i + 1][1]) / 2);
    x.lineTo(mid[mid.length - 1][0], mid[mid.length - 1][1]);
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
    } else if (kd === 'bunker') {
      x.fillStyle = 'rgba(42,45,47,.85)'; x.fillRect(X + cs / 2 - 5, Y + cs / 2 - 4, 10, 8);
      x.strokeStyle = INK; x.lineWidth = 1.2; x.beginPath(); x.arc(X + cs / 2, Y + cs / 2, 9, Math.PI * 1.1, Math.PI * 1.9); x.stroke();
    } else if (kd === 'works') {
      x.strokeStyle = 'rgba(92,70,40,.85)'; x.lineWidth = 1.5;
      x.beginPath(); x.moveTo(X + 2, Y + cs / 2);
      for (let i = 0; i < 4; i++) x.lineTo(X + 2 + (i + 0.5) * (cs - 4) / 4, Y + cs / 2 + (i % 2 ? 4 : -4));
      x.lineTo(X + cs - 2, Y + cs / 2); x.stroke();
    }
  }

  // 8. The paper's grain and a ruled border, as on the campaign map's sheets.
  x.fillStyle = 'rgba(80,60,30,.035)';
  for (let i = 0; i < (W * H) / 900; i++) x.fillRect(rnd() * W, rnd() * H, 1.5, 1.5);
  x.strokeStyle = 'rgba(42,45,47,.6)'; x.lineWidth = 3; x.strokeRect(1.5, 1.5, W - 3, H - 3);
  return out;
}
