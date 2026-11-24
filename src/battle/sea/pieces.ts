// The campaign's fleets turned into pieces on the field, and drawn up at the start.
//
// Each battleship, carrier and cruiser is a piece of its own and destroyers sail in fours (data/battle/sea.json
// classes.per). A side with more than pieces.most pieces pairs its cruisers, then puts more destroyers in each piece,
// until it fits. A piece's weight is its ships' weight as the rules count it (navy.ts fleetPower: the class's power,
// the carrier's deck, and the fleet's own multiplier); its hull is that weight over the class's toughness, so a
// battleship takes about twice the wear of its weight that a destroyer does, as the rules sink them. Each side's
// hulls together come to the same multiple of its weight (fire.hull), so toughness picks who goes down, not who wins.
//
// Ships by class (naval-battles/ships-by-class-plan.md 4.7 and 5.1, c1): where the ticket gives a fleet's ships of a kind
// by row, a ship weighs her row's worth times her kind's weight, and a piece holds hulls of one row. Rows cut more
// pieces than the kind's count where several sail to a piece; past the side's limit, rows of one kind within one fleet
// merge, the closest worths first, and a merged piece weighs the mean of its hulls' worths. A ticket with no rows draws
// up as ever.

import type { Side } from '../core/types';
import { RULES_CLASSES, SEA } from './numbers';
import { SEA_CLASSES, type FleetTicket, type SeaClass, type SeaShips, type SeaUnit, type ShipRow } from './types';

/** A fleet's ships of a kind as the rules weigh them, in ships of the kind's weight: where it has rows, each hull at her
 *  row's worth and those past the rows at 1 (shiprows.ts hullSum); else its count. */
export function hullsOf(f: { ships: SeaShips; rows?: Partial<Record<SeaClass, ShipRow[]>> }, c: SeaClass): number {
  const rows = f.rows?.[c];
  if (!rows) return f.ships[c];
  let n = f.ships[c];
  for (const r of rows) n += r.n * (r.worth - 1);
  return n;
}

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
  /** Ships by class: the row of each hull in the piece, in its fleet's list order, where its fleet gives the kind by row. */
  rows?: (string | null)[];
}

/** A hull of a fleet that gives its kind by row: her row, its worth, and how much of a ship she is (1, or the part ship
 *  a count ends on, as the campaign's counts can). */
interface Hull { id: string | null; worth: number; part: number }

/** A fleet's hulls of a kind, row by row in its list order, the hulls past its rows last at worth 1 with no row (with
 *  hulls of a row of no row at worth 1, where it has one); null where it gives the kind by count. */
function rowHulls(f: FleetTicket, c: SeaClass): Hull[][] | null {
  const rows = f.rows?.[c];
  if (!rows) return null;
  const groups: Hull[][] = [];
  let left = f.ships[c];
  for (const r of rows) {
    const g: Hull[] = [];
    for (let i = 0; i < r.n && left > 1e-9; i++) { g.push({ id: r.id, worth: r.worth, part: 1 }); left -= 1; }
    if (g.length) groups.push(g);
  }
  if (left > 1e-9) {
    let rest = groups.find((g) => g[0].id === null && g[0].worth === 1);
    if (!rest) groups.push((rest = []));
    while (left > 1e-9) { const part = Math.min(1, left); rest.push({ id: null, worth: 1, part }); left -= part; }
  }
  return groups;
}

const meanWorth = (g: Hull[]): number => g.reduce((s, h) => s + h.worth * h.part, 0) / g.reduce((s, h) => s + h.part, 0);

/**
 * Past the side's limit (pieces.most), rows of one kind within one fleet merge, the closest worths first, until it fits
 * (plan 4.7): of the pairs whose merging saves a piece, the closest; where no single pair saves one, the closest pair of
 * all, so three rows that fit in two pieces only together still come together. Ties go to the first in the fleets' and
 * the kinds' order and the lists'. The merged row keeps the earlier row's place, its hulls after them.
 */
function mergeRows(cut: Map<FleetTicket, Partial<Record<SeaClass, Hull[][]>>>, count: () => number, per: Record<SeaClass, number>): void {
  while (count() > SEA.pieces.most) {
    let best: { gs: Hull[][]; i: number; j: number; d: number; saves: boolean } | null = null;
    for (const by of cut.values()) for (const c of SEA_CLASSES) {
      const gs = by[c];
      if (!gs) continue;
      for (let i = 0; i < gs.length; i++) for (let j = i + 1; j < gs.length; j++) {
        const saves = Math.ceil((gs[i].length + gs[j].length) / per[c]) < Math.ceil(gs[i].length / per[c]) + Math.ceil(gs[j].length / per[c]);
        const d = Math.abs(meanWorth(gs[i]) - meanWorth(gs[j]));
        if (!best || (saves && !best.saves) || (saves === best.saves && d < best.d - 1e-12)) best = { gs, i, j, d, saves };
      }
    }
    if (!best) return;
    best.gs[best.i] = [...best.gs[best.i], ...best.gs[best.j]];
    best.gs.splice(best.j, 1);
  }
}

/** Each class's row in the line, counted back from the front: destroyers ahead, then cruisers, battleships, carriers. */
const ROW: Record<SeaClass, number> = { dd: 0, ca: 1, bb: 2, cv: 3 };

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
    // Ships by class: the hulls of each kind a fleet gives by row, row by row, merged past the side's limit (mergeRows).
    const cut = new Map<FleetTicket, Partial<Record<SeaClass, Hull[][]>>>();
    for (const f of mine) for (const c of SEA_CLASSES) { const g = rowHulls(f, c); if (g) cut.set(f, { ...cut.get(f), [c]: g }); }
    if (cut.size) {
      const count = (): number => mine.reduce((n, f) => n + SEA_CLASSES.reduce((m, c) => {
        const gs = cut.get(f)?.[c];
        return m + (gs ? gs.reduce((k, g) => k + Math.ceil(g.length / per[c]), 0) : Math.ceil(f.ships[c] / per[c]));
      }, 0), 0);
      mergeRows(cut, count, per);
    }
    // Lines across the field, one row per class, heaviest at the back: destroyers ahead, then cruisers, then battleships, then carriers.
    // 'w': the weight of one of its ships; 'ids': its hulls' rows (ships by class).
    const rows: Record<SeaClass, { f: FleetTicket; n: number; w: number; ids?: (string | null)[] }[]> = { dd: [], ca: [], bb: [], cv: [] };
    for (const f of mine) for (const c of SEA_CLASSES) {
      const gs = cut.get(f)?.[c];
      if (gs) {
        // Pieces cut within a row: per[c] hulls a piece, a ship weighing the mean of its piece's hulls' worths.
        for (const g of gs) for (let k = 0; k < g.length; k += per[c]) {
          const hs = g.slice(k, k + per[c]), n = hs.reduce((s, h) => s + h.part, 0);
          rows[c].push({ f, n, w: shipWeight(f, c) * (hs.reduce((s, h) => s + h.worth * h.part, 0) / n), ids: hs.map((h) => h.id) });
        }
        continue;
      }
      let left = f.ships[c];
      while (left > 0) { const n = Math.min(per[c], left); rows[c].push({ f, n, w: shipWeight(f, c) }); left -= n; }
    }
    const order: SeaClass[] = ['dd', 'ca', 'bb', 'cv'];
    const toward = side === 'att' ? 1 : -1;
    const baseY = side === 'att' ? F.edge : F.size - F.edge;
    const sideW = mine.reduce((n, f) => n + SEA_CLASSES.reduce((m, c) => m + hullsOf(f, c) * shipWeight(f, c), 0), 0);
    order.forEach((c) => {
      const list = rows[c];
      const y = baseY - toward * ROW[c] * F.row;
      // One front for every class (b458): a row is pieces.front metres times its share of its side's weight, the same
      // metres for battleships, carriers, cruisers and destroyers, with its pieces spaced evenly along it. Rows that
      // carry the same share of their side's weight are as wide whatever ships they are, so fleets of the same weight
      // meet on the same front whatever their make and however many pieces they come in. Each class had its own spacing
      // before (650 m between big ships, 500 m between fours of destroyers): for the same weight a cruiser row stood on
      // three times the front of a battleship row and a destroyer row on twice, so a light line stood two or three times
      // as wide as a battle line of the same weight, its wings out of reach, and won 31 in 100 against a card of 50.
      const rowW = list.reduce((m, p) => m + p.n * p.w, 0);
      const slot = (SEA.pieces.front * rowW) / (Math.max(1e-6, sideW) * Math.max(1, list.length));
      list.forEach((p, i) => {
        // The defender's line is the attacker's turned half round, not its mirror: each side's first ship is on its
        // own left, so a ship turning to starboard does for one side what it does for the other (b402: drawn as a
        // mirror, the same helm put one side across the other's bows).
        const x = F.size / 2 + (side === 'def' ? -1 : 1) * (i - (list.length - 1) / 2) * slot;
        const w = p.w;
        const u: SeaUnit = {
          id, side, corps: p.f.id, nat: p.f.nat, kind: c,
          // Numbered in kind on its side, as the campaign counts ships by kind and the field never names what the
          // campaign does not know (the design director): "Cruiser 2", "Destroyers 4". Its fleet is extra.fleet.
          name: `${cap(p.n === 1 ? RULES_CLASSES[c].one : RULES_CLASSES[c].many)} ${i + 1}`,
          x, y, face: side === 'att' ? 0 : Math.PI, str: 1, mor: 1, state: 'steady',
          men0: p.n * SEA.classes[c].crew,
          // For the models: one figure a ship, the piece's ships in line ahead three lengths apart (the sim keeps these up to date).
          figures0: p.n, figures: p.n, w: SEA.classes[c].beam, d: SEA.classes[c].len * (1 + 3 * (p.n - 1)), act: 'march', speed: SEA.classes[c].speed * SEA.speeds.cruise,
          extra: { cls: c, ships0: p.n, ships: p.n, sunk: 0, way: SEA.classes[c].speed * SEA.speeds.cruise, speed: 'cruise', target: -1, fleet: p.f.name },
        };
        if (c === 'cv') u.abilities = [{ key: 'strike', label: 'Carrier strike', ready: 0, windup: SEA.strike.windup, winding: false }];
        units.push(u);
        info.set(id, { weight: w, hull: w / RULES_CLASSES[c].tough, fleet: p.f, ...(p.ids ? { rows: p.ids } : {}) });
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
