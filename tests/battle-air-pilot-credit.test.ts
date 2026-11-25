// R5 (the 3D air thread's pilot design, pilot-ai-r2 section 7; the design director, 25 November 2026: "Q4 credit: no
// argmax, and no new draw on the rules' stream. R5 as the fractional ledger: each group credited its share of the owed
// losses, whole losses by the largest remainder"). Who a loss is booked to was the group that owed the most that tick,
// so of two equal groups on one box one took every loss. Now each loss credits every group that owed on the box its
// share of it, and is booked to the largest balance. Credit decides nothing else: the losses, the moment each falls
// and the main stream are the baseline's.
import { describe, expect, it } from 'vitest';
import type { AirSim } from '../src/battle/air/sim';
import { BASELINE, digest, fly, runs } from './battle-air-pilot-raids';

const ALL = runs();
type By = number | 'flak';
const ledgerOf = (s: AirSim): Map<number, Map<By, number>> => (s as unknown as { ledger: Map<number, Map<By, number>> }).ledger;
/** Each victim group's losses booked to each creditor. */
function booked(s: AirSim): Map<number, Map<By, number>> {
  const out = new Map<number, Map<By, number>>();
  for (const e of s.history()) {
    if (e.what !== 'air:lost') continue;
    const by = (e.extra as { by: By }).by, m = out.get(e.unit!) ?? new Map<By, number>();
    m.set(by, (m.get(by) ?? 0) + 1);
    out.set(e.unit!, m);
  }
  return out;
}

describe('credit by the fractional ledger (R5)', () => {
  it('every battle loses the same aircraft on each side, each at the same moment, as the baseline, and the main stream ends where it did', () => {
    for (const r of ALL) expect(digest(fly(r)), r.key).toBe(BASELINE[r.key]);
  });

  it('two equal defending groups on one box share its losses, each within one of its share (the largest debt gave one of them all)', () => {
    let shared = 0;
    for (const r of ALL.filter((x) => x.key.startsWith('twins:'))) {
      const s = fly(r);
      const b = s.units.find((u) => u.side === 'def' && u.name.endsWith(' II'))!;
      const a = s.units.find((u) => u.side === 'def' && u.name === b.name.slice(0, -3))!;
      // The two halves of the largest group: the same size, from the same field, with the same orders. (Each draws its
      // own luck when it first meets the raid, so what they owe is near, not the same.)
      expect(a.n0).toBe(b.n0);
      for (const [box, m] of booked(s)) {
        if (s.unit(box)!.air !== 'bmr') continue;
        const ka = m.get(a.id) ?? 0, kb = m.get(b.id) ?? 0;
        if (ka + kb < 10) continue;
        // Each within one of its share: the ledger's balance is its share less what is booked to it.
        const L = ledgerOf(s).get(box)!;
        for (const id of [a.id, b.id]) expect(Math.abs(L.get(id) ?? 0)).toBeLessThan(1);
        // And the box's losses between them are shared near evenly, where the argmax booked every one to one of them.
        expect(Math.min(ka, kb) / (ka + kb), `${r.key} box ${box}: ${ka} and ${kb}`).toBeGreaterThan(0.4);
        shared++;
      }
    }
    expect(shared).toBeGreaterThanOrEqual(8);
  });

  it('every loss is booked to exactly one group or the guns, and each creditor keeps within one of its share', () => {
    let creditors = 0;
    for (const r of ALL) {
      const s = fly(r);
      const lost = s.history().filter((e) => e.what === 'air:lost');
      expect(lost.length, r.key).toBe(s.units.reduce((t, u) => t + u.n0 - u.n, 0));
      for (const e of lost) {
        const by = (e.extra as { by?: By | null }).by;
        expect(by === 'flak' || (typeof by === 'number' && s.unit(by)!.side !== e.side), `${r.key} at ${e.t}`).toBe(true);
      }
      // The ledger's balance is each creditor's share of a group's losses less the losses booked to it.
      const B = booked(s);
      for (const [victim, L] of ledgerOf(s)) {
        let sum = 0;
        for (const [by, v] of L) {
          creditors++;
          sum += v;
          expect(Math.abs(v), `${r.key}: ${victim} by ${by}`).toBeLessThan(1);
        }
        // Shares sum to the losses: the balances to nothing.
        expect(Math.abs(sum)).toBeLessThan(1e-6);
        const n = [...(B.get(victim) ?? new Map()).values()].reduce((t, k) => t + k, 0);
        expect(n).toBe(s.unit(victim)!.n0 - s.unit(victim)!.n);
      }
    }
    expect(creditors).toBeGreaterThan(50);
  });
});
