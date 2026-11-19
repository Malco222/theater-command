// The computer general on the field: plain and predictable, so the fairness test measures the odds, not cleverness.
// TODO(polish): reserves, a real plan for the wings, falling back on a line, picking off guns.
//
// Attacking: every block takes an enemy block in its own lane and goes for it; tanks and lorries go for the ends of
// the enemy line; guns come up until the enemy is in reach and stand. Defending: hold the ground, help a neighbour
// who is in a fight, and go after a block that is wavering or running close by. A reserve held behind the line goes,
// once the line is struck, to where it is struck hardest; a block on a quiet stretch of the line, with no enemy facing
// it, closes on the fight (the design director, b407).

import { dist } from '../core/geometry';
import type { Side } from '../core/types';
import type { LandBattle } from './sim';
import { LAND } from './rules';
import type { LandUnit } from './units';

const ok = (u: LandUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn' && u.state !== 'routing';

export function thinkFor(b: LandBattle, side: Side): void {
  const mine = b.units.filter((u) => u.side === side && ok(u) && u.kind !== 'fort');
  const foes = b.units.filter((u) => u.side !== side && ok(u));
  if (!foes.length) return;
  const by = (u: LandUnit, t: LandUnit): void => { b.command({ do: 'attack', units: [u.id], target: t.id }, side); };
  const hasTarget = (u: LandUnit): boolean => u.order?.kind === 'attack' && foes.some((f) => f.id === u.order!.target);
  if (side === 'att') {
    const sorted = [...foes].sort((a, c) => a.x - c.x);
    for (const u of mine) {
      if (u.order?.kind === 'withdraw') continue;
      if (u.kind === 'art') {
        const near = foes.reduce((m, f) => Math.min(m, b.gap(u, f)), Infinity);
        if (near > LAND.reach.art * 0.9 && !u.order) {
          const cy = foes.reduce((s, f) => s + f.y, 0) / foes.length;
          b.command({ do: 'move', units: [u.id], x: u.x, y: Math.min(cy - LAND.reach.art * 0.75, u.y + 400) }, side);
        }
        continue;
      }
      if (hasTarget(u)) continue;
      let t: LandUnit;
      if (u.kind === 'arm' || u.kind === 'mot') {
        // The nearer end of their line.
        const left = sorted[0], right = sorted[sorted.length - 1];
        t = dist(u.x, u.y, left.x, left.y) < dist(u.x, u.y, right.x, right.y) ? left : right;
      } else {
        t = foes.reduce((best, f) => (Math.abs(f.x - u.x) + 0.3 * Math.abs(f.y - u.y) < Math.abs(best.x - u.x) + 0.3 * Math.abs(best.y - u.y) ? f : best), foes[0]);
      }
      by(u, t);
    }
    return;
  }
  // Defending: the line stays where it was drawn up. A block steps out only to finish one that is wavering right in
  // front of it, or to take one that has come round a neighbour's flank, and never far from its own ground. In a
  // capital's or a city's streets never out of them: an enemy south of the edge of its core is left to come on into the
  // houses (the design director, b529).
  const leash = 300;
  const edge = b.field.streetEdge, cell = b.field.cell;
  const outside = (f: LandUnit): boolean => {
    if (!edge) return false;
    const y = edge[Math.min(edge.length - 1, Math.max(0, Math.floor(f.x / cell)))];
    return y < 0 || f.y < y;
  };
  // Where the line is struck, hardest first: its blocks in a fight, the most worn first (b407). Its guns firing from
  // far back are not the line struck, as they are not contact (sim.ts contactAt).
  const struck = mine.filter((m) => m.kind !== 'art' && m.reserve !== 'held' && m.engaged !== undefined && foes.some((f) => f.id === m.engaged))
    .sort((a, c) => a.str / a.str0 - c.str / c.str0);
  let next = 0;
  const sendTo = (u: LandUnit): void => {
    const at = struck[next++ % struck.length];
    u.reserve = 'sent';
    by(u, foes.find((f) => f.id === at.engaged)!);
  };
  for (const u of mine) {
    if (u.kind === 'art' || u.order?.kind === 'withdraw') continue;
    if (u.reserve === 'held') { if (struck.length) sendTo(u); continue; }
    if (u.reserve === 'sent') { if (!hasTarget(u) && struck.length) sendTo(u); continue; }
    // A quiet stretch: no enemy in this block's lane anywhere on the field, while the line is struck elsewhere.
    if (struck.length && !hasTarget(u) && u.engaged === undefined && !foes.some((f) => Math.abs(f.x - u.home[0]) < LAND.quiet)) { sendTo(u); continue; }
    const away = dist(u.x, u.y, u.home[0], u.home[1]);
    if (hasTarget(u)) {
      const t = foes.find((f) => f.id === u.order!.target)!;
      if (away > leash || (u.engaged !== t.id && outside(t))) b.command({ do: 'move', units: [u.id], x: u.home[0], y: u.home[1], face: Math.PI }, side);
      continue;
    }
    if (u.engaged !== undefined) continue;
    if (away > 40 && !u.order) { b.command({ do: 'move', units: [u.id], x: u.home[0], y: u.home[1], face: Math.PI }, side); continue; }
    const shaky = foes.find((f) => f.state === 'wavering' && b.gap(u, f) < 150 && !outside(f));
    if (shaky) { by(u, shaky); continue; }
    const round = foes.find((f) => dist(f.x, f.y, u.home[0], u.home[1]) < leash && !outside(f) && mine.some((m) => m.engaged === f.id && m !== u));
    if (round) by(u, round);
  }
}
