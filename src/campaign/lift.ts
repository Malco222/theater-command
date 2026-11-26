// Sea lift by the record (ruling 234, rider b630; data/campaign/lift.json). A nation's shipping changes on the
// record's dates, and a technology's shipping can wait for the month the record had it. Both are kept in g.flags,
// so they are saved with the game:
//   lfd:<row>        the dated row has been read
//   lft:<nat>:<tech> the month a technology's held-back shipping comes
import data from '../../data/campaign/lift.json';
import type { Game } from './types';

export const LIFT = data as unknown as {
  dated: [number, string, number, string][];
  techFrom: Record<string, Record<string, number>>;
};

/** Reads the dated rows and the held-back shipping that are due for this nation (the start of its phase). */
export function liftDates(g: Game, nat: string): void {
  const n = g.nations[nat];
  if (!n) return;
  LIFT.dated.forEach(([turn, who, add], i) => {
    const key = `lfd:${i}`;
    if (who !== nat || g.turn < turn || g.flags[key]) return;
    g.flags[key] = 1;
    n.sealift = Math.max(0, (n.sealift ?? 0) + add);
  });
  for (const id of Object.keys(LIFT.techFrom)) {
    const key = `lft:${nat}:${id}`, due = g.flags[key];
    if (due === undefined || g.turn < due) continue;
    n.sealift += g.flags[`${key}:n`] ?? 1;
    delete g.flags[key];
    delete g.flags[`${key}:n`];
  }
}

/** The shipping a technology just taken gives: now, or held back to the record's month for this nation. */
export function techLift(g: Game, nat: string, id: string, lift: number): void {
  const from = LIFT.techFrom[id]?.[nat];
  if (from !== undefined && g.turn < from) {
    g.flags[`lft:${nat}:${id}`] = from;
    g.flags[`lft:${nat}:${id}:n`] = lift;
    return;
  }
  g.nations[nat].sealift += lift;
}
