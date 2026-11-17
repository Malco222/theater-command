// The computer general on the field: plain and predictable, so the fairness test measures the odds, not cleverness.
// TODO(polish): reserves, a real plan for the wings, falling back on a line, picking off guns.
//
// Attacking: every block takes an enemy block in its own lane and goes for it; tanks and lorries go for the ends of
// the enemy line; guns come up until the enemy is in reach and stand. Defending: hold the ground, help a neighbour
// who is in a fight, and go after a block that is wavering or running close by.

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
  // front of it, or to take one that has come round a neighbour's flank, and never far from its own ground.
  const leash = 300;
  for (const u of mine) {
    if (u.kind === 'art' || u.order?.kind === 'withdraw') continue;
    const away = dist(u.x, u.y, u.home[0], u.home[1]);
    if (hasTarget(u)) {
      if (away > leash) b.command({ do: 'move', units: [u.id], x: u.home[0], y: u.home[1], face: Math.PI }, side);
      continue;
    }
    if (u.engaged !== undefined) continue;
    if (away > 40 && !u.order) { b.command({ do: 'move', units: [u.id], x: u.home[0], y: u.home[1], face: Math.PI }, side); continue; }
    const shaky = foes.find((f) => f.state === 'wavering' && b.gap(u, f) < 150);
    if (shaky) { by(u, shaky); continue; }
    const round = foes.find((f) => dist(f.x, f.y, u.home[0], u.home[1]) < leash && mine.some((m) => m.engaged === f.id && m !== u));
    if (round) by(u, round);
  }
}
