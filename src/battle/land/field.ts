// The field: a province's ground, built for one battle.
//
// Made, not placed: everything comes from what the campaign knows about the province (its terrain, a river between
// the armies and whether its bridges are down, its town and how big it is, its fortress works) and from the battle's
// seed, so the same attack always gives the same field. The field is a grid of cells; each cell has a ground kind and
// a height. The 3D game builds its relief and paints its ground from exactly these two lists.
//
// Frame (see core/types.ts): metres, x east, y north, the attacker comes on from the south.

import { Rng } from '../../core/rng';

/** Ground kinds, in the order their codes are stored in LandField.ground. Append only: saved fields keep their codes. */
// (The art director's list of 17 November: open, field, woods, buildings, road, water with a bridge standing or blown,
// fieldworks and bunker first; then marsh and rock; rough last. Desert is a paint of the whole field, from the terrain.)
export const GROUNDS = ['open', 'field', 'woods', 'rough', 'marsh', 'water', 'road', 'bridge', 'buildings', 'rock', 'bunker', 'works', 'blown'] as const;
export type Ground = (typeof GROUNDS)[number];
const G = Object.fromEntries(GROUNDS.map((k, i) => [k, i])) as Record<Ground, number>;

export type Terrain = 'plains' | 'forest' | 'hills' | 'mountain' | 'marsh' | 'urban' | 'desert';

/** The province, as the field needs it. Plain data: it crosses to the 3D game with the ticket. */
export interface LandPlace {
  region: number;
  name: string;
  terrain: Terrain;
  /** The river the attack crosses, by name, or null. */
  river: string | null;
  /** Its bridges are down: only men on foot can get over, and slowly. */
  bridgeDown: boolean;
  /** The chief town's name and size, where the fight is near it. */
  town: string | null;
  townSize: 'capital' | 'city' | 'town' | 'village' | null;
  /** Fortress works facing this attack, 0 to 3. */
  fort: number;
  /** Minefields on the ground, 0 up. */
  mines: number;
}

export interface LandField {
  /** Metres. */
  w: number;
  h: number;
  /** Metres a cell. */
  cell: number;
  cols: number;
  rows: number;
  /** Row by row from the south-west corner: ground[r * cols + c] is a code into GROUNDS. */
  ground: number[];
  /** Height of each cell in metres, the same order. */
  height: number[];
  /** Where each side forms up: the attacker's band along the south, the defender's along the north. */
  zones: { att: [number, number, number, number]; def: [number, number, number, number] };
  /** The town's centre, where there is one on the field. */
  town: { x: number; y: number; r: number } | null;
}

export const FIELD = { w: 3000, h: 2000, cell: 25 };

/** How each province's ground is made: relief in metres, and how much of it is each kind of cover. */
const RECIPE: Record<Terrain, { relief: number; woods: number; fields: number; rough: number; marsh: number; rock: number }> = {
  plains: { relief: 14, woods: 0.14, fields: 0.38, rough: 0.03, marsh: 0, rock: 0 },
  forest: { relief: 22, woods: 0.55, fields: 0.12, rough: 0.05, marsh: 0.02, rock: 0 },
  hills: { relief: 70, woods: 0.26, fields: 0.2, rough: 0.12, marsh: 0, rock: 0.02 },
  mountain: { relief: 190, woods: 0.22, fields: 0.04, rough: 0.22, marsh: 0, rock: 0.16 },
  marsh: { relief: 5, woods: 0.16, fields: 0.1, rough: 0.02, marsh: 0.42, rock: 0 },
  urban: { relief: 12, woods: 0.06, fields: 0.18, rough: 0.04, marsh: 0, rock: 0 },
  desert: { relief: 18, woods: 0, fields: 0, rough: 0.14, marsh: 0, rock: 0.03 },
};

/** Smooth value noise in 0..1, from a seeded lattice. 'scale' is the size of one feature in cells. */
function noise(rng: Rng, cols: number, rows: number, scale: number): Float32Array {
  const gx = Math.ceil(cols / scale) + 2, gy = Math.ceil(rows / scale) + 2;
  const lat = new Float32Array(gx * gy);
  for (let i = 0; i < lat.length; i++) lat[i] = rng.next();
  const out = new Float32Array(cols * rows);
  const s = (t: number): number => t * t * (3 - 2 * t);
  for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
    const fx = c / scale, fy = r / scale, x0 = Math.floor(fx), y0 = Math.floor(fy), tx = s(fx - x0), ty = s(fy - y0);
    const a = lat[y0 * gx + x0], b = lat[y0 * gx + x0 + 1], d = lat[(y0 + 1) * gx + x0], e = lat[(y0 + 1) * gx + x0 + 1];
    out[r * cols + c] = (a + (b - a) * tx) * (1 - ty) + (d + (e - d) * tx) * ty;
  }
  return out;
}

/** Several octaves of noise, 0..1. */
function fractal(rng: Rng, cols: number, rows: number, scale: number, octaves = 3): Float32Array {
  const out = new Float32Array(cols * rows);
  let amp = 1, total = 0;
  for (let o = 0; o < octaves; o++) {
    const n = noise(rng, cols, rows, Math.max(2, scale / 2 ** o));
    for (let i = 0; i < out.length; i++) out[i] += n[i] * amp;
    total += amp; amp *= 0.5;
  }
  for (let i = 0; i < out.length; i++) out[i] /= total;
  return out;
}

/** The value below which 'share' of the cells fall. */
function cut(v: Float32Array, share: number): number {
  if (share <= 0) return -1;
  if (share >= 1) return 2;
  const s = Array.from(v).sort((a, b) => a - b);
  return s[Math.min(s.length - 1, Math.floor(share * s.length))];
}

export function buildField(place: LandPlace, seed: number): LandField {
  const { w, h, cell } = FIELD;
  const cols = Math.round(w / cell), rows = Math.round(h / cell), n = cols * rows;
  const rng = new Rng(seed ^ 0x5eed);
  const R = RECIPE[place.terrain] ?? RECIPE.plains;
  const at = (c: number, r: number): number => r * cols + c;
  const ground = new Array<number>(n).fill(G.open);
  const height = new Array<number>(n).fill(0);

  // Relief. Mountains get ridges running across the field and a lower pass up the middle.
  const lift = fractal(rng, cols, rows, 28, 4);
  for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
    let v = lift[at(c, r)];
    if (place.terrain === 'mountain') {
      const pass = Math.exp(-(((c - cols / 2) / (cols * 0.14)) ** 2));
      v = Math.max(0, v - 0.45 * pass) * 1.25;
    }
    if (place.terrain === 'desert') {
      // An escarpment: a step in the ground across the defender's half.
      v = v * 0.5 + (r > rows * 0.62 ? 0.5 : r > rows * 0.55 ? (r - rows * 0.55) / (rows * 0.07) * 0.5 : 0);
    }
    height[at(c, r)] = v * R.relief;
  }

  // Cover from independent noise fields, each taking its share of the ground.
  const coverOf = (share: number, scale: number, code: number, where: (c: number, r: number) => boolean = () => true): void => {
    if (share <= 0) return;
    const f = fractal(rng, cols, rows, scale, 3), k = cut(f, 1 - share);
    for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) if (f[at(c, r)] > k && where(c, r) && ground[at(c, r)] === G.open) ground[at(c, r)] = code;
  };
  coverOf(R.marsh, 12, G.marsh, (c, r) => height[at(c, r)] < R.relief * 0.55);
  coverOf(R.woods, 10, G.woods);
  coverOf(R.fields, 7, G.field);
  coverOf(R.rough, 9, G.rough);
  // Rock: the steepest ground.
  if (R.rock > 0) {
    const slope = new Float32Array(n);
    for (let r = 1; r < rows - 1; r++) for (let c = 1; c < cols - 1; c++) slope[at(c, r)] = Math.hypot(height[at(c + 1, r)] - height[at(c - 1, r)], height[at(c, r + 1)] - height[at(c, r - 1)]) / (2 * cell);
    const k = cut(slope, 1 - R.rock);
    for (let i = 0; i < n; i++) if (slope[i] > k && slope[i] > 0.12) ground[i] = G.rock;
  }

  // The town, in the defender's half: a village, a town, or for a city a district that fills the middle.
  let town: LandField['town'] = null;
  const size = place.terrain === 'urban' ? 'city' : place.townSize;
  if (size) {
    const rad = size === 'capital' ? 520 : size === 'city' ? 430 : size === 'town' ? 260 : 150;
    const tx = w * (0.4 + 0.2 * rng.next()), ty = h * (0.7 + 0.06 * rng.next());
    town = { x: tx, y: ty, r: rad };
    const dense = fractal(rng, cols, rows, 3, 2);
    for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
      const d = Math.hypot(c * cell + cell / 2 - tx, r * cell + cell / 2 - ty) / rad;
      if (d < 1 && dense[at(c, r)] > 0.25 + 0.45 * d) ground[at(c, r)] = G.buildings;
    }
    // The town sits on level ground.
    for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
      const d = Math.hypot(c * cell - tx, r * cell - ty) / (rad * 1.3);
      if (d < 1) height[at(c, r)] *= 0.4 + 0.6 * d;
    }
  }

  // Roads: one up the field from the attacker's side through the town, one across it.
  const road = (path: (t: number) => [number, number], wide = 1): void => {
    for (let i = 0; i <= 600; i++) {
      const [x, y] = path(i / 600);
      const c0 = Math.floor(x / cell), r0 = Math.floor(y / cell);
      for (let dc = 0; dc < wide; dc++) {
        const c = c0 + dc;
        if (c >= 0 && c < cols && r0 >= 0 && r0 < rows && ground[at(c, r0)] !== G.water) ground[at(c, r0)] = G.road;
      }
    }
  };
  const tx = town?.x ?? w / 2, ty = town?.y ?? h * 0.72;
  const wob = rng.range(-1, 1);
  road((t) => [w / 2 + (tx - w / 2) * Math.min(1, t * 1.4) + Math.sin(t * 5 + wob) * 90, t * h]);
  road((t) => [t * w, ty + Math.sin(t * 4 + wob) * 70]);

  // The river: across the field between the two armies, with its bridges where the roads cross it.
  if (place.river) {
    const base = h * (0.46 + 0.04 * rng.next()), amp = 60 + 50 * rng.next(), ph = rng.range(0, 6);
    for (let c = 0; c < cols; c++) {
      const yc = base + Math.sin((c / cols) * 7 + ph) * amp + Math.sin((c / cols) * 17 + ph * 2) * amp * 0.3;
      const half = 28 + 10 * Math.sin(c * 0.3 + ph);
      for (let r = 0; r < rows; r++) {
        const y = r * cell + cell / 2;
        if (Math.abs(y - yc) <= half) {
          const i = at(c, r);
          ground[i] = ground[i] === G.road ? (place.bridgeDown ? G.blown : G.bridge) : G.water;
          height[i] = Math.min(height[i], 0) - 2;
        } else if (Math.abs(y - yc) <= half + 40 && ground[at(c, r)] === G.open && place.terrain !== 'desert') ground[at(c, r)] = G.field;
      }
    }
  }

  // Fortress works: a line of bunkers across the front of the defender's half, thicker with the level.
  if (place.fort > 0) {
    const fy = h * 0.6, every = Math.round(10 / place.fort);
    for (let c = 2; c < cols - 2; c += every) {
      const r = Math.floor((fy + Math.sin(c * 0.2) * 40) / cell);
      for (const [dc, dr] of [[0, 0], [1, 0], [0, 1], [1, 1]]) {
        const i = at(c + dc, r + dr);
        if (ground[i] !== G.water && ground[i] !== G.bridge && ground[i] !== G.blown) ground[i] = G.bunker;
      }
    }
  }

  return {
    w, h, cell, cols, rows, ground, height,
    zones: { att: [w * 0.12, 120, w * 0.88, 480], def: [w * 0.12, h * 0.62, w * 0.88, h * 0.9] },
    town,
  };
}

/** Fieldworks dug where a defender stood dug in: the cells under its box become 'works'. */
export function digWorks(f: LandField, x: number, y: number, w: number, d: number): void {
  for (let yy = y - d / 2 - 15; yy <= y + d / 2 + 15; yy += f.cell / 2) for (let xx = x - w / 2; xx <= x + w / 2; xx += f.cell / 2) {
    const c = Math.floor(xx / f.cell), r = Math.floor(yy / f.cell);
    if (c < 0 || r < 0 || c >= f.cols || r >= f.rows) continue;
    const i = r * f.cols + c, k = GROUNDS[f.ground[i]];
    if (k !== 'water' && k !== 'bridge' && k !== 'blown' && k !== 'buildings' && k !== 'bunker') f.ground[i] = G.works;
  }
}

/** The ground and height under a point. */
export function groundAt(f: LandField, x: number, y: number): Ground {
  const c = Math.min(f.cols - 1, Math.max(0, Math.floor(x / f.cell))), r = Math.min(f.rows - 1, Math.max(0, Math.floor(y / f.cell)));
  return GROUNDS[f.ground[r * f.cols + c]];
}
export function heightAt(f: LandField, x: number, y: number): number {
  const c = Math.min(f.cols - 1, Math.max(0, Math.floor(x / f.cell))), r = Math.min(f.rows - 1, Math.max(0, Math.floor(y / f.cell)));
  return f.height[r * f.cols + c];
}
/** How steep the ground is at a point: rise over run. */
export function slopeAt(f: LandField, x: number, y: number): number {
  const d = f.cell;
  return Math.hypot(heightAt(f, x + d, y) - heightAt(f, x - d, y), heightAt(f, x, y + d) - heightAt(f, x, y - d)) / (2 * d);
}
