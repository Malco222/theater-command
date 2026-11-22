// The field: a province's ground, built for one battle.
//
// Made, not placed: everything comes from what the campaign knows about the province (its terrain, a river between
// the armies and whether its bridges are down, its town and how big it is, its fortress works) and from the battle's
// seed, so the same attack always gives the same field. The field is a grid of cells; each cell has a ground kind and
// a height. The 3D game builds its relief and paints its ground from exactly these two lists.
//
// Frame (see core/types.ts): metres, x east, y north, the attacker comes on from the south.

import { hashString, Rng } from '../../core/rng';
import { LAND } from './rules';
import planData from '../../../data/battle/city-plans.json';

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
  /**
   * A capital's or a city's own river through its streets, as it lies to this attack (the design director, b526): never
   * between the armies. From the hand list of real cities (data/battle/city-rivers.json, stream.ts). None where the card
   * prices a crossing (river): one river a field.
   */
  stream?: Stream;
}

/**
 * A city's own river as it lies to the attack (b526). 'up': along the attack's line, up the field through the city's
 * core, a bridge on every avenue; 'across': across it, through the core's back half behind the defence (as Praga across
 * the Vistula, or Buda across the Danube); 'shore': along the city's edge at one side of the field, not through it (the
 * Tagus along Lisbon). The field's west is the attack's left.
 */
export interface Stream {
  name: string;
  lie: 'up' | 'across' | 'shore';
  /** Which way its water runs on the field: up or down the field ('north', 'south') along it, 'east' or 'west' across it. */
  flows: 'north' | 'south' | 'east' | 'west';
  /** A shore's side of the field: 'back' along its far edge, behind the defence (b595). */
  side?: 'west' | 'east' | 'back';
  /**
   * A city that runs along its shore as a strip (Stalingrad along the Volga; the art director, 19 Nov): with the water
   * behind the defence, its streets run the field's whole width along the bank and reach only a little way inland.
   */
  strip?: boolean;
  /** Wider than about 250 m at the city (the Danube at Budapest), drawn wider. */
  great: boolean;
  /** A second river meeting it at the city (the Sava at Belgrade): its name and the side of the field it comes in from. */
  join?: { name: string; side: 'west' | 'east'; bridges?: number };
  /**
   * Its road bridges in the war years (b596, the historian's record; none where it had none): the field lays that many,
   * those nearest the heart, every other avenue ending at a quay. Left out, a bridge on every avenue.
   */
  bridges?: number;
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
   * A capital's or a city's core: in each column, the metres north at which its blocks begin (the core's ragged edge on
   * the attacker's side), -1 in a column it does not reach. The defence draws up a block inside it (b529).
   */
  streetEdge?: number[];
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
  /**
   * A capital's or a city's own river (b526): its name and how it lies, its middle line in metres from upstream to
   * downstream, and how far either side of that line it is water (half, a little more and less as it goes); a shore's
   * line is its bank, the water lying between it and the field's edge (half 0). Where two meet, the second's likewise,
   * from the field's edge to where it meets the first. Its water and its bridges are in ground: a river in the rules
   * like any other, but not on the card, so never between the armies. One across the field, or a shore along its back
   * edge, keeps behind the defence's front line as it read it (front, metres north): the defence draws up no deeper.
   */
  stream?: FieldStream;
}

export interface FieldStream {
  name: string;
  lie: Stream['lie'];
  /** A shore's side of the field ('back': along the field's far edge, behind the defence). */
  side?: 'west' | 'east' | 'back';
  /** Its city laid as a strip along it (Stream.strip): the field's whole width, shallow. */
  strip?: boolean;
  great: boolean;
  half: number;
  course: [number, number][];
  join?: { name: string; half: number; course: [number, number][] };
  front?: number;
}

export const FIELD = { w: 3000, h: 2000, cell: 25 };

/** Where each side forms up: the attacker's band along the south, the defender's along the north. */
export const zonesOf = (w: number, h: number): LandField['zones'] => ({ att: [w * 0.12, 120, w * 0.88, 480], def: [w * 0.12, h * 0.62, w * 0.88, h * 0.9] });

/**
 * The river's bend (b464(5)), in metres: the radius it turns on; how far its run down to the attacker's edge swings
 * either side of its line, and over what length; and how far either side of that line no attacking block draws up.
 */
export const BEND = { r: 180, swing: 30, wave: 520, clear: 130 };

/**
 * A city's own river (b526), in metres: half its width, great or lesser; how far it swings as it goes; how wide a shore's
 * water is from the field's edge; how far behind the defence's front line it keeps, and a second river that meets it
 * further still (the defence's rows reach 230 m back and its guns stand 50 m deep, deeper where its lanes are narrow); the far bank it leaves at the
 * field's edge; how far either side of its line a block draws up clear of it; and how far it keeps from the roads up and
 * across the field, which it bridges and never runs along; and the stone's throw a block stands off its water and bridges.
 * Its water reaches half its width and a little more as it goes (wobble: up to an eighth more).
 */
/**
 * A city along its shore as a strip (Stream.strip), in metres inland from where its water begins: its core, then its
 * terraces, then its suburbs. About 800 m in all against the field's 3000 m width, as Stalingrad ran 20 km along the Volga
 * and 2 to 5 km deep.
 */
export const STRIP = { core: 380, terraces: 170, suburbs: 250 };
export const STREAM = { great: 55, lesser: 35, swing: 45, shore: 230, behind: 330, joinBehind: 420, far: 60, clear: 75, road: 90, off: 60, wobble: 0.12 };

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
  // A real city's street plan is a fact of the place (the design director, b618): laid from the town's own seed, the
  // same every time it is fought and never another city's: where it stands on the field, its streets, and the two roads
  // through it. The battle's own draws are still taken, so everything else on the field draws as it did. A practice
  // ground's town is no place on the map, and keeps the battle's seed.
  const plan = planSeed(place, seed), own = plan !== seed && (size === 'capital' || size === 'city'), pr = new Rng(plan ^ 0x7a11);
  let built: Pick<LandField, 'storeys' | 'landmark' | 'streetEdge'> = {};
  if (size) {
    const rad = size === 'capital' ? 520 : size === 'city' ? 430 : size === 'town' ? 260 : 150;
    // A city along its shore as a strip has its heart near the water, in the middle of its depth.
    const s = place.stream, strip = size === 'city' && s?.lie === 'shore' && s.side === 'back' && s.strip ? h - (s.great ? STREAM.shore : STREAM.shore * 0.7) : 0;
    const ux = rng.next(), uy = rng.next();
    const tx = w * (0.4 + 0.2 * (own ? pr.next() : ux)), ty0 = h * (0.7 + 0.06 * (own ? pr.next() : uy)), ty = strip ? strip - STRIP.core / 2 : ty0;
    town = { x: tx, y: ty, r: rad };
    const dense = fractal(rng, cols, rows, 3, 2);
    if (size === 'capital' || size === 'city') built = layCity(size, tx, ty, cols, rows, ground, height, plan, strip);
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
  const wob0 = rng.range(-1, 1), wob = own ? pr.range(-1, 1) : wob0;
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
  // A capital's or a city's own river (b526), laid over its streets after the roads, which it bridges; never where the
  // card prices a crossing (one river a field). Its own stream of draws, so the rest of the field is as it was.
  let stream: FieldStream | undefined, cityWater: boolean[] | undefined;
  if (place.stream && !flow && town && built.landmark) {
    const roadX = (y: number): number => w / 2 + (tx - w / 2) * Math.min(1, (y / h) * 1.4) + Math.sin((y / h) * 5 + wob) * 90;
    const roadY = (x: number): number => ty + Math.sin((x / w) * 4 + wob) * 70;
    const dry = ground.map((k) => k !== G.water);
    stream = layStream(place.stream, seed, place.terrain, ground, height, built.landmark, town, built.streetEdge, roadX, roadY);
    cityWater = dry.map((d, i) => d && ground[i] === G.water);
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
      // A city's own river keeps the record's bridges (b596): no new deck over its water but one beside a bridge it has.
      if (cityWater?.[k] && ![-1, 0, 1].some((a) => [-1, 0, 1].some((b) => ground[at(Math.min(cols - 1, Math.max(0, (k % cols) + a)), Math.min(rows - 1, Math.max(0, Math.floor(k / cols) + b)))] === G.bridge))) continue;
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
    ...(built.streetEdge ? { streetEdge: built.streetEdge } : {}),
    ...(course ? { bend, course } : {}),
    ...(stream ? { stream } : {}),
  };
}

/** A quadratic curve from a to b about k, as points. */
function bezier(a: [number, number], k: [number, number], b: [number, number], steps = 40): [number, number][] {
  return Array.from({ length: steps + 1 }, (_, i) => {
    const u = i / steps, v = 1 - u;
    return [v * v * a[0] + 2 * v * u * k[0] + u * u * b[0], v * v * a[1] + 2 * v * u * k[1] + u * u * b[1]] as [number, number];
  });
}

/**
 * A capital's or a city's own river through its streets (Brian, 18 November: the ground matches the map; the design
 * director, b526), never between the armies:
 *   - 'up', along the attack's line: up the field through the core, beside its heart on the side away from the road up
 *     the field and clear of that road all the way (failing either side, across instead);
 *   - 'across' it: through the core's back half, behind the defence's rows (a block inside the core's edge, b529) and
 *     the road across the field, the city's far quarter beyond it, as Praga across the Vistula or Buda across the Danube;
 *   - 'shore': along the city's edge at one side of the field, its water to the field's edge, never through it.
 * A second river meeting it (the Sava at Belgrade) never lies between the armies either: to meet one 'across', from its
 * side edge to the heart's corner, beyond the defence's flank (b596(2)); to meet one 'up' the field, from its side of
 * the field behind the defence's rows. A road it crosses keeps its way over by a bridge straight across the water where
 * the road came to it, as many as the city had (b596(1), the record's count; none given, a bridge on every avenue),
 * those nearest the heart; every other avenue ends at its quay, and a road that only comes down to the water ends at its
 * bank. A river with no road over it gets one bridge, nearest the heart, unless the record says it had none. Water is
 * water in the rules (b526: what is drawn as a river is a river), but the card never prices it, so it lies where no
 * attack must cross it.
 */
function layStream(
  s: Stream, seed: number, terrain: Terrain, ground: number[], height: number[], heart: { x: number; y: number }, town: { x: number; y: number },
  streetEdge: number[] | undefined, roadX: (y: number) => number, roadY: (x: number) => number,
): FieldStream {
  const { w, h, cell } = FIELD, cols = Math.round(w / cell), rows = Math.round(h / cell), z = zonesOf(w, h);
  const rng = new Rng(seed ^ 0x57ea);
  const half = s.great ? STREAM.great : STREAM.lesser, jh = STREAM.lesser;
  // Where the defence's front line stands: a block inside the core's ragged edge (b529), never nearer the attack than
  // its zone; read deep (the edge three-quarters of the way back about the defence's middle), so it keeps behind every row.
  const dcx = Math.min(z.def[2] - 300, Math.max(z.def[0] + 300, town.x));
  const edge = (streetEdge ?? []).filter((y, c) => y >= 0 && Math.abs(c * cell + cell / 2 - dcx) <= 700).sort((a, b) => a - b);
  const front = Math.max(z.def[1] + 40, edge.length ? edge[Math.floor(edge.length * 0.75)] + LAND.city.inside : 0);
  // Its slow swing as it goes, 0 to 1.
  const L = rng.range(1500, 2300), p1 = rng.range(0, Math.PI * 2), p2 = rng.range(0, Math.PI * 2);
  const wave = (v: number): number => 0.5 + 0.35 * Math.sin((2 * Math.PI * v) / L + p1) + 0.15 * Math.sin((2 * Math.PI * v) / (L * 0.37) + p2);
  const far = (hw: number): number => h - hw - STREAM.far;
  const jside = s.join?.side === 'east' ? 1 : -1;
  const course: [number, number][] = [];
  let lie = s.lie, join: FieldStream['join'];

  if (lie === 'shore' && s.side === 'back') {
    // Its water straight ahead of the attack: along the field's back edge, behind the defence's rows as a river across
    // the field keeps (b595), its bank no nearer than that, and at least the far stone's throw of water on the field.
    const width = s.great ? STREAM.shore : STREAM.shore * 0.7;
    const bank = (x: number): number => Math.min(h - STREAM.far, Math.max(front + STREAM.behind, h - width + 35 * (2 * wave(x) - 1)));
    for (let x = -50; x <= w + 50; x += cell) course.push([x, bank(x)]);
    if (s.flows === 'west') course.reverse();
    for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
      const x = c * cell + cell / 2, y = r * cell + cell / 2, i = r * cols + c, d = y - bank(x);
      if (d >= 0) { ground[i] = G.water; height[i] = Math.min(height[i], 0) - 2; }
      else if (d > -40 && ground[i] === G.open && terrain !== 'desert') ground[i] = G.field;
    }
    return { name: s.name, lie, side: 'back', ...(s.strip ? { strip: true } : {}), great: s.great, half: 0, course, front };
  }
  if (lie === 'shore') {
    const west = s.side !== 'east', width = s.great ? STREAM.shore : STREAM.shore * 0.7;
    const bank = (y: number): number => width + 35 * (2 * wave(y) - 1);
    for (let y = -50; y <= h + 50; y += cell) course.push([west ? bank(y) : w - bank(y), y]);
    if (s.flows === 'south') course.reverse();
    for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
      const x = c * cell + cell / 2, y = r * cell + cell / 2, i = r * cols + c, d = west ? bank(y) - x : x - (w - bank(y));
      if (d >= 0) { ground[i] = G.water; height[i] = Math.min(height[i], 0) - 2; }
      else if (d > -40 && ground[i] === G.open && terrain !== 'desert') ground[i] = G.field;
    }
    return { name: s.name, lie, side: west ? 'west' : 'east', great: s.great, half: 0, course };
  }
  // How many cells of road a line would drown: of the lines it may take, it takes the one that drowns fewest, so it
  // crosses the avenues rather than running along one.
  const drowns = (f: (v: number) => number, up: boolean): number => {
    const hit = new Set<number>(), k = Math.ceil(half / cell);
    for (let v = -50; v <= (up ? h : w) + 50; v += cell / 2) {
      const x = up ? f(v) : v, y = up ? v : f(v), c0 = Math.floor(x / cell), r0 = Math.floor(y / cell);
      for (let dr = -k; dr <= k; dr++) for (let dc = -k; dc <= k; dc++) {
        const c = c0 + dc, r = r0 + dr;
        if (c < 0 || r < 0 || c >= cols || r >= rows || Math.hypot(c * cell + cell / 2 - x, r * cell + cell / 2 - y) > half) continue;
        if (ground[r * cols + c] === G.road) hit.add(r * cols + c);
      }
    }
    return hit.size;
  };
  // The same for a second river's course: it comes in where it drowns fewest cells of road, never along an avenue.
  const drownsPts = (pts: [number, number][], hw: number): number => {
    const hit = new Set<number>(), k = Math.ceil(hw / cell);
    for (let n = 1; n < pts.length; n++) for (let t = 0; t < 1; t += 0.25) {
      const x = pts[n - 1][0] + (pts[n][0] - pts[n - 1][0]) * t, y = pts[n - 1][1] + (pts[n][1] - pts[n - 1][1]) * t, c0 = Math.floor(x / cell), r0 = Math.floor(y / cell);
      for (let dr = -k; dr <= k; dr++) for (let dc = -k; dc <= k; dc++) {
        const c = c0 + dc, r = r0 + dr;
        if (c < 0 || r < 0 || c >= cols || r >= rows || Math.hypot(c * cell + cell / 2 - x, r * cell + cell / 2 - y) > hw) continue;
        if (ground[r * cols + c] === G.road) hit.add(r * cols + c);
      }
    }
    return hit.size;
  };
  const fewest = (ways: [number, number][][]): [number, number][] => ways.reduce((a, b) => (drownsPts(b, jh) < drownsPts(a, jh) ? b : a));
  if (lie === 'up') {
    const pref = town.x > w / 2 ? 1 : -1;
    const line = (x0: number) => (y: number): number => x0 + STREAM.swing * 1.2 * (2 * wave(y) - 1);
    let xr: ((y: number) => number) | null = null, least = Infinity;
    for (const side of [pref, -pref]) for (let off = 200; off <= 400; off += 25) {
      const f = line(heart.x + side * off);
      let ok = true;
      for (let y = -50; y <= h + 50 && ok; y += cell) {
        const x = f(y);
        ok = Math.abs(x - roadX(Math.min(h, Math.max(0, y)))) >= half + STREAM.road && x >= z.att[0] + 200 && x <= z.att[2] - 200;
      }
      const n = ok ? drowns(f, true) : Infinity;
      if (n < least) { least = n; xr = f; }
    }
    if (xr) {
      for (let y = -50; y <= h + 50; y += cell) course.push([xr(y), y]);
      if (s.flows === 'south') course.reverse();
      if (s.join) {
        const yc = Math.min(far(jh), front + STREAM.joinBehind + jh), xc = xr(yc), ex = jside < 0 ? -50 : w + 50;
        const da = rng.range(40, 160), dk = rng.range(0, 80);
        const way = (a: number, k: number): [number, number][] => bezier([ex, Math.min(h + 50, yc + a)], [(ex + xc) / 2, Math.min(h + 50, yc + k)], [xc, yc]);
        join = { name: s.join.name, half: jh, course: fewest([way(da, dk), ...[40, 100, 160, 220].flatMap((a) => [way(a, 0), way(a, a / 2)])]) };
      }
    } else lie = 'across';
  }
  if (lie === 'across') {
    const line = (base: number) => (x: number): number => Math.min(far(half), Math.max(base, roadY(Math.min(w, Math.max(0, x))) + STREAM.road + half) + STREAM.swing * wave(x));
    let yr = line(front + STREAM.behind + half), least = drowns(yr, false);
    for (let k = 1; k <= 6; k++) {
      const f = line(front + STREAM.behind + half + k * cell), n = drowns(f, false);
      if (n < least) { least = n; yr = f; }
    }
    for (let x = -50; x <= w + 50; x += cell) course.push([x, yr(x)]);
    if (s.flows === 'west') course.reverse();
    if (s.join) {
      // The second comes in from its own side edge (b596: the Sava from the west at Belgrade), behind the defence's front
      // line, and turns up along the defence's flank to meet the first at the heart's corner: the defence draws up on
      // the heart's side of it, its flank on the water, the city with water on two sides; never between the armies.
      const d0 = rng.range(260, 420), e0 = rng.range(60, 200), k0 = rng.range(0, 120);
      const way = (d: number, e: number, k: number): [number, number][] => {
        const xc = heart.x + jside * d, yc = yr(xc), ye = Math.min(yc - 2 * jh - 2 * cell, front + e);
        return bezier([jside < 0 ? -50 : w + 50, ye], [xc + jside * k, ye + (yc - ye) * 0.1], [xc, yc]);
      };
      join = { name: s.join.name, half: jh, course: fewest([way(d0, e0, k0), ...[260, 340, 420].flatMap((d) => [way(d, 80, 40), way(d, 140, 80), way(d, 200, 0)])]) };
    }
  }

  const lay = (pts: [number, number][], hw: number, cap: number): void => {
    // The roads as they were before this water: those it crosses keep their way over (a second river's too, the roads
    // the first one's bridges were given among them).
    const road0 = ground.map((k) => k === G.road);
    const wob = rng.range(0, Math.PI * 2), m = hw * 1.15 + 40;
    const along = [0];
    for (let k = 1; k < pts.length; k++) along.push(along[k - 1] + Math.hypot(pts[k][0] - pts[k - 1][0], pts[k][1] - pts[k - 1][1]));
    const xs = pts.map((p) => p[0]), ys = pts.map((p) => p[1]);
    const c0 = Math.max(0, Math.floor((Math.min(...xs) - m) / cell)), c1 = Math.min(cols - 1, Math.ceil((Math.max(...xs) + m) / cell));
    const r0 = Math.max(0, Math.floor((Math.min(...ys) - m) / cell)), r1 = Math.min(rows - 1, Math.ceil((Math.max(...ys) + m) / cell));
    // Each cell near it: how far from its line, on which bank (+1 its left as it flows, -1 its right), and how far along.
    const dist = new Map<number, number>(), bank = new Map<number, number>(), sta = new Map<number, number>();
    const wet = new Set<number>();
    for (let r = r0; r <= r1; r++) for (let c = c0; c <= c1; c++) {
      const x = c * cell + cell / 2, y = r * cell + cell / 2, i = r * cols + c;
      // A second river stops where the first's water begins.
      if (ground[i] === G.water) continue;
      let best = Infinity, sAt = 0, sd = 0;
      for (let k = 1; k < pts.length; k++) {
        const [ax, ay] = pts[k - 1], vx = pts[k][0] - ax, vy = pts[k][1] - ay;
        const q = Math.max(0, Math.min(1, ((x - ax) * vx + (y - ay) * vy) / (vx * vx + vy * vy || 1)));
        const dd = Math.hypot(x - ax - q * vx, y - ay - q * vy);
        if (dd < best) { best = dd; sAt = along[k - 1] + q * (along[k] - along[k - 1]); sd = vx * (y - ay) - vy * (x - ax) >= 0 ? 1 : -1; }
      }
      if (best > m) continue;
      dist.set(i, best); bank.set(i, sd); sta.set(i, sAt);
      // The first river's bridge: never a second's water, but a bank its roads come to (an avenue over the first river
      // and on over the second keeps its way over both).
      if (ground[i] === G.bridge) continue;
      if (best <= hw * (1 + STREAM.wobble * Math.sin(sAt / 170 + wob))) wet.add(i);
      else if (best <= hw + 40 && ground[i] === G.open && terrain !== 'desert') ground[i] = G.field;
    }
    for (const i of wet) { ground[i] = G.water; height[i] = Math.min(height[i], 0) - 2; }
    const mid = (j: number): [number, number] => [(j % cols) * cell + cell / 2, Math.floor(j / cols) * cell + cell / 2];
    const cellOf = (x: number, y: number): number => { const c = Math.floor(x / cell), r = Math.floor(y / cell); return c < 0 || r < 0 || c >= cols || r >= rows ? -1 : r * cols + c; };
    // Road from one point to another; over the water a bridge, or, where it keeps to its own bank (land), nothing there.
    const pave = (x0: number, y0: number, x1: number, y1: number, land = false): void => {
      const steps = Math.max(1, Math.ceil(Math.hypot(x1 - x0, y1 - y0) / (cell / 4)));
      for (let t = 0; t <= steps; t++) {
        const j = cellOf(x0 + ((x1 - x0) * t) / steps, y0 + ((y1 - y0) * t) / steps);
        if (j < 0) continue;
        if (wet.has(j)) { if (!land) ground[j] = G.bridge; }
        else if (ground[j] !== G.road && ground[j] !== G.bridge && ground[j] !== G.water) ground[j] = G.road;
      }
    };
    // A bridge straight over the water at s metres along it; its land at either end laid as road to the nearest of the
    // road's cells on that bank (ends: -1 its right bank, +1 its left, which lies along the normal (-uy, ux)).
    const span = (sAt: number, ends?: Record<number, number[]>): void => {
      let k = 1;
      while (k < pts.length - 1 && along[k] < sAt) k++;
      const q = Math.max(0, Math.min(1, (sAt - along[k - 1]) / (along[k] - along[k - 1] || 1)));
      const [ax, ay] = pts[k - 1], [bx, by] = pts[k], len = Math.hypot(bx - ax, by - ay) || 1;
      const ux = (bx - ax) / len, uy = (by - ay) / len, px = ax + (bx - ax) * q, py = ay + (by - ay) * q, reach = hw * 1.15 + cell;
      pave(px + uy * reach, py - ux * reach, px - uy * reach, py + ux * reach);
      if (ends) for (const sd of [1, -1]) {
        const ex = px - uy * reach * sd, ey = py + ux * reach * sd;
        const to = ends[sd].reduce((a, j) => (Math.hypot(mid(j)[0] - ex, mid(j)[1] - ey) < Math.hypot(mid(a)[0] - ex, mid(a)[1] - ey) ? j : a), ends[sd][0]);
        // Over the land only: where its bank curls, a stray piece of deck in the notch would read as a bridge of its own.
        pave(ex, ey, ...mid(to), true);
      }
    };
    // Every road it crosses keeps its way over: in each joined piece of road the water took, where a road comes to the
    // water on one bank and leaves it on the other, nearest first along the river, a bridge straight across between
    // them. A road that runs along in the water (a boulevard by the quay) bridges nothing; one that only comes down to
    // the water ends at its bank.
    // Each bridge it would have: where along the river, and the roads it carries over (two roads that come to the water
    // within a few metres of each other share one).
    const plans: { s: number; ends: Record<number, number[]>[] }[] = [];
    const plan = (sAt: number, ends: Record<number, number[]>): void => {
      const near = plans.find((p) => Math.abs(p.s - sAt) < 2 * cell);
      if (near) near.ends.push(ends); else plans.push({ s: sAt, ends: [ends] });
    };
    const seen = new Set<number>();
    // Each joined piece of road the water took: the cells of its roads where they come to the water, run by run.
    const pieces: number[][][] = [];
    for (const i0 of wet) {
      if (!road0[i0] || seen.has(i0)) continue;
      const ends: Record<number, Set<number>> = { 1: new Set(), [-1]: new Set() };
      const stack = [i0];
      seen.add(i0);
      while (stack.length) {
        const i = stack.pop()!;
        for (let dr = -1; dr <= 1; dr++) for (let dc = -1; dc <= 1; dc++) {
          const c = (i % cols) + dc, r = Math.floor(i / cols) + dr, j = r * cols + c;
          if ((!dc && !dr) || c < 0 || r < 0 || c >= cols || r >= rows) continue;
          if (wet.has(j)) { if (road0[j] && !seen.has(j)) { seen.add(j); stack.push(j); } }
          else if ((ground[j] === G.road || ground[j] === G.bridge) && bank.has(j)) ends[bank.get(j)!].add(j);
        }
      }
      // Where along the river each bank's road comes to the water: its cells there, in runs along the bank.
      const runs = (cs: Set<number>): { s: number; cells: number[] }[] => {
        const out: { s: number; cells: number[] }[] = [];
        for (const j of [...cs].sort((p, q) => sta.get(p)! - sta.get(q)! || p - q)) {
          const last = out[out.length - 1];
          if (last && sta.get(j)! - sta.get(last.cells[last.cells.length - 1])! <= 2 * cell) last.cells.push(j);
          else out.push({ s: 0, cells: [j] });
        }
        for (const o of out) o.s = o.cells.reduce((a, j) => a + sta.get(j)!, 0) / o.cells.length;
        return out;
      };
      const right = runs(ends[-1]), left = runs(ends[1]);
      pieces.push([...right, ...left].map((o) => o.cells));
      const pairs: [number, number, number][] = [];
      // A road over it slantwise comes to the far bank further along: one crossing down to about 20 degrees pairs too,
      // nearest first, so one that crosses square still pairs with the run straight over from it.
      right.forEach((a, ia) => left.forEach((b, ib) => { const d = Math.abs(a.s - b.s); if (d <= 6 * hw + 2 * cell) pairs.push([d, ia, ib]); }));
      pairs.sort((p, q) => p[0] - q[0] || p[1] - q[1] || p[2] - q[2]);
      const usedR = new Set<number>(), usedL = new Set<number>();
      for (const [, ia, ib] of pairs) {
        if (usedR.has(ia) || usedL.has(ib)) continue;
        usedR.add(ia); usedL.add(ib);
        plan((right[ia].s + left[ib].s) / 2, { [-1]: right[ia].cells, 1: left[ib].cells });
      }
      // A road left over on one bank where two came to the water together (two avenues meeting at the river): its own
      // bridge to the nearest road over the water, though that one has its bridge already.
      for (const [mine, other, used, sd] of [[right, left, usedR, -1], [left, right, usedL, 1]] as const) mine.forEach((o, io) => {
        if (used.has(io) || !other.length) return;
        const p = other.reduce((a, b) => (Math.abs(b.s - o.s) < Math.abs(a.s - o.s) ? b : a));
        if (Math.abs(p.s - o.s) > 6 * hw + 2 * cell) return;
        used.add(io);
        plan((o.s + p.s) / 2, { [sd]: o.cells, [-sd]: p.cells });
      });
      // A road over it at a shallow slant (a second river meeting the road across the field) comes out on the far bank
      // further along than a bridge pairs; where nothing above bridged this piece and its road reaches both banks, one
      // bridge between the nearest run on each side.
      if (!usedR.size && !usedL.size && right.length && left.length) {
        let ia = 0, ib = 0;
        right.forEach((a, i) => left.forEach((b, j) => { if (Math.abs(a.s - b.s) < Math.abs(right[ia].s - left[ib].s)) { ia = i; ib = j; } }));
        usedR.add(ia); usedL.add(ib);
        plan((right[ia].s + left[ib].s) / 2, { [-1]: right[ia].cells, 1: left[ib].cells });
      }
      // What the water cut along one bank is joined again over the land: each run's cells in turn along it (a road that
      // dipped into the water and came out on the same bank), and a run left over, with no bridge, to another run on its
      // bank close by. Never a bridge: those are the record's (b596); where the land between is water, the repair below.
      for (const [side, used] of [[right, usedR], [left, usedL]] as const) {
        for (const o of side) for (let k = 1; k < o.cells.length; k++) pave(...mid(o.cells[k - 1]), ...mid(o.cells[k]), true);
        side.forEach((o, io) => {
          if (used.has(io)) return;
          let a0 = -1, b0 = -1, bd = Infinity;
          side.forEach((p, ip) => {
            if (ip !== io) for (const a of o.cells) for (const b of p.cells) { const d = Math.hypot(mid(a)[0] - mid(b)[0], mid(a)[1] - mid(b)[1]); if (d < bd) { bd = d; a0 = a; b0 = b; } }
          });
          if (a0 >= 0 && bd <= 4 * cell) pave(...mid(a0), ...mid(b0), true);
        });
      }
    }
    // As many bridges as the city had (b596: its road bridges in the war years, by the historian's record), those nearest
    // the heart; every other avenue ends at its bank, a quay. Bridges blown in the fighting are the battle's, not these.
    const pointAt = (sv: number): [number, number] => {
      let k = 1;
      while (k < pts.length - 1 && along[k] < sv) k++;
      const q = Math.max(0, Math.min(1, (sv - along[k - 1]) / (along[k] - along[k - 1] || 1)));
      return [pts[k - 1][0] + (pts[k][0] - pts[k - 1][0]) * q, pts[k - 1][1] + (pts[k][1] - pts[k - 1][1]) * q];
    };
    const fromHeart = (sv: number): number => { const [x, y] = pointAt(sv); return Math.hypot(x - heart.x, y - heart.y); };
    for (const p of [...plans].sort((a, b) => fromHeart(a.s) - fromHeart(b.s) || a.s - b.s).slice(0, cap)) for (const e of p.ends) span(p.s, e);
    // Roads that met in the water, two avenues crossing or three roads meeting there, come to it in two or more places on
    // each bank and are bridged pair by pair: each bridge leads on, but what joined one to the other was in the water.
    // Where a piece's roads are no longer one road, the nearest of them are joined again over the land between them.
    const deck = (i: number): boolean => ground[i] === G.road || ground[i] === G.bridge;
    const label = (): Int32Array => {
      const lab = new Int32Array(cols * rows).fill(-1);
      let n = 0;
      for (let i0 = 0; i0 < cols * rows; i0++) {
        if (lab[i0] >= 0 || !deck(i0)) continue;
        lab[i0] = n;
        const todo = [i0];
        while (todo.length) {
          const i = todo.pop()!;
          for (let dr = -1; dr <= 1; dr++) for (let dc = -1; dc <= 1; dc++) {
            const c = (i % cols) + dc, r = Math.floor(i / cols) + dr, j = r * cols + c;
            if (c < 0 || r < 0 || c >= cols || r >= rows || lab[j] >= 0 || !deck(j)) continue;
            lab[j] = n; todo.push(j);
          }
        }
        n++;
      }
      return lab;
    };
    let lab: Int32Array | null = null;
    for (const piece of pieces) {
      if (piece.length < 2) continue;
      for (let tries = 0; tries < piece.length; tries++) {
        lab ??= label();
        const L = lab, own = new Set(piece.map((cs) => L[cs[0]]));
        if (own.size < 2) break;
        // The shortest way over land (any ground but water) from the first of them to another: no further than a bridge
        // would pair them.
        const from = L[piece[0][0]], goal = new Set([...own].filter((k) => k !== from));
        const prev = new Map<number, number>(), todo: number[] = [];
        for (let i = 0; i < cols * rows; i++) if (L[i] === from) { prev.set(i, -1); todo.push(i); }
        let hit = -1;
        for (let q = 0, depth = 0; q < todo.length && hit < 0; depth++) {
          const end = todo.length;
          if (depth > Math.ceil((6 * hw + 2 * cell) / cell)) break;
          for (; q < end && hit < 0; q++) {
            const i = todo[q];
            for (let dr = -1; dr <= 1 && hit < 0; dr++) for (let dc = -1; dc <= 1; dc++) {
              const c = (i % cols) + dc, r = Math.floor(i / cols) + dr, j = r * cols + c;
              if (c < 0 || r < 0 || c >= cols || r >= rows || prev.has(j) || ground[j] === G.water || ground[j] === G.blown) continue;
              prev.set(j, i);
              if (goal.has(L[j])) { hit = j; break; }
              todo.push(j);
            }
          }
        }
        if (hit < 0) break;
        for (let i = prev.get(hit)!; i >= 0 && L[i] !== from; i = prev.get(i)!) if (!deck(i)) ground[i] = G.road;
        lab = null;
      }
    }
    if (!plans.length && cap > 0 && pts.length > 1) {
      // No road over it: one bridge, nearest the heart, where it is its own water and both its ends on land (a second
      // river's nearest point is often where it meets the first, its bridge in the first one's water).
      let s1 = 0, d1 = Infinity;
      const dry = (x: number, y: number): boolean => { const j = cellOf(x, y); return j >= 0 && !wet.has(j) && ground[j] !== G.water; };
      for (let k = 0; k < pts.length; k++) {
        const d = Math.hypot(pts[k][0] - heart.x, pts[k][1] - heart.y), j = cellOf(pts[k][0], pts[k][1]);
        const [ax, ay] = pts[Math.max(0, k - 1)], [bx, by] = pts[Math.max(1, k)], len = Math.hypot(bx - ax, by - ay) || 1, reach = hw * 1.15 + cell;
        const nx = (by - ay) / len, ny = -(bx - ax) / len;
        if (d < d1 && j >= 0 && wet.has(j) && dry(pts[k][0] + nx * reach, pts[k][1] + ny * reach) && dry(pts[k][0] - nx * reach, pts[k][1] - ny * reach)) { d1 = d; s1 = along[k]; }
      }
      if (d1 < Infinity) span(s1);
    }
  };
  lay(course, half, s.bridges ?? Infinity);
  if (join) lay(join.course, jh, s.join?.bridges ?? Infinity);
  return { name: s.name, lie, great: s.great, half, course, ...(join ? { join } : {}), ...(lie === 'across' ? { front } : {}) };
}

/** The share of a field that is built: its houses, blocks, suburbs and industry. */
export function builtShare(f: LandField): number {
  let n = 0;
  for (const k of f.ground) { const g = GROUNDS[k]; if (g === 'buildings' || g === 'suburb' || g === 'industry') n++; }
  return n / f.ground.length;
}

/**
 * Each real city's kept street plan by its key (planKey: its town's name; data/battle/city-plans.json, b625): the first of its own
 * sequence that keeps it within 7 points of its kind and no more than one fight in ten to the clock. Not listed: plan 1.
 */
export const CITY_PLANS: Record<string, number> = { ...(planData as { plans: Record<string, number> }).plans };
/** A real city's own key: its town's name, or, where two share a name, the key its province gives it (city-plans.json). */
export function planKey(place: LandPlace): string {
  return (planData as { shared: Record<string, Record<string, string>> }).shared[place.town ?? '']?.[place.name] ?? place.town ?? '';
}

/**
 * The seed a capital's or a city's street plan is laid from (b618): a real town's own, from its name and its kept plan
 * number (b625), so it is the same every time it is fought and no two cities share one; the battle's seed for a practice
 * ground (no place on the map).
 */
export function planSeed(place: LandPlace, seed: number): number {
  if (!(place.region >= 0 && place.town)) return seed;
  const key = planKey(place), n = CITY_PLANS[key] ?? 1;
  return hashString(n > 1 ? `streets:${key}#${n}` : `streets:${key}`);
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
 * A European core grew; it was not ruled (the art director, 18:53Z): its avenues run out from the landmark like spokes
 * and bend as they go, an old quarter tighter and taller than the rest stands round it with a boulevard where its walls
 * were (and in a capital a second, further out), and no edge of it is a ruler's line.
 * Roads, the river and fortress works are laid over it after, as on every field. Its own stream of draws, so the rest
 * of the field (its roads, its river) is as it was. Returns the storeys of what it built and its landmark.
 */
function layCity(size: 'capital' | 'city', tx: number, ty: number, cols: number, rows: number, ground: number[], height: number[], seed: number, strip = 0): Pick<LandField, 'storeys' | 'landmark' | 'streetEdge'> {
  const { w, h, cell } = FIELD, n = cols * rows;
  const cr = new Rng(seed ^ 0xc17e);
  const edge = fractal(cr, cols, rows, 6, 2), fill = fractal(cr, cols, rows, 2, 2);
  const storeys = new Array<number>(n).fill(0);
  const core = new Uint8Array(n);
  const capital = size === 'capital', front = h * 0.6;
  // Long slow waves along an edge: a district bulges out here and is bitten back there, so no edge is a ruler's line.
  const wave = (a: number, l: number): ((s: number) => number) => {
    const p1 = cr.range(0, Math.PI * 2), p2 = cr.range(0, Math.PI * 2), l2 = l * cr.range(0.3, 0.45);
    return (s) => a * (0.7 * Math.sin((s / l) * Math.PI * 2 + p1) + 0.3 * Math.sin((s / l2) * Math.PI * 2 + p2));
  };
  const coreFront = wave(170, 1300), termFront = wave(130, 1000), westSide = wave(110, 700), eastSide = wave(110, 700);
  const ringAt = new Uint8Array(n);
  for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
    const i = r * cols + c, x = c * cell + cell / 2, y = r * cell + cell / 2, e = (edge[i] - 0.5) * 120;
    // Which ring the cell is in: 3 core, 2 terraces, 1 suburbs, 0 outside the city.
    let ring = 0;
    if (capital) {
      const west = w * 0.05 + westSide(y) + e * 0.6, east = w - w * 0.05 - eastSide(y) - e * 0.6;
      ring = y >= front + coreFront(x) + e && x > west && x < east ? 3 : y >= front - 350 + termFront(x) + e ? 2 : y >= 90 ? 1 : 0;
    } else if (strip) {
      // Along its shore, 'strip' the line where its water begins (laid later, over the back of it): the whole width,
      // shallow, its edges ragged.
      const cf = strip - STRIP.core + coreFront(x) * 0.4 + e;
      ring = y >= cf ? 3 : y >= cf - STRIP.terraces + termFront(x) * 0.4 ? 2 : y >= cf - STRIP.terraces - STRIP.suburbs + termFront(x) * 0.6 + e ? 1 : 0;
    } else {
      // Rounded boxes about the centre (|dx|^4 + |dy|^4 < 1); the suburbs reach back to the middle of the field.
      const box = (a: number, b: number): number => ((x - tx) / a) ** 4 + ((y - ty) / b) ** 4;
      const back = Math.max(300, ty - h * 0.5 + e);
      ring = box(400 + e, 300 + e) < 1 ? 3 : box(580 + e, 480 + e) < 1 ? 2 : (((x - tx) / (1150 + e)) ** 2 + ((y - ty) / back) ** 2 < 1 || (y > ty && Math.abs(x - tx) < 1150 + e)) ? 1 : 0;
    }
    ringAt[i] = ring;
    if (ring === 3) {
      ground[i] = G.buildings; core[i] = 1;
      storeys[i] = 3 + (fill[i] > 0.55 ? 1 : 0);
    } else if (ring === 2 && fill[i] > 0.28) { ground[i] = G.buildings; storeys[i] = 2; }
    else if (ring === 1 && fill[i] > (capital ? 0.4 : 0.3)) { ground[i] = G.suburb; storeys[i] = 1 + (fill[i] > 0.7 ? 1 : 0); }
    // The city stands on level ground: its core and terraces flatter still than its suburbs.
    if (ring) height[i] *= ring === 1 ? 0.7 : 0.4;
  }
  const inCore = (c: number, r: number): boolean => c >= 0 && r >= 0 && c < cols && r < rows && core[r * cols + c] === 1;
  const c0 = Math.round(tx / cell);
  // The heart: the core's cell nearest the town's centre, where its landmark stands and its avenues meet.
  let hub = -1, hd = Infinity;
  for (let i = 0; i < n; i++) {
    if (!core[i]) continue;
    const d = Math.hypot((i % cols) * cell + cell / 2 - tx, Math.floor(i / cols) * cell + cell / 2 - ty);
    if (d < hd) { hd = d; hub = i; }
  }
  if (hub < 0) return { storeys };
  const hc = hub % cols, hr = Math.floor(hub / cols), hx = hc * cell + cell / 2, hy = hr * cell + cell / 2;
  const lay = (i: number): void => { if (i !== hub) { ground[i] = G.road; storeys[i] = 0; } };
  // A path of cells laid as a road, where it runs inside the city: a diagonal step stays a diagonal (the corner cell
  // between two on the slant is left out), so a street on the slant runs smooth, not as a stair.
  const diag = (p: number, q: number): boolean => Math.abs((p % cols) - (q % cols)) === 1 && Math.abs(Math.floor(p / cols) - Math.floor(q / cols)) === 1;
  const layPath = (path: number[], where: (i: number) => boolean): void => {
    let last = path[0];
    for (let j = 1; j < path.length; j++) {
      const q = path[j + 1];
      if (q !== undefined && diag(last, q)) continue;
      if (where(path[j])) lay(path[j]);
      last = path[j];
    }
  };
  // An irregular round about the heart: its radius swells and shrinks as it goes round.
  const round = (radius: number): { at: (a: number) => number; inside: (i: number) => boolean } => {
    const p2 = cr.range(0, Math.PI * 2), p3 = cr.range(0, Math.PI * 2);
    const at = (a: number): number => radius * (1 + 0.16 * Math.sin(2 * a + p2) + 0.1 * Math.sin(3 * a + p3));
    return { at, inside: (i) => { const x = (i % cols) * cell + cell / 2 - hx, y = Math.floor(i / cols) * cell + cell / 2 - hy; return Math.hypot(x, y) < at(Math.atan2(y, x)); } };
  };
  // The old quarter, tighter and taller than the rest of the core: four and five storeys.
  const old = round(capital ? 300 : 210);
  for (let i = 0; i < n; i++) if (core[i] && old.inside(i)) storeys[i] = 4 + (fill[i] > 0.5 ? 1 : 0);
  storeys[hub] = 5;
  // A boulevard where the old quarter's walls stood, and in a capital two more further out, where the city's later walls
  // and its railways ran: round each, inside the core.
  for (const { at } of capital ? [old, round(720), round(1150)] : [old]) {
    const path: number[] = [];
    const steps = Math.ceil((at(0) * Math.PI * 2) / (cell / 2));
    for (let t = 0; t <= steps; t++) {
      const a = (t / steps) * Math.PI * 2, c = Math.floor((hx + Math.cos(a) * at(a)) / cell), r = Math.floor((hy + Math.sin(a) * at(a)) / cell);
      const i = c >= 0 && r >= 0 && c < cols && r < rows ? r * cols + c : -1;
      if (i >= 0 && i !== path[path.length - 1]) path.push(i);
    }
    layPath(path, (i) => core[i] === 1);
  }
  // The avenues: out from the heart like spokes, the first toward the attacker's side where the road comes up the
  // field, each bending as it goes, through the core and its terraces.
  const spokes = capital ? 8 : 5, a0 = -Math.PI / 2 + cr.range(-0.15, 0.15);
  for (let k = 0; k < spokes; k++) {
    const a = a0 + (k * Math.PI * 2) / spokes + (k ? cr.range(-0.22, 0.22) : 0);
    const bend = cr.range(-0.35, 0.35), bl = cr.range(500, 900), bp = cr.range(0, Math.PI * 2);
    const path: number[] = [hub];
    let x = hx, y = hy;
    for (let s = 0; s < 3000; s += cell / 2) {
      const dir = a + bend * (Math.sin(s / bl + bp) - Math.sin(bp));
      x += Math.cos(dir) * (cell / 2); y += Math.sin(dir) * (cell / 2);
      const c = Math.floor(x / cell), r = Math.floor(y / cell);
      if (c < 0 || r < 0 || c >= cols || r >= rows || ringAt[r * cols + c] < 2) break;
      const i = r * cols + c;
      if (i !== path[path.length - 1]) path.push(i);
    }
    layPath(path, () => true);
  }
  // A square before the landmark: the cells about the heart.
  for (let dr = -1; dr <= 1; dr++) for (let dc = -1; dc <= 1; dc++) if (inCore(hc + dc, hr + dr)) lay((hr + dr) * cols + hc + dc);
  // Squares and parks: open patches in the core, two in a city, three in a capital.
  for (let k = 0; k < (capital ? 3 : 2); k++) {
    for (let tries = 0; tries < 20; tries++) {
      const pc = Math.floor(cr.next() * cols), pr = Math.floor(cr.next() * rows), pw = 3 + Math.floor(cr.next() * 2), ph = 3 + Math.floor(cr.next() * 2);
      if (!inCore(pc, pr) || !inCore(pc + pw - 1, pr + ph - 1) || old.inside(pr * cols + pc) || old.inside((pr + ph - 1) * cols + pc + pw - 1)) continue;
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
  // Its landmark, at the heart.
  const kinds = ['spire', 'dome', 'station'] as const;
  const landmark = { x: hx, y: hy, kind: kinds[Math.floor(cr.next() * 3)] };
  // Where the core's blocks begin, column by column.
  const streetEdge = new Array<number>(cols).fill(-1);
  for (let c = 0; c < cols; c++) for (let r = 0; r < rows; r++) if (core[r * cols + c]) { streetEdge[c] = r * cell; break; }
  return { storeys, landmark, streetEdge };
}

/** Ground no fieldworks are dug in: the river, a bridge, houses, works already there. */
const UNDUG: readonly Ground[] = ['water', 'bridge', 'blown', 'buildings', 'industry', 'bunker', 'works'];
/** Whether men can dig fieldworks on this ground (digWorks; hold until relieved, release 2). */
export const diggable = (g: Ground): boolean => !UNDUG.includes(g);

/** Fieldworks dug where a defender stood dug in: the cells under its box become 'works'. */
export function digWorks(f: LandField, x: number, y: number, w: number, d: number): void {
  for (let yy = y - d / 2 - 15; yy <= y + d / 2 + 15; yy += f.cell / 2) for (let xx = x - w / 2; xx <= x + w / 2; xx += f.cell / 2) {
    const c = Math.floor(xx / f.cell), r = Math.floor(yy / f.cell);
    if (c < 0 || r < 0 || c >= f.cols || r >= f.rows) continue;
    const i = r * f.cols + c, k = GROUNDS[f.ground[i]];
    if (diggable(k)) {
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
