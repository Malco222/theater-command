// R2 (the 3D air thread's pilot design, pilot-ai-r2 section 7; the design director, 25 November 2026: "R2 ('by' and
// 'gun' on air:hit): yes, shown only"). Every hit an aircraft survives ('air:hit') says whose fire it was: 'by', the
// enemy group the rules held owed for it at that moment (its id, as the view gives it) or 'flak'; and 'gun', the gun's
// name, where every gun that group carries has the one name. Shown only: no draw is added, so the battle, its losses and
// every other event are what they were.
import { afterEach, describe, expect, it } from 'vitest';
import { SKY } from '../src/battle/air/book';
import { AirSim } from '../src/battle/air/sim';
import type { FieldEvent } from '../src/battle/core/types';
import { BASELINE, digest, fly, runs, stripHit } from './battle-air-pilot-raids';

const ALL = runs();
// Three seeds of each raid: the guns and fighters together (Kent, Alsace), two equal defending groups, the defender's
// order, and the fight for the sky.
const SOME = ALL.filter((r) => Number(r.key.split(':')[1]) < 3);
type Who = { by?: number | 'flak'; gun?: string };
const whoOf = (e: FieldEvent): Who => e.extra as Who;
const proto = AirSim.prototype as unknown as Record<string, unknown>;
const HIT_BY = proto.hitBy;
afterEach(() => { proto.hitBy = HIT_BY; });

describe("every hit says whose it was (R2: 'by' and 'gun' on air:hit), shown only", () => {
  it('the losses, the moment and victim of each, the main stream and the hits themselves are the baseline\'s', () => {
    for (const r of SOME) expect(digest(fly(r)), r.key).toBe(BASELINE[r.key]);
  });

  it('with the hits\' who taken out, the whole event stream is the one the battle gives without it', () => {
    for (const r of SOME) {
      const with_ = fly(r).history().map(stripHit);
      // Without R2: no group is ever named for a hit.
      proto.hitBy = () => null;
      const without = fly(r).history();
      proto.hitBy = HIT_BY;
      expect(without.some((e) => e.what === 'air:hit' && whoOf(e).by !== undefined)).toBe(false);
      expect(JSON.stringify(with_), r.key).toBe(JSON.stringify(without));
    }
  });

  it('each hit is by an enemy group firing on the group hit that tick, live or lost in that tick, or by the guns', () => {
    let hits = 0, flak = 0, groups = 0, guns = 0, onBoxes = 0;
    for (const r of SOME) {
      let seen = 0;
      fly(r, (s) => {
        const h = s.history();
        const now = h.slice(seen);
        seen = h.length;
        const lostNow = new Set(now.filter((e) => e.what === 'air:lost').map((e) => e.unit));
        for (const e of now.filter((x) => x.what === 'air:hit')) {
          hits++;
          const u = s.unit(e.unit!)!, { by, gun } = whoOf(e);
          expect(by, `${r.key} at ${e.t}`).toBeDefined();
          if (by === 'flak') {
            flak++;
            expect(gun).toBeUndefined();
            // The guns round the target (or a strafing group's ground fire).
            expect(Math.hypot(u.x - SKY.target.x, u.y - SKY.target.y) <= SKY.flakR || u.job.do === 'strafe').toBe(true);
            continue;
          }
          groups++;
          const v = s.unit(by as number)!;
          expect(v.side).not.toBe(u.side);
          expect(v.state !== 'gone' || lostNow.has(v.id)).toBe(true);
          // It owed on the group this tick: it was firing on it (a box's attackers and gunners alike).
          expect(v.fire.some((f) => f.on === u.id && f.rate > 0), `${r.key} at ${e.t}: ${v.id} on ${u.id}`).toBe(true);
          if (u.air === 'bmr') onBoxes++;
          if (gun !== undefined) {
            guns++;
            const L = s.place.guns?.[v.nat]?.[v.air];
            if (L) expect(new Set(L.guns.map((g) => g.gun))).toEqual(new Set([gun]));
          }
        }
      });
    }
    expect(hits).toBeGreaterThan(100);
    expect(flak).toBeGreaterThan(0);
    expect(groups).toBeGreaterThan(0);
    expect(onBoxes).toBeGreaterThan(0);
    expect(guns).toBeGreaterThan(0);
  });
});
