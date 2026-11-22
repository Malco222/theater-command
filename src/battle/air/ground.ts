// The ground under an air battle: the country round the target as the campaign map draws it, turned as the board is
// turned (the raid comes on from the south edge), so every raid flies over its own ground (the design director,
// b384(j)): the coasts and the sea, the great rivers, the provinces' borders, and the target province's own country.
// Everything is in board metres. The browser page draws it under the formations; the 3D game can lay its terrain
// from the same lines.

import mapData from '../../generated/campaign-map.json';
import worldMap from '../../generated/world-map.json';
import { riversOf } from '../../campaign/rivers';
import type { Game } from '../../campaign/types';
import { SKY } from './book';

type Pt = [number, number];

export interface AirGround {
  /** The land, as filled outlines (rings): what is not land on the board is sea. Empty where the board is all land. */
  land: Pt[][];
  /** The great rivers, as lines. */
  rivers: Pt[][];
  /** The provinces' borders, as lines; the target province's own outline apart. */
  borders: Pt[][];
  target: Pt[][];
  /** The target province's country (plains, forest, hills, mountain, marsh, desert, urban): it decides the woods and fields. */
  terrain: string;
  /** Whether the board is all land (no coast within it). */
  inland: boolean;
  /**
   * True north on the turned board, as a unit vector: [0, 1] when the raid comes on from due south. For laying the
   * ground on its real heading (a river that runs north on the map runs along this on the board). Nothing in the
   * battle reads it.
   */
  north: Pt;
}

/** How far beyond the board the ground is kept, in metres, so the edges never show a cut. */
const MARGIN = 50_000;
/** The finest a line is kept, in metres: a point nearer than this to the last is dropped. */
const FINE = 400;

interface MapLike { land: Pt[][][] | Pt[][]; regions: { id: number; terrain: string; polys: Pt[][][] }[] }

/**
 * The ground round a target. 'at' turns a map point into board metres (the ticket's own turn of the board), and
 * 'centre' is the target province's map position.
 */
export function groundOf(g: Game, target: number, at: (x: number, y: number) => Pt): AirGround {
  const maps: MapLike[] = [mapData as unknown as MapLike, ...(g.flags.world === 1 ? [worldMap as unknown as MapLike] : [])];
  const lo = -MARGIN, hi = SKY.w + MARGIN, hiY = SKY.h + MARGIN;
  const inBox = ([x, y]: Pt): boolean => x >= lo && x <= hi && y >= lo && y <= hiY;
  const thin = (pts: Pt[]): Pt[] => {
    const out: Pt[] = [];
    for (const p of pts) { const l = out[out.length - 1]; if (!l || Math.hypot(p[0] - l[0], p[1] - l[1]) >= FINE) out.push(p); }
    return out;
  };
  // A line, cut to the runs that come within the box (one point either side kept, so it runs off the edge).
  const runs = (pts: Pt[]): Pt[][] => {
    const out: Pt[][] = [];
    let cur: Pt[] = [];
    pts.forEach((p, i) => {
      const near = inBox(p) || (i > 0 && inBox(pts[i - 1])) || (i < pts.length - 1 && inBox(pts[i + 1]));
      if (near) cur.push(p); else if (cur.length) { if (cur.length > 1) out.push(thin(cur)); cur = []; }
    });
    if (cur.length > 1) out.push(thin(cur));
    return out;
  };
  const bboxIn = (pts: Pt[]): boolean => {
    let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
    for (const [x, y] of pts) { x0 = Math.min(x0, x); x1 = Math.max(x1, x); y0 = Math.min(y0, y); y1 = Math.max(y1, y); }
    return x1 >= lo && x0 <= hi && y1 >= lo && y0 <= hiY;
  };
  const land: Pt[][] = [], borders: Pt[][] = [], mine: Pt[][] = [];
  let terrain = 'plains';
  let coast = false;
  for (const m of maps) {
    for (const poly of m.land as unknown as Pt[][][]) {
      for (const ring of poly) {
        const pts = ring.map(([x, y]) => at(x, y));
        if (!bboxIn(pts)) continue;
        land.push(thin(pts));
        // A coast within the board: some stretch of the outline runs over it.
        if (pts.some(([x, y]) => x >= 0 && x <= SKY.w && y >= 0 && y <= SKY.h)) coast = true;
      }
    }
    for (const r of m.regions) {
      for (const poly of r.polys) for (const ring of poly) {
        const pts = ring.map(([x, y]) => at(x, y));
        if (!bboxIn(pts)) continue;
        const closed = [...pts, pts[0]];
        if (r.id === target) { mine.push(thin(closed)); terrain = r.terrain; } else borders.push(...runs(closed));
      }
    }
  }
  const rivers: Pt[][] = [];
  for (const rv of riversOf(g)) rivers.push(...runs(rv.pts.map(([x, y]) => at(x, y))));
  // North on the map is up the page (y falls), so it is where a step up the map lands on the board.
  const o = at(0, 0), up = at(0, -1), far = Math.hypot(up[0] - o[0], up[1] - o[1]) || 1;
  const north: Pt = [Math.round(((up[0] - o[0]) / far) * 1e6) / 1e6 + 0, Math.round(((up[1] - o[1]) / far) * 1e6) / 1e6 + 0];
  return { land: coast ? land : [], rivers, borders, target: mine, terrain, inland: !coast, north };
}
