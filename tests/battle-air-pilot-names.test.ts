// The gunner's seat on a stream of its own (the design director, 25 November 2026 16:54Z: "split the gunner's seat and
// the fallen bomber's name onto a stream of their own, in the M1 round ... it stops every later booking change from
// re-throwing which named crew falls, and names carry tours"). Which named bomber falls, or falls out of formation, is
// drawn on the battle's fork 59 (seatRng); which bomber's gunners are credited with a fighter booked to their box is
// drawn on fork 73 (gunRng). So how a loss is booked (the creditor) can change without moving which crew falls.
import { describe, expect, it } from 'vitest';
import { AirSim } from '../src/battle/air/sim';
import type { BomberTold } from '../src/battle/air/types';
import type { FieldEvent } from '../src/battle/core/types';
import { BASELINE, digest, fly, runs, type Run } from './battle-air-pilot-raids';

const ALL = runs();
type By = number | 'flak' | null;
const bomberOf = (e: FieldEvent): BomberTold | undefined => (e.extra as { bomber?: BomberTold } | undefined)?.bomber;
const gunnerOf = (e: FieldEvent): (BomberTold & { box: number }) | undefined => (e.extra as { gunner?: BomberTold & { box: number } } | undefined)?.gunner;

/** Flies a battle with the booking swapped for another rule: every loss to the lowest id that owed on it this tick (flak last). */
function flyLowest(r: Run): AirSim {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const P = AirSim.prototype as any;
  const own = P.creditor;
  P.creditor = function (this: { owedBy: Map<number, Map<number | 'flak', number>> }, u: { id: number }): By {
    const due = [...(this.owedBy.get(u.id) ?? new Map<number | 'flak', number>())].filter(([, k]) => k > 0).map(([by]) => by);
    const ids = due.filter((x): x is number => typeof x === 'number').sort((a, b) => a - b);
    return ids.length ? ids[0] : due.includes('flak') ? 'flak' : null;
  };
  try { return fly(r); } finally { P.creditor = own; }
}

/** Every bomber lost by name, in order: [time, box, seat, name]. */
const named = (s: AirSim): string[] => s.history().filter((e) => e.what === 'air:lost' && bomberOf(e)).map((e) => `${e.t}:${e.unit}:${bomberOf(e)!.seat}:${bomberOf(e)!.name}`);
const fells = (s: AirSim) => s.outcome().groups.map((g) => [g.id, g.fell ?? null]);
const shots = (s: AirSim) => JSON.stringify(s.outcome().groups.map((g) => g.shot ?? null));
const guns = (s: AirSim) => s.history().filter((e) => e.what === 'air:lost' && gunnerOf(e)).map((e) => `${e.t}:${gunnerOf(e)!.box}:${gunnerOf(e)!.seat}`).join(',');

describe('the gunner\'s seat on a stream of its own', () => {
  it('the pilot raids still give their pinned digests (losses, the (time, victim) list, the main stream, the hits)', () => {
    for (const r of ALL) expect(digest(fly(r)), r.key).toBe(BASELINE[r.key]);
  }, 300_000);

  it('the named bombers that fall, and the fell lists handed back, do not hang on how losses are booked', () => {
    let names = 0, movedCredit = 0, movedGuns = 0;
    for (const r of ALL) {
      const a = fly(r), b = flyLowest(r);
      // The swapped booking moves no count.
      expect(digest(b).split(' ').slice(0, 3), r.key).toEqual(digest(a).split(' ').slice(0, 3));
      expect(named(b), r.key).toEqual(named(a));
      expect(fells(b), r.key).toEqual(fells(a));
      names += named(a).length;
      const by = (s: AirSim) => s.history().filter((e) => e.what === 'air:lost').map((e) => (e.extra as { by: By }).by).join();
      if (by(a) !== by(b)) movedCredit++;
      if (shots(a) !== shots(b) || guns(a) !== guns(b)) movedGuns++;
    }
    // The test means something: bombers fall by name, the swap does move the booking, and with it the gunners credited.
    expect(names).toBeGreaterThan(100);
    expect(movedCredit).toBeGreaterThan(5);
    expect(movedGuns).toBeGreaterThan(0);
  }, 300_000);

  it('every gunner credited is a bomber of that box by name, still flying when the fighter fell', () => {
    let credited = 0;
    for (const r of ALL) {
      for (const s of [fly(r), flyLowest(r)]) {
        const gone = new Map<number, Set<number>>();
        for (const e of s.history()) {
          if (e.what !== 'air:lost') continue;
          const b = bomberOf(e);
          if (b) { (gone.get(e.unit!) ?? gone.set(e.unit!, new Set()).get(e.unit!)!).add(b.seat); continue; }
          const g = gunnerOf(e);
          if (!g) continue;
          const box = s.unit(g.box)!;
          const seats = r.ticket.place.bombers![box.corps].seats;
          expect(box.side, r.key).toBe('att');
          expect(box.air, r.key).toBe('bmr');
          expect(Number.isInteger(g.seat) && g.seat >= 0 && g.seat < Math.min(box.n0, seats.length), r.key).toBe(true);
          expect(g.name, r.key).toBe(seats[g.seat].name);
          expect(gone.get(box.id)?.has(g.seat) ?? false, r.key).toBe(false);
          credited++;
        }
      }
    }
    expect(credited).toBeGreaterThan(20);
  }, 300_000);
});
