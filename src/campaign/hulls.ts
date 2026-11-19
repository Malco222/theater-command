// The names of the battleships and carriers that go down, for the card that marks one lost (Brian's memorial card).
// A record only: nothing here changes a fight, a count or a die. The record of names and crews is
// data/campaign/hulls.json; what is drawn is drawn apart from the war's dice, the same for the same game and fight.

import data from '../../data/campaign/hulls.json';
import { Rng, hashString } from '../core/rng';
import type { Game, LostHull, SeaSide, Ships } from './types';

type Big = 'bb' | 'cv';
interface Row { name: string; crew: number; from?: number; fleet?: string; pair?: boolean }
const HULLS = data as unknown as { share: Record<Big, number[]>; crew: Record<Big, number>; navies: Record<string, Partial<Record<Big, Row[]>>> };

/** A number from 0 to 1 that the same game and key always give, drawn apart from the war's own dice. */
const draw = (g: Game, key: string): number => new Rng(hashString(`hull:${g.seed}:${key}`)).next();

/**
 * The battleships and carriers a fleet of a nation lost (`sunk`), each named and its men counted, and each one named
 * marked as gone so that no other loss takes her name. The ship named is one still afloat of the fleet that lost her
 * if it has one, any afloat of her nation if not, and none once the record runs out; of her crew one of the real
 * shares of her class is lost. `key` is the fight's own and keeps the draws apart from the war's dice.
 */
export function nameLosses(g: Game, nat: string, fleet: string, sunk: Ships, key: string): LostHull[] {
  const out: LostHull[] = [];
  for (const cls of ['bb', 'cv'] as const) {
    const rows = HULLS.navies[nat]?.[cls] ?? [];
    const shares = HULLS.share[cls];
    for (let k = 0; k < (sunk[cls] ?? 0); k++) {
      const share = shares[Math.floor(draw(g, `${key}:${cls}:${k}:men`) * shares.length)];
      const afloat = rows.map((r, i) => ({ r, i })).filter(({ r, i }) => (r.from ?? 1) <= g.turn && g.flags[`hull:${nat}:${cls}:${i}`] === undefined);
      const near = afloat.filter(({ r }) => r.fleet === fleet);
      const pool = near.length ? near : afloat;
      if (!pool.length) {
        out.push({ name: null, cls, crew: HULLS.crew[cls], lost: Math.round(HULLS.crew[cls] * share) });
        continue;
      }
      const { r, i } = pool[Math.floor(draw(g, `${key}:${cls}:${k}`) * pool.length)];
      g.flags[`hull:${nat}:${cls}:${i}`] = g.turn;
      out.push({ name: r.name, cls, crew: r.crew, lost: Math.round(r.crew * share), ...(r.pair ? { pair: true } : {}) });
    }
  }
  return out;
}

/** Names a fight's losses on its side of the report ('hulls'), where it lost a battleship or a carrier. */
export function nameSide(g: Game, side: SeaSide, key: string): void {
  const hulls = nameLosses(g, side.nat, side.fleet, side.sunk, key);
  if (hulls.length) side.hulls = hulls;
}
