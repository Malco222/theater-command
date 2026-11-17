// A raid fought out as an air battle, handed back to the rules (src/battle/air).
//
// When the player fights a raid instead of letting the dice settle it, the battle reports what each air group lost,
// how many downed fighter pilots came back, and how many bombers dropped on the target. fly() then applies those
// numbers in place of its dice, and everything a raid does after that (output stopped, railways, factories, the sky
// won, the news) follows from them exactly as it does for a raid the dice settled.
//
// The rules stay the judge: what the battle reports is held inside the dice's own run round the expected figures
// (the design director's ruling of 2026-11-17 17:55Z): each side's losses within fly()'s luck of the expected, and
// the bombers that bomb within what that luck allows. Absent, fly() rolls its dice exactly as before.

import type { AirFleet } from './types';

export interface FoughtAir {
  /** By campaign air group id: fighters and bombers lost, and downed fighter pilots who fly again. */
  lost: Record<number, { ftr: number; bmr: number; back: number }>;
  /** Bombers that dropped on the target. */
  dropped: number;
}

/** The dice's run either side of the expected figure: fly()'s own luck (AIR.combat.luck). */
const RUN = { luck: 0.15 };

/**
 * What fly() applies for a fought raid: each group's losses, held inside the dice's run. `expected` are fly()'s own
 * figures at even luck (defending fighters, escorting fighters, bombers lost), `groups` the groups each applies to
 * with the share fly() would give each, and `B` the bombers that reached the target.
 */
export function settleFought(
  fought: FoughtAir,
  expected: { foe: number; ftr: number; bmr: number },
  groups: { def: { f: AirFleet; share: number }[]; esc: { f: AirFleet; share: number }[]; bmr: { f: AirFleet; share: number }[] },
  B: number,
  luck = RUN.luck,
): { ftr: (f: AirFleet) => number; bmr: (f: AirFleet) => number; back: (f: AirFleet, k: number) => number; through: (survivors: number) => number } {
  const held = (rows: { f: AirFleet; share: number }[], want: (f: AirFleet) => number, exp: number, cap: (f: AirFleet) => number): Map<number, number> => {
    const got = new Map(rows.map((r) => [r.f.id, Math.min(cap(r.f), Math.max(0, Math.round(want(r.f))))]));
    const total = [...got.values()].reduce((s, v) => s + v, 0);
    const lo = Math.floor(exp * (1 - luck)), hi = Math.ceil(exp * (1 + luck));
    const goal = total < lo ? lo : total > hi ? hi : total;
    if (goal === total || !rows.length) return got;
    // Spread the difference by the shares fly() itself would give, a whole aircraft at a time.
    let left = goal - total;
    const step = Math.sign(left);
    for (let guard = 0; left !== 0 && guard < 100_000; guard++) {
      const pick = [...rows].sort((a, b) => step * (b.share - a.share) || a.f.id - b.f.id)
        .find((r) => (step > 0 ? got.get(r.f.id)! < cap(r.f) : got.get(r.f.id)! > 0));
      if (!pick) break;
      got.set(pick.f.id, got.get(pick.f.id)! + step);
      left -= step;
    }
    return got;
  };
  const row = (f: AirFleet): FoughtAir['lost'][number] => fought.lost[f.id] ?? { ftr: 0, bmr: 0, back: 0 };
  const def = held(groups.def, (f) => row(f).ftr, expected.foe, (f) => f.ftr);
  const esc = held(groups.esc, (f) => row(f).ftr, expected.ftr, (f) => f.ftr);
  const bmr = held(groups.bmr, (f) => row(f).bmr, expected.bmr, (f) => f.bmr);
  const bomberRun = { lo: B - Math.ceil(expected.bmr * (1 + luck)), hi: B - Math.floor(expected.bmr * (1 - luck)) };
  return {
    ftr: (f) => def.get(f.id) ?? esc.get(f.id) ?? 0,
    bmr: (f) => bmr.get(f.id) ?? 0,
    back: (f, k) => Math.min(k, row(f).back),
    // The bombers that bomb: what dropped, but never outside the run the dice give for the bombers that survive.
    through: (survivors) => Math.max(0, Math.min(survivors, Math.max(bomberRun.lo, Math.min(bomberRun.hi, fought.dropped)))),
  };
}
