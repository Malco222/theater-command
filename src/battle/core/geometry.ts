// Angles and distances on the field (see the frame in types.ts: 0 = north, clockwise, metres).

export const TAU = Math.PI * 2;

/** The facing that looks from (x0, y0) toward (x1, y1). */
export const bearing = (x0: number, y0: number, x1: number, y1: number): number => Math.atan2(x1 - x0, y1 - y0);

/** The smallest turn from a to b, -PI..PI. */
export const turn = (a: number, b: number): number => { let d = (b - a) % TAU; if (d > Math.PI) d -= TAU; if (d < -Math.PI) d += TAU; return d; };

/** Metres between two points. (A plain square root: Math.hypot was the sims' single biggest cost, the sea thread found.) */
export const dist = (ax: number, ay: number, bx: number, by: number): number => { const dx = bx - ax, dy = by - ay; return Math.sqrt(dx * dx + dy * dy); };

/** Which side of a unit facing 'face' at (x, y) a blow from (fx, fy) lands on. */
export type Aspect = 'front' | 'flank' | 'rear';
export function aspectOf(face: number, x: number, y: number, fx: number, fy: number): Aspect {
  const off = Math.abs(turn(face, bearing(x, y, fx, fy)));
  return off <= Math.PI / 3 ? 'front' : off <= (2 * Math.PI) / 3 ? 'flank' : 'rear';
}

/** Unit vector for a facing. */
export const ahead = (face: number): [number, number] => [Math.sin(face), Math.cos(face)];

export const clamp = (v: number, lo: number, hi: number): number => (v < lo ? lo : v > hi ? hi : v);
