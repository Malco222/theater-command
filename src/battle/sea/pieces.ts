// The campaign's fleets turned into pieces on the field, and drawn up at the start.
//
// Each battleship, carrier and cruiser is a piece of its own and destroyers sail in fours (data/battle/sea.json
// classes.per). A side with more than pieces.most pieces pairs its cruisers, then puts more destroyers in each piece,
// until it fits. A piece's weight is its ships' weight as the rules count it (navy.ts fleetPower: the class's power,
// the carrier's deck, and the fleet's own multiplier); its hull is that weight over the class's toughness, so a
// battleship takes about twice the wear of its weight that a destroyer does, as the rules sink them. Each side's
// hulls together come to the same multiple of its weight (fire.hull), so toughness picks who goes down, not who wins.

import type { Side } from '../core/types';
import { RULES_CLASSES, SEA } from './numbers';
import { SEA_CLASSES, type FleetTicket, type SeaClass, type SeaUnit } from './types';

/** How many ships of each class sail as one piece on this side, so that it has no more than the most pieces. */
export function groupSizes(fleets: FleetTicket[]): Record<SeaClass, number> {
  const per: Record<SeaClass, number> = { bb: SEA.classes.bb.per, cv: SEA.classes.cv.per, ca: SEA.classes.ca.per, dd: SEA.classes.dd.per };
  const count = (): number => fleets.reduce((n, f) => n + SEA_CLASSES.reduce((m, c) => m + Math.ceil(f.ships[c] / per[c]), 0), 0);
  if (count() > SEA.pieces.most) per.ca = 2;
  while (count() > SEA.pieces.most && per.dd < 64) per.dd += 2;
  while (count() > SEA.pieces.most && per.ca < 64) per.ca += 1;
  return per;
}

/** One ship's weight in this fleet, as the rules count it. */
export const shipWeight = (f: FleetTicket, c: SeaClass): number => RULES_CLASSES[c].power * (c === 'cv' ? f.deck : 1) * f.mul;

export interface PieceInfo {
  /** The weight of one ship of the piece, and the hull of one ship. */
  weight: number;
  hull: number;
  fleet: FleetTicket;
}

/**
 * The pieces of both sides, drawn up at the start: the attacker's toward the south edge facing north, the defender's
 * toward the north edge facing south, each fleet in its own column, capital ships in the centre and the destroyers
 * on the wings. Returns the units and, by unit id, what the sim needs to know of each.
 */
export function drawUp(fleets: FleetTicket[]): { units: SeaUnit[]; info: Map<number, PieceInfo> } {
  const units: SeaUnit[] = [], info = new Map<number, PieceInfo>();
  const F = SEA.field;
  let id = 1;
  for (const side of ['att', 'def'] as Side[]) {
    const mine = fleets.filter((f) => f.side === side);
    const per = groupSizes(mine);
    // Lines across the field, one row per class, heaviest at the back: destroyers ahead, then cruisers, then battleships, then carriers.
    const rows: Record<SeaClass, { f: FleetTicket; n: number }[]> = { dd: [], ca: [], bb: [], cv: [] };
    for (const f of mine) for (const c of SEA_CLASSES) {
      let left = f.ships[c];
      while (left > 0) { const n = Math.min(per[c], left); rows[c].push({ f, n }); left -= n; }
    }
    const order: SeaClass[] = ['dd', 'ca', 'bb', 'cv'];
    const toward = side === 'att' ? 1 : -1;
    const baseY = side === 'att' ? F.edge : F.size - F.edge;
    order.forEach((c, row) => {
      const list = rows[c];
      const gap = c === 'dd' ? 500 : 650;
      const y = baseY - toward * row * 700;
      list.forEach((p, i) => {
        const x = F.size / 2 + (i - (list.length - 1) / 2) * gap;
        const w = shipWeight(p.f, c);
        const u: SeaUnit = {
          id, side, corps: p.f.id, nat: p.f.nat, kind: c,
          name: p.n === 1 ? `${cap(RULES_CLASSES[c].one)}, ${p.f.name}` : `${p.n} ${RULES_CLASSES[c].many}, ${p.f.name}`,
          x, y, face: side === 'att' ? 0 : Math.PI, str: 1, mor: 1, state: 'steady',
          men0: p.n * SEA.classes[c].crew,
          // The core's fields for the models (from the land thread's merge; the sea thread keeps them up to date as the sim runs).
          figures0: p.n, figures: p.n, w: 120, d: 220 * p.n, act: 'idle', speed: 0,
          extra: { cls: c, ships0: p.n, ships: p.n, sunk: 0, way: SEA.classes[c].speed * SEA.speeds.cruise, speed: 'cruise', target: -1, fleet: p.f.name },
        };
        if (c === 'cv') u.abilities = [{ key: 'strike', label: 'Carrier strike', ready: 0, windup: SEA.strike.windup, winding: false }];
        units.push(u);
        info.set(id, { weight: w, hull: w / RULES_CLASSES[c].tough, fleet: p.f });
        id++;
      });
    });
    // Toughness decides which ships go down, as in the rules, and not who wins: the rules weigh a fleet by its power
    // alone. So each side's hulls add up to the same 'hull' per unit of weight, shared out over its ships by toughness.
    let w = 0, h = 0;
    for (const u of units) if (u.side === side) { const p = info.get(u.id)!; w += p.weight * u.extra.ships0; h += p.hull * u.extra.ships0; }
    for (const u of units) if (u.side === side && h > 0) info.get(u.id)!.hull *= (SEA.fire.hull * w) / h;
  }
  return { units, info };
}

const cap = (s: string): string => s.charAt(0).toUpperCase() + s.slice(1);
