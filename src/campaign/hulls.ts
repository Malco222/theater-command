// The names of the battleships and carriers that go down, for the card that marks one lost (Brian's memorial card).
// A record only: nothing here changes a fight, a count or a die. The record of names and crews is
// data/campaign/hulls.json; what is drawn is drawn apart from the war's dice, the same for the same game and fight.

import data from '../../data/campaign/hulls.json';
import { Rng, hashString } from '../core/rng';
import type { Game, LostHull, SeaSide, Ships } from './types';
import { farOn } from './world';

type Big = 'bb' | 'cv';
/** A change to a ship from a month on (the historian's check): a new name ('name') or a move to another fleet ('fleet'). */
interface Later { from: number; name?: string; fleet?: string }
export interface HullRow { name: string; crew: number; from?: number; fleet?: string; pair?: boolean; type?: 'battlecruiser'; later?: Later[] }
const HULLS = data as unknown as { share: Record<Big, number[]>; crew: Record<Big, number>; navies: Record<string, Partial<Record<Big, HullRow[]>>>;
  /** The far chart's fleet for a class of a navy's ships, where it differs from the old chart's fleet the rows name: [nation][class][old fleet] = far fleet. */
  far?: Record<string, Partial<Record<Big, Record<string, string>>>> };

/** A ship's name and fleet in a month: her first ones, then each change of 'later' that has come by then, in order. */
export function hullAt(r: HullRow, turn: number): { name: string; fleet?: string } {
  let name = r.name, fleet = r.fleet;
  for (const x of r.later ?? []) if (x.from <= turn) { name = x.name ?? name; fleet = x.fleet ?? fleet; }
  return { name, fleet };
}

/** A number from 0 to 1 that the same game and key always give, drawn apart from the war's own dice. */
const draw = (g: Game, key: string): number => new Rng(hashString(`hull:${g.seed}:${key}`)).next();

/**
 * The battleships and carriers a fleet of a nation lost (`sunk`), each named and its men counted, and each one named
 * marked as gone so that no other loss takes her name. The ship named is one still afloat of the fleet that lost her
 * (her fleet in that month) if it has one, any afloat of her nation if not, and none once the record runs out; she
 * carries the name she bore that month. Of her crew one of the real shares of her class is lost. `key` is the fight's own and keeps the draws apart from the war's dice.
 */
export function nameLosses(g: Game, nat: string, fleet: string, sunk: Ships, key: string): LostHull[] {
  const out: LostHull[] = [];
  for (const cls of ['bb', 'cv'] as const) {
    const rows = HULLS.navies[nat]?.[cls] ?? [];
    const shares = HULLS.share[cls];
    for (let k = 0; k < (sunk[cls] ?? 0); k++) {
      const share = shares[Math.floor(draw(g, `${key}:${cls}:${k}:men`) * shares.length)];
      const afloat = rows.map((r, i) => ({ r, i })).filter(({ r, i }) => (r.from ?? 1) <= g.turn && g.flags[`hull:${nat}:${cls}:${i}`] === undefined);
      // (On the far chart a navy's ships may sail in a fleet the old chart does not have: America's carriers are a fleet of
      // their own there. The record names the old chart's fleet, and 'far' gives the far chart's for that class.)
      const far = farOn(g) ? HULLS.far?.[nat]?.[cls] : undefined;
      const near = afloat.filter(({ r }) => { const at = hullAt(r, g.turn).fleet; return (at !== undefined && far?.[at] !== undefined ? far[at] : at) === fleet; });
      const pool = near.length ? near : afloat;
      if (!pool.length) {
        out.push({ name: null, cls, crew: HULLS.crew[cls], lost: Math.round(HULLS.crew[cls] * share) });
        continue;
      }
      const { r, i } = pool[Math.floor(draw(g, `${key}:${cls}:${k}`) * pool.length)];
      g.flags[`hull:${nat}:${cls}:${i}`] = g.turn;
      out.push({ name: hullAt(r, g.turn).name, cls, crew: r.crew, lost: Math.round(r.crew * share), ...(r.pair ? { pair: true } : {}), ...(r.type ? { type: r.type } : {}) });
    }
  }
  return out;
}

/** Names a fight's losses on its side of the report ('hulls'), where it lost a battleship or a carrier. */
export function nameSide(g: Game, side: SeaSide, key: string): void {
  const hulls = nameLosses(g, side.nat, side.fleet, side.sunk, key);
  if (hulls.length) side.hulls = hulls;
}
