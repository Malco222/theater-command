// Field hospitals (release 2).
//
// Brian, 27 November 2026 17:10Z, yes to the idea: "the wounded go back down the line, and the ones who recover come
// back to their corps". Under this switch:
//
// 1. Of what a corps loses in a battle (the strength the fight itself took; never the men taken prisoner in the chase
//    after it), a share 'wounded' are wounded men who can come back. They are kept on the corps (c.wd, in divisions'
//    worth of men, so a corps that grows or shrinks keeps the same men).
// 2. Each month, before the drafts, a share 'back' of its wounded rejoins the corps, free: no men from the reserve and
//    nothing from the treasury. They come back only while the corps is in supply, and only into room it has (never past
//    its own full size): the supply's own reach (game.ts computeSupply, true reach where that row is on) stands for the
//    chain of field hospitals and the trains that carry the wounded (the DD, 17:15Z). A Medicine building in its
//    province or next door (buildings.ts hospitalNear) raises that share by 'raise' a level.
// 3. Cut off, its wounded wait, and 'lost' of them a month never come back. A corps destroyed loses its wounded.
// 4. The drafts then fill only what the returning wounded have left short.
// 5. Price once (the DD, 17:15Z): the Medicine building's quicker drafts for a resting corps ("gets more of its men
//    back") stood in for the wounded. Under this switch that stand-in goes (game.ts ward: drafts 1) and the building
//    raises the wounded's return instead. Its extra rest stays.
// 6. Every army's corps alike, the player's and the computers'.
//
// With the switch off nothing here is written, and the drafts read the Medicine building as they always did.

import data from '../../data/campaign/fieldhospitals.json';
import type { Corps, Game } from './types';
import { MEN_PER_DIV } from './types';

export const HOSP = data as unknown as { wounded: number; back: number; lost: number; raise: number };

/** The switch: a grand campaign's own, off unless the release 2 ship list or the player turns it on. */
export const hospOn = (g: Game): boolean => g.flags.fieldhosp === 1;

/** Turns the switch on or off at any time. Off sends every wounded man home: no corps keeps any. */
export function setHosp(g: Game, on: boolean): boolean {
  if (on) {
    if (g.flags.open !== 1) return false;
    g.flags.fieldhosp = 1;
    return true;
  }
  delete g.flags.fieldhosp;
  for (const c of g.corps) delete c.wd;
  for (const v of g.atSea ?? []) delete v.corps.wd;
  return true;
}

/** Books the wounded of a battle: `took` is the strength the fight took from the corps (before less after, a share of a division). */
export function woundedBook(g: Game, c: Corps, took: number): void {
  if (!hospOn(g) || c.kind === 'fort' || !(took > 0)) return;
  c.wd = Math.round(((c.wd ?? 0) + took * c.divs * HOSP.wounded) * 1e6) / 1e6;
}

/**
 * The month's wounded for a corps, before its drafts. `fed`: its drafts can reach it this month (in supply, not shut in
 * a town or a pocket, not living on a port's stores). `room`: the strength it lacks of its own full size (a share of a
 * division). `level`: the Medicine building in its province or next door (0 for none). Returns the strength given back.
 */
export function woundedMonth(g: Game, c: Corps, fed: boolean, room: number, level: number): number {
  if (!hospOn(g) || !c.wd) return 0;
  if (!fed) {
    c.wd = Math.round(c.wd * (1 - HOSP.lost) * 1e6) / 1e6;
    if (c.wd < 1e-4) delete c.wd;
    return 0;
  }
  const share = Math.min(1, HOSP.back * (1 + HOSP.raise * level));
  const give = Math.min(c.wd * share, Math.max(0, room) * c.divs);
  if (give > 0) c.str += give / c.divs;
  c.wd = Math.round((c.wd - give) * 1e6) / 1e6;
  if (c.wd < 1e-4) delete c.wd;
  return give;
}

/** The corps card's line for its own player: "Wounded: 3,400 men in the hospitals; about 850 rejoin next month." */
export function woundedLine(g: Game, c: Corps, fed: boolean, level: number): string {
  if (!hospOn(g) || !c.wd) return '';
  const menOf = (x: number): string => (Math.round((x * MEN_PER_DIV) / 100) * 100).toLocaleString('en-US');
  if (!fed) return `Wounded: ${menOf(c.wd)} men waiting. Cut off, some will not come back. They rejoin once supply reaches the corps.`;
  return `Wounded: ${menOf(c.wd)} men in the hospitals; about ${menOf(c.wd * Math.min(1, HOSP.back * (1 + HOSP.raise * level)))} rejoin next month.`;
}
