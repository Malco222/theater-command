// Selection helpers for any screen: what is under the pointer, what a dragged box takes in, and where a group goes
// when the player draws a line for it. The screens keep the selection themselves; the sims only see orders.

import { bearing, dist } from './geometry';
import type { Side, UnitBase } from './types';

const live = (u: UnitBase): boolean => u.state !== 'gone' && u.state !== 'withdrawn';

/** The unit nearest (x, y) within 'reach' metres, the given side's first if both are near. */
export function unitAt<U extends UnitBase>(units: U[], x: number, y: number, reach: number, prefer?: Side): U | null {
  let best: U | null = null, bd = Infinity;
  for (const u of units) {
    if (!live(u)) continue;
    const d = dist(u.x, u.y, x, y) - (prefer && u.side === prefer ? reach * 0.25 : 0);
    if (d < bd && dist(u.x, u.y, x, y) <= reach) { bd = d; best = u; }
  }
  return best;
}

/** The side's units inside a box. */
export function unitsIn<U extends UnitBase>(units: U[], side: Side, x0: number, y0: number, x1: number, y1: number): U[] {
  const [lx, hx] = x0 < x1 ? [x0, x1] : [x1, x0], [ly, hy] = y0 < y1 ? [y0, y1] : [y1, y0];
  return units.filter((u) => live(u) && u.side === side && u.x >= lx && u.x <= hx && u.y >= ly && u.y <= hy);
}

/**
 * Where each unit of a group stands along a line drawn from (x0, y0) to (x1, y1), and the way they face: across
 * the line, toward the enemy's edge (north for the attacker, south for the defender). The units keep their order
 * from left to right as they stand now, so nobody crosses through his neighbour. A very short line (a click, not
 * a drag) lays them out side by side centred on the point, facing the enemy's edge.
 */
export function lineUp(units: UnitBase[], side: Side, x0: number, y0: number, x1: number, y1: number, gap = 120): { id: number; x: number; y: number; face: number }[] {
  if (!units.length) return [];
  let len = dist(x0, y0, x1, y1);
  if (len < 30) {
    const w = gap * (units.length - 1);
    x0 -= w / 2; x1 = x0 + w; y1 = y0; len = w;
  }
  // Across the line, on the enemy's side of it.
  let face = bearing(x0, y0, x1, y1) - Math.PI / 2;
  const toward = side === 'att' ? 0 : Math.PI;
  if (Math.cos(face - toward) < 0) face += Math.PI;
  const ux = len > 0 ? (x1 - x0) / len : 1, uy = len > 0 ? (y1 - y0) / len : 0;
  // Left to right as they stand now, measured along the line.
  const order = [...units].sort((a, b) => (a.x * ux + a.y * uy) - (b.x * ux + b.y * uy));
  const n = order.length;
  return order.map((u, i) => {
    const k = n === 1 ? 0.5 : i / (n - 1);
    return { id: u.id, x: x0 + (x1 - x0) * k, y: y0 + (y1 - y0) * k, face };
  });
}
