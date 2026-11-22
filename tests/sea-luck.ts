// Seeds for the fleet-action tests whose luck of the day is spread evenly over the dice's whole run.
//
// The luck of the day is the first roll of a battle's seed (sim.ts: the rules' own run of the dice, 0.8 to 1.2), and it
// decides more of a close action than anything else. Plain seeds (1 x 7919, 2 x 7919, ...) happened to roll lucky for
// the attacker: the first 40 averaged 1.023, worth about a tenth of a win to every attacker in every test, and the
// practice seeds 1000 + k x 7919 averaged 0.980, a tenth against. So each test takes one seed from each of n equal
// slices of the run, and measures the field, not the seeds' luck.
import { Rng } from '../src/core/rng';
import { DICE } from '../src/battle/sea/numbers';

/** n seeds, the k-th rolling a luck of the day in the k-th of n equal slices of the dice's run; found by stepping from 'base'. */
export function evenLuck(n: number, base = 7919): number[] {
  const out: number[] = [], used = new Set<number>();
  for (let k = 0; k < n; k++) {
    const lo = DICE[0] + ((DICE[1] - DICE[0]) * k) / n, hi = DICE[0] + ((DICE[1] - DICE[0]) * (k + 1)) / n;
    for (let j = 1; ; j++) {
      const s = base * j;
      if (used.has(s)) continue;
      const luck = new Rng(s).range(DICE[0], DICE[1]);
      if (luck >= lo && luck < hi) { out.push(s); used.add(s); break; }
    }
  }
  return out;
}
