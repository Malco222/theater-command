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
// Then a city's own (Brian, 18 November, "if are attacking in hills than have that or a forrest or towns ect"; the art
// director's rings): its suburbs of houses and gardens, its industry, and its parks and squares.
export const GROUNDS = ['open', 'field', 'woods', 'rough', 'marsh', 'water', 'road', 'bridge', 'buildings', 'rock', 'bunker', 'works', 'blown', 'suburb', 'industry', 'park'] as const;
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
  /**
   * Where the record has the attack over its river at more places than the road's own bridge by the town: each a
   * share of the field's width, west to east, a bridge with its road down to either bank (b439). Where its bridges are
   * down they are down here too.
   */
  crossings?: number[];
  /**
   * Where some of the attack's corps come on from ground with no river between them and the place (ForceTicket.cross
   * false) and others cross: the end of the field those that do not cross take, the end on their side as the attack
   * faces the place (b464(5)). Absent there, the west.
   */
  dryEnd?: 'west' | 'east';
}

/**
 * The river bent away from the lanes of the attack's corps that do not cross it (the design director, b464(5)): it runs
 * across the field only before the lanes of those that cross, east of x where the others draw up to the west ('west'),
 * and at x turns to the attacker's edge and leaves the field there, between the two. The defence's whole line stays
 * on the far side of it, and the card's dry ground is the field's.
 */
export interface Bend { x: number; dry: 'west' | 'east' }

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
  /**
   * How many storeys the houses of each built cell stand ('buildings', 'suburb', 'industry'), the same order as ground;
   * 0 elsewhere. Looks only: the 3D game raises them so; the fight reads the ground alone.
   */
  storeys?: number[];
  /** A capital's or a city's one landmark, at its heart: a spire, a dome or a station (the art director). Looks only. */
  landmark?: { x: number; y: number; kind: 'spire' | 'dome' | 'station' };
  /**
   * What lay under each bunker cell before the bunker, by cell index: the screens paint that ground (a bunker in a wood
   * keeps its trees, and a bunker thinned off the screen leaves no trace, the art director), while the sim keeps the
   * bunker's cover.
   */
  beneath?: Record<number, number>;
  /**
   * What each fieldworks cell was dug into, by cell index: works give exactly that ground's own cover, since the card
   * prices the works in the defence's strength already (b418: what the card prices, the field draws but never prices
   * again). Painting is unaffected.
   */
  dugInto?: Record<number, number>;
  /**
   * Where there is a river, the metres north of its middle line in each column: which bank a point is on. 0 in a column
   * the river has bent away from (bend): the whole of it is the defence's bank.
   */
  riverMid?: number[];
  /** Where the river bends away from the lanes of the corps that do not cross it (b464(5)). */
  bend?: Bend;
  /** Where it bends, its middle line in metres, from where it comes onto the field to where it leaves at the attacker's edge. */
  course?: [number, number][];
}

export const FIELD = { w: 3000, h: 2000, cell: 25 };

/** Where each side forms up: the attacker's band along the south, the defender's along the north. */
export const zonesOf = (w: number, h: number): LandField['zones'] => ({ att: [w * 0.12, 120, w * 0.88, 480], def: [w * 0.12, h * 0.62, w * 0.88, h * 0.9] });

/**
 * The river's bend (b464(5)), in metres: the radius it turns on; how far its run down to the attacker's edge swings
 * either side of its line, and over what length; and how far either side of that line no attacking block draws up.
 */
export const BEND = { r: 180, swing: 30, wave: 520, clear: 130 };

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

export function buildField(place: LandPlace, seed: number, bend?: Bend): LandField {
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

  // The town, in the defender's half: a village or a town as a cluster about its centre; a capital or a city as streets
  // (layCity below). An urban province is a city at least.
  let town: LandField['town'] = null;
  const size = streetsOf(place) ?? place.townSize;
  let built: Pick<LandField, 'storeys' | 'landmark'> = {};
  if (size) {
    const rad = size === 'capital' ? 520 : size === 'city' ? 430 : size === 'town' ? 260 : 150;
    const tx = w * (0.4 + 0.2 * rng.next()), ty = h * (0.7 + 0.06 * rng.next());
    town = { x: tx, y: ty, r: rad };
    const dense = fractal(rng, cols, rows, 3, 2);
    if (size === 'capital' || size === 'city') built = layCity(size, tx, ty, cols, rows, ground, height, seed);
    else {
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
  // The river's line, drawn from the seed here, before the roads are laid (they draw nothing from it, so every field draws
  // what it always did): its middle in each column, metres north of the attacker's edge, and its half-width there.
  const flow = place.river ? { base: h * (0.46 + 0.04 * rng.next()), amp: 60 + 50 * rng.next(), ph: rng.range(0, 6) } : null;
  const midAt = (c: number): number => flow!.base + Math.sin((c / cols) * 7 + flow!.ph) * flow!.amp + Math.sin((c / cols) * 17 + flow!.ph * 2) * flow!.amp * 0.3;
  const halfAt = (c: number): number => 28 + 10 * Math.sin(c * 0.3 + flow!.ph);
  // Where the river bends (b464(5)): it runs straight across beyond the turn, on the side of those that cross.
  const away = bend?.dry === 'east' ? -1 : 1, turnX = bend ? bend.x + away * BEND.r : 0;
  const straight = (c: number): boolean => !bend || (c * cell + cell / 2 - turnX) * away >= 0;
  if (flow && bend) {
    // The road up the field comes up the middle of the lanes that cross, over the river by its bridge there, and only
    // then makes for the town: it never crosses the river where it runs down to the attacker's edge. Where those lanes
    // are few, it keeps its swing and the river's width clear of the turn, so it never runs up beside the river where it
    // comes down, nor bridges it where it comes round.
    const z = zonesOf(w, h).att, mid = (bend.x + (away > 0 ? z[2] : z[0])) / 2;
    const rx = away > 0 ? Math.max(mid, turnX + 130) : Math.min(mid, turnX - 130), yr = midAt(rx / cell - 0.5) + 150, yt = h * 0.71;
    road((t) => [rx + (tx - rx) * Math.min(1, Math.max(0, (t * h - yr) / (yt - yr))) + Math.sin(t * 5 + wob) * 60, t * h]);
  } else road((t) => [w / 2 + (tx - w / 2) * Math.min(1, t * 1.4) + Math.sin(t * 5 + wob) * 90, t * h]);
  road((t) => [t * w, ty + Math.sin(t * 4 + wob) * 70]);

  // The river: across the field between the two armies, with its bridges where the roads cross it.
  let riverMid: number[] | undefined, course: [number, number][] | undefined;
  if (flow) {
    riverMid = [];
    const over = new Set((place.crossings ?? []).map((x) => Math.min(cols - 1, Math.max(0, Math.floor((x * w) / cell)))));
    for (let c = 0; c < cols; c++) {
      const yc = midAt(c);
      riverMid.push(yc);
      if (!straight(c)) continue;
      const half = halfAt(c);
      for (let r = 0; r < rows; r++) {
        const y = r * cell + cell / 2;
        if (Math.abs(y - yc) <= half) {
          const i = at(c, r);
          ground[i] = ground[i] === G.road || over.has(c) ? (place.bridgeDown ? G.blown : G.bridge) : G.water;
          height[i] = Math.min(height[i], 0) - 2;
        } else if (over.has(c) && Math.abs(y - yc) <= half + 200) ground[at(c, r)] = G.road;
        else if (Math.abs(y - yc) <= half + 40 && ground[at(c, r)] === G.open && place.terrain !== 'desert') ground[at(c, r)] = G.field;
      }
    }
    if (bend) {
      // Where it bends (b464(5)): from the turn it comes round toward the lanes that do not cross and runs down to the
      // attacker's edge between the two groups of lanes, swinging a little either side of the line between them, a
      // river and not a ruler's line. Its own bridges stay where it runs across before those that cross: none where it
      // runs down, so nobody comes round it by the attacker's own bank.
      const R = BEND.r, c0 = turnX / cell - 0.5, y0 = midAt(c0), slope = (midAt(c0 + 0.5) - midAt(c0 - 0.5)) / cell;
      const tl = Math.hypot(1, slope), k = 0.55 * R, top = y0 - R;
      const P = [[turnX, y0], [turnX - (away * k) / tl, y0 - (away * slope * k) / tl], [bend.x, top + k], [bend.x, top]];
      const bez = (u: number, i: 0 | 1): number => (1 - u) ** 3 * P[0][i] + 3 * (1 - u) ** 2 * u * P[1][i] + 3 * (1 - u) * u ** 2 * P[2][i] + u ** 3 * P[3][i];
      const arc: [number, number][] = Array.from({ length: 49 }, (_, i) => [bez(i / 48, 0), bez(i / 48, 1)]);
      const sw = flow.ph > 3 ? 1 : -1, leg: [number, number][] = [];
      for (let d = 10, y = top - d; ; d += 10, y = Math.max(-60, top - d)) {
        leg.push([bend.x + sw * BEND.swing * Math.sin((2 * Math.PI * (top - y)) / BEND.wave) * Math.min(1, (top - y) / 150), y]);
        if (y <= -60) break;
      }
      const line = [...arc, ...leg];
      // Each point's distance along the bend from the turn, and so its half-width, going on from the straight stretch's.
      const along = [0];
      for (let i = 1; i < line.length; i++) along.push(along[i - 1] + Math.hypot(line[i][0] - line[i - 1][0], line[i][1] - line[i - 1][1]));
      const arcLen = along[arc.length - 1];
      const lo = Math.min(bend.x - away * 160, turnX + away * 60), hi = Math.max(bend.x - away * 160, turnX + away * 60);
      for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
        const x = c * cell + cell / 2, y = r * cell + cell / 2;
        if (x < lo || x > hi || y > y0 + 120) continue;
        let best = Infinity, sAt = 0;
        for (let i = 1; i < line.length; i++) {
          const [ax, ay] = line[i - 1], [bx, by] = line[i], vx = bx - ax, vy = by - ay;
          const q = Math.max(0, Math.min(1, ((x - ax) * vx + (y - ay) * vy) / (vx * vx + vy * vy || 1)));
          const dd = Math.hypot(x - ax - q * vx, y - ay - q * vy);
          if (dd < best) { best = dd; sAt = along[i - 1] + q * (along[i] - along[i - 1]); }
        }
        const half = halfAt(c0 - (away * sAt) / cell), i = at(c, r), g0 = ground[i];
        if (g0 === G.water || g0 === G.bridge || g0 === G.blown) continue;
        if (best <= half) {
          ground[i] = g0 === G.road && sAt <= arcLen ? (place.bridgeDown ? G.blown : G.bridge) : G.water;
          height[i] = Math.min(height[i], 0) - 2;
        } else if (best <= half + 40 && g0 === G.open && place.terrain !== 'desert') ground[i] = G.field;
      }
      // Which bank: the bend's own line where it comes round before the lanes that cross; none at all beyond it.
      for (let c = 0; c < cols; c++) {
        if (straight(c)) continue;
        const x = c * cell + cell / 2;
        riverMid[c] = (x - bend.x) * away < 0 ? 0 : arc.reduce((m, p) => (Math.abs(p[0] - x) <= cell / 2 ? Math.max(m, p[1]) : m), top);
      }
      course = [];
      for (let x = away > 0 ? w : 0; (x - turnX) * away > 0; x -= away * cell) course.push([x, midAt(x / cell - 0.5)]);
      course.push(...line);
    }
  }
  // A road on the slant steps from cell to cell corner to corner, and where both cells beside such a step are rock, marsh
  // or water a lorry cannot take it: the road takes one of them, over water the bridge does (b418: tanks and lorries
  // stood for good where their road crossed a river or a ridge on the slant).
  const span = place.bridgeDown ? G.blown : G.bridge;
  const deck = (k: number): boolean => k === G.road || k === G.bridge || k === G.blown;
  const hard = (k: number): boolean => k === G.rock || k === G.marsh || k === G.water || k === G.bunker;
  for (let r = 0; r < rows - 1; r++) for (let c = 0; c < cols; c++) {
    if (!deck(ground[at(c, r)])) continue;
    for (const dc of [-1, 1]) {
      if (c + dc < 0 || c + dc >= cols || !deck(ground[at(c + dc, r + 1)])) continue;
      const p = at(c + dc, r), q = at(c, r + 1);
      if (!hard(ground[p]) || !hard(ground[q])) continue;
      const k = ground[p] === G.water && ground[q] !== G.water ? q : p;
      ground[k] = ground[k] === G.water ? span : G.road;
    }
  }

  // Fortress works: bunkers along the front edge of the line the defenders draw up on, across their zone and a little
  // past it, thicker with the level, so the defenders stand in their works (the art director, ruling 226(12)).
  const beneath: Record<number, number> = {};
  if (place.fort > 0) {
    // The defenders' front row stands 40 m inside their zone (deploy in units.ts); its front edge 20 m before that.
    const fy = h * 0.62 + 40 - 20 - cell, every = Math.round(10 / place.fort);
    for (let c = Math.floor((w * 0.1) / cell); c < Math.min(cols - 2, Math.ceil((w * 0.9) / cell)); c += every) {
      const r = Math.floor(fy / cell);
      for (const [dc, dr] of [[0, 0], [1, 0], [0, 1], [1, 1]]) {
        const i = at(c + dc, r + dr);
        if (ground[i] !== G.water && ground[i] !== G.bridge && ground[i] !== G.blown) { if (ground[i] !== G.bunker) beneath[i] = ground[i]; ground[i] = G.bunker; }
      }
    }
  }

  // Storeys for every house still standing after the roads, the river and the works: a village's and a town's one or
  // two, from the cell's own place so no draw moves.
  let storeys = built.storeys;
  if (ground.some((k) => k === G.buildings || k === G.suburb || k === G.industry)) {
    storeys ??= new Array<number>(n).fill(0);
    for (let i = 0; i < n; i++) {
      const k = ground[i];
      if (k !== G.buildings && k !== G.suburb && k !== G.industry) storeys[i] = 0;
      else if (!storeys[i]) storeys[i] = 1 + (Math.imul(i + 1, 0x9e3779b1) >>> 31);
    }
  } else storeys = undefined;

  return {
    w, h, cell, cols, rows, ground, height, beneath, riverMid,
    zones: zonesOf(w, h),
    town,
    ...(storeys ? { storeys } : {}),
    ...(built.landmark ? { landmark: built.landmark } : {}),
    ...(course ? { bend, course } : {}),
  };
}

/** Whether the place is fought in a capital's or a city's streets (layCity): an urban province is a city at least. */
export function streetsOf(place: LandPlace): 'capital' | 'city' | null {
  if (place.townSize === 'capital') return 'capital';
  return place.terrain === 'urban' || place.townSize === 'city' ? 'city' : null;
}

/**
 * The streets of a capital or a city (Brian, 18 November: the battle's ground matches the map, "towns ect"; the design
 * director, b511: the share of streets follows the city's size on the map, a capital or a great city mostly streets; the
 * art director's rings, 17:23Z). From the heart out: a core of blocks three to five storeys high on its avenues, with
 * its squares and parks; a ring of terraces two storeys high along their streets; then suburbs of houses and gardens with
 * open ground between; and one patch of industry at the core's side.
 *   - A capital: the core fills the defender's part of the field, from 60 per cent of the way up it to its far edge,
 *     terraces 350 m before it, and suburbs from there back to the attacker's start, so the fight is in its streets from
 *     the first minute. About 70 per cent of the field built.
 *   - A city: a core about 800 by 600 m round the town's centre, terraces 150 m about it, suburbs out to the middle of the
 *     field. About a third of the field built.
 * Roads, the river and fortress works are laid over it after, as on every field. Its own stream of draws, so the rest
 * of the field (its roads, its river) is as it was. Returns the storeys of what it built and its landmark.
 */
function layCity(size: 'capital' | 'city', tx: number, ty: number, cols: number, rows: number, ground: number[], height: number[], seed: number): Pick<LandField, 'storeys' | 'landmark'> {
  const { w, h, cell } = FIELD, n = cols * rows;
  const cr = new Rng(seed ^ 0xc17e);
  const edge = fractal(cr, cols, rows, 6, 2), fill = fractal(cr, cols, rows, 2, 2);
  const storeys = new Array<number>(n).fill(0);
  const core = new Uint8Array(n);
  const capital = size === 'capital', front = h * 0.6;
  for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
    const i = r * cols + c, x = c * cell + cell / 2, y = r * cell + cell / 2, e = (edge[i] - 0.5) * 120;
    // Which ring the cell is in: 3 core, 2 terraces, 1 suburbs, 0 outside the city.
    let ring = 0;
    if (capital) {
      const margin = w * 0.05 + e * 0.6;
      ring = y >= front + e && x > margin && x < w - margin ? 3 : y >= front - 350 + e ? 2 : y >= 90 ? 1 : 0;
    } else {
      // Rounded boxes about the centre (|dx|^4 + |dy|^4 < 1); the suburbs reach back to the middle of the field.
      const box = (a: number, b: number): number => ((x - tx) / a) ** 4 + ((y - ty) / b) ** 4;
      const back = Math.max(300, ty - h * 0.5 + e);
      ring = box(400 + e, 300 + e) < 1 ? 3 : box(580 + e, 480 + e) < 1 ? 2 : (((x - tx) / (1150 + e)) ** 2 + ((y - ty) / back) ** 2 < 1 || (y > ty && Math.abs(x - tx) < 1150 + e)) ? 1 : 0;
    }
    if (ring === 3) {
      ground[i] = G.buildings; core[i] = 1;
      storeys[i] = 3 + (fill[i] > 0.5 ? 1 : 0) + (Math.hypot(x - tx, y - ty) < 350 ? 1 : 0);
    } else if (ring === 2 && fill[i] > 0.28) { ground[i] = G.buildings; storeys[i] = 2; }
    else if (ring === 1 && fill[i] > (capital ? 0.4 : 0.3)) { ground[i] = G.suburb; storeys[i] = 1 + (fill[i] > 0.7 ? 1 : 0); }
    // The city stands on level ground: its core and terraces flatter still than its suburbs.
    if (ring) height[i] *= ring === 1 ? 0.7 : 0.4;
  }
  const inCore = (c: number, r: number): boolean => c >= 0 && r >= 0 && c < cols && r < rows && core[r * cols + c] === 1;
  // Avenues through the core, about every 500 m up it and 250 m across it, through the centre.
  const c0 = Math.round(tx / cell), r0 = Math.round(ty / cell), ac = Math.round(500 / cell), ar = Math.round(250 / cell);
  for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
    if (!inCore(c, r) || !(((c - c0) % ac === 0) || ((r - r0) % ar === 0))) continue;
    ground[r * cols + c] = G.road; storeys[r * cols + c] = 0;
  }
  // Squares and parks: open patches in the core, two in a city, three in a capital.
  for (let k = 0; k < (capital ? 3 : 2); k++) {
    for (let tries = 0; tries < 20; tries++) {
      const pc = Math.floor(cr.next() * cols), pr = Math.floor(cr.next() * rows), pw = 3 + Math.floor(cr.next() * 2), ph = 3 + Math.floor(cr.next() * 2);
      if (!inCore(pc, pr) || !inCore(pc + pw - 1, pr + ph - 1)) continue;
      for (let r = pr; r < pr + ph; r++) for (let c = pc; c < pc + pw; c++) if (ground[r * cols + c] === G.buildings) { ground[r * cols + c] = G.park; storeys[r * cols + c] = 0; }
      break;
    }
  }
  // Industry: one patch of sheds and yards against the core's side, west or east, toward the attacker.
  const iw = capital ? 12 : 8, ih = capital ? 6 : 5, side = cr.next() < 0.5 ? -1 : 1;
  for (let c = c0 + side * (capital ? 30 : 16) - (side < 0 ? iw : 0), k = 0; k < iw; k++, c++) {
    for (let r = Math.round((capital ? front : ty - 300) / cell) - 2, j = 0; j < ih; j++, r++) {
      if (c < 0 || r < 0 || c >= cols || r >= rows) continue;
      const i = r * cols + c;
      if (ground[i] !== G.road) { ground[i] = G.industry; storeys[i] = 2; }
    }
  }
  // Its landmark, on the core's cell nearest the centre.
  let best = -1, bd = Infinity;
  for (let i = 0; i < n; i++) {
    if (ground[i] !== G.buildings || !core[i]) continue;
    const d = Math.hypot((i % cols) * cell + cell / 2 - tx, Math.floor(i / cols) * cell + cell / 2 - ty);
    if (d < bd) { bd = d; best = i; }
  }
  const kinds = ['spire', 'dome', 'station'] as const;
  const landmark = best < 0 ? undefined : { x: (best % cols) * cell + cell / 2, y: Math.floor(best / cols) * cell + cell / 2, kind: kinds[Math.floor(cr.next() * 3)] };
  return { storeys, ...(landmark ? { landmark } : {}) };
}

/** Fieldworks dug where a defender stood dug in: the cells under its box become 'works'. */
export function digWorks(f: LandField, x: number, y: number, w: number, d: number): void {
  for (let yy = y - d / 2 - 15; yy <= y + d / 2 + 15; yy += f.cell / 2) for (let xx = x - w / 2; xx <= x + w / 2; xx += f.cell / 2) {
    const c = Math.floor(xx / f.cell), r = Math.floor(yy / f.cell);
    if (c < 0 || r < 0 || c >= f.cols || r >= f.rows) continue;
    const i = r * f.cols + c, k = GROUNDS[f.ground[i]];
    if (k !== 'water' && k !== 'bridge' && k !== 'blown' && k !== 'buildings' && k !== 'industry' && k !== 'bunker' && k !== 'works') {
      (f.dugInto ??= {})[i] = f.ground[i];
      f.ground[i] = G.works;
    }
  }
}

/** The ground a blow lands on for its cover: fieldworks count as what they were dug into. */
export function coverAt(f: LandField, x: number, y: number): Ground {
  const c = Math.min(f.cols - 1, Math.max(0, Math.floor(x / f.cell))), r = Math.min(f.rows - 1, Math.max(0, Math.floor(y / f.cell)));
  const i = r * f.cols + c, k = f.ground[i];
  return GROUNDS[k === G.works && f.dugInto?.[i] !== undefined ? f.dugInto[i] : k];
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
