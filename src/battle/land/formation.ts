// Drawing up before the battle (release 2; Brian, 26 November 2026: "i should be able to move my units in a formation
// before the battle begins during the deployed phase"). A control of the setup step and no battle rule (the design
// director, 03:50Z): where the blocks in hand would stand in a line, two lines, a wedge or a column, about the middle of
// where they stand now, facing the enemy, the guns and support teams behind. Nothing here places a block: the rules'
// own land:place has the last word on every place (sim.placeManyWhy), and the computer's setup never reads this.
//
// The spacing is the draw-up's own (units.ts): GAP metres between blocks side by side, ROW metres from one row to the
// next (the front line to the tanks behind it). Where the player's ground is too shallow for that, the rows close up, to
// no less than a block's depth and the setup step's own clearance; where it is too narrow, a row runs on behind.

import { LAND } from './rules';
import { GAP } from './units';

export type Formation = 'line' | 'two' | 'wedge' | 'column';
export const FORMATIONS: readonly Formation[] = ['line', 'two', 'wedge', 'column'];

/** A block to draw up: where it stands, its frontage and depth, and whether it is a gun or a support team (they go behind). */
export interface FormBlock { id: number; x: number; y: number; w: number; d: number; rear: boolean }
/** Where a block would stand. */
export interface FormSlot { id: number; x: number; y: number }

/** Metres from one row to the next, as the draw-up stands the tanks behind the front line. */
export const ROW = 110;
/** Metres each block of a wedge stands behind the one inside it. */
export const STEP = 60;

interface Row { us: FormBlock[]; back: number; steps?: number[] }

const widthOf = (us: FormBlock[]): number => us.reduce((a, u) => a + u.w, 0) + GAP * Math.max(0, us.length - 1);
const byX = (a: FormBlock, b: FormBlock): number => a.x - b.x || a.id - b.id;

/** A line cut into rows no wider than the room, the front ones full. */
function wrap(us: FormBlock[], wide: number): FormBlock[][] {
  const rows: FormBlock[][] = [];
  for (const u of us) {
    const r = rows[rows.length - 1];
    if (r && widthOf([...r, u]) <= wide) r.push(u);
    else rows.push([u]);
  }
  return rows;
}

/**
 * Where the blocks would stand drawn up so, inside the room [x0, y0, x1, y1]: `dir` 1 for a side that faces up the
 * field (the attack), -1 for one that faces down it. Null where the room cannot hold them even closed up. The order
 * along a row is the order they stand in now, west to east; the rows nearest the enemy are those that stand nearest
 * it now.
 */
export function drawnUp(kind: Formation, blocks: readonly FormBlock[], dir: 1 | -1, room: readonly [number, number, number, number]): FormSlot[] | null {
  if (!blocks.length) return [];
  const wide = room[2] - room[0] - 0.1, deep = room[3] - room[1] - 0.1;
  const ahead = (a: FormBlock, b: FormBlock): number => dir * (b.y - a.y) || byX(a, b);
  let front = blocks.filter((u) => !u.rear).sort(byX), rear = blocks.filter((u) => u.rear).sort(byX);
  // Only guns and teams in hand: they are the line.
  if (!front.length) { front = rear; rear = []; }
  const maxD = Math.max(...blocks.map((u) => u.d)), close = maxD + LAND.placeClear + 1;
  const rows: Row[] = [];
  let pitch = ROW;
  if (kind === 'column') {
    // One behind another, the foremost first, the guns and teams at the rear; where the ground is too shallow for one
    // file, as many files side by side as it takes.
    const file = [...[...front].sort(ahead), ...[...rear].sort(ahead)];
    pitch = maxD + GAP;
    const perFile = Math.max(1, Math.floor((deep - maxD) / pitch) + 1), files = Math.ceil(file.length / perFile);
    for (let k = 0; k < file.length; k += files) rows.push({ us: file.slice(k, k + files).sort(byX), back: 0 });
  } else {
    if (kind === 'wedge' && widthOf(front) <= wide && front.length >= 3) {
      // Its point toward the enemy: the middle block foremost, each one out from it a step further back.
      const mid = Math.floor((front.length - 1) / 2);
      rows.push({ us: front, back: 0, steps: front.map((_, i) => Math.abs(i - mid)) });
    } else if (kind === 'two' && front.length >= 2) {
      // Two lines: the half that stands nearest the enemy now in front.
      const order = [...front].sort(ahead), n = Math.ceil(front.length / 2);
      for (const r of wrap(order.slice(0, n).sort(byX), wide)) rows.push({ us: r, back: 0 });
      for (const r of wrap(order.slice(n).sort(byX), wide)) rows.push({ us: r, back: 0 });
    } else {
      for (const r of wrap(front, wide)) rows.push({ us: r, back: 0 });
    }
    for (const r of wrap(rear, wide)) rows.push({ us: r, back: 0 });
  }
  // The rows' depths: each a pitch behind the one before, a wedge's own depth counted; closed up where the room is
  // shallow, a wedge's steps first and then the rows.
  const wedge = rows.find((r) => r.steps), most = wedge ? Math.max(...wedge.steps!) : 0;
  let step = most > 0 ? STEP : 0;
  const need = (): number => maxD + most * step + pitch * (rows.length - 1);
  if (need() > deep && most > 0) step = Math.max(0, (deep - maxD - pitch * (rows.length - 1)) / most);
  if (need() > deep && rows.length > 1) pitch = (deep - maxD - most * step) / (rows.length - 1);
  if (need() > deep + 1e-6 || (rows.length > 1 && pitch < close)) return null;
  const slots: FormSlot[] = [];
  let back = 0;
  for (const r of rows) {
    let x = -widthOf(r.us) / 2;
    r.us.forEach((u, i) => {
      slots.push({ id: u.id, x: x + u.w / 2, y: -dir * (back + (r.steps ? r.steps[i] * step : 0)) });
      x += u.w + GAP;
    });
    back += pitch + (r.steps ? most * step : 0);
  }
  // About the middle of where they stand now, and moved in off the edges of the room.
  const size = new Map(blocks.map((u) => [u.id, u]));
  const box = (): [number, number, number, number] => [
    Math.min(...slots.map((s) => s.x - size.get(s.id)!.w / 2)), Math.min(...slots.map((s) => s.y - size.get(s.id)!.d / 2)),
    Math.max(...slots.map((s) => s.x + size.get(s.id)!.w / 2)), Math.max(...slots.map((s) => s.y + size.get(s.id)!.d / 2)),
  ];
  const cx = blocks.reduce((a, u) => a + u.x, 0) / blocks.length, cy = blocks.reduce((a, u) => a + u.y, 0) / blocks.length;
  const b0 = box(), mx = (b0[0] + b0[2]) / 2, my = (b0[1] + b0[3]) / 2;
  for (const s of slots) { s.x += cx - mx; s.y += cy - my; }
  // (a hair inside the line, so that a place worked out to the centimetre is never a hair outside it)
  const b1 = box(), E = 0.02;
  const sx = b1[0] < room[0] + E ? room[0] + E - b1[0] : b1[2] > room[2] - E ? room[2] - E - b1[2] : 0;
  const sy = b1[1] < room[1] + E ? room[1] + E - b1[1] : b1[3] > room[3] - E ? room[3] - E - b1[3] : 0;
  for (const s of slots) { s.x = Math.round((s.x + sx) * 100) / 100; s.y = Math.round((s.y + sy) * 100) / 100; }
  return slots;
}
