// R3 (the 3D air thread's pilot design, pilot-ai-r2 section 7; the design director, 25 November 2026: "R2 ('by' and
// 'gun' on air:hit) and R3 (the leader's choice in the view): yes, shown only"). Each fighter group's view extra
// carries its leader's choice: 'aim' (what it is set on), 'threat' (the enemy group coming down on it, and whether it
// is a bounce) and, for an escort, 'answer' (the enemy it has turned on and the box of its raid that enemy threatens).
// Shown only: the same battles lose the same aircraft at the same moments, and the main stream ends where it did
// (tests/battle-air-pilot-pass.test.ts and the other pilot tests hold the digests of tests/battle-air-pilot-raids.ts).
import { describe, expect, it } from 'vitest';
import type { AirSim } from '../src/battle/air/sim';
import type { AirAim, AirUnit } from '../src/battle/air/types';
import { fly, runs } from './battle-air-pilot-raids';

const AIMS = ['bombers', 'escort', 'wait', 'turn'];
const choiceOf = (u: AirUnit): AirAim => (u.extra ?? {}) as AirAim;

describe("the leader's choice in the view (R3, shown only)", () => {
  it('aim, threat and answer take only the allowed values, and name the groups they should', () => {
    const seen = { aim: new Set<string>(), threat: 0, bounce: 0, answer: 0, otherBox: 0 };
    const look = (key: string) => (s: AirSim): void => {
      for (const u of s.units) {
        if (u.air !== 'ftr' || !u.extra) continue;
        const c = choiceOf(u), at = `${key} ${u.id} at ${s.t}`;
        if (c.aim !== undefined) {
          expect(AIMS, at).toContain(c.aim);
          // An escort's choice is its answer, never an aim.
          expect(u.job.do, at).not.toBe('escort');
          seen.aim.add(c.aim);
        }
        if (c.threat !== undefined) {
          // A threat is an enemy fighter group.
          const v = s.unit(c.threat.by);
          expect(v, at).toBeDefined();
          expect(v!.side, at).not.toBe(u.side);
          expect(v!.air, at).toBe('ftr');
          expect(typeof c.threat.bounce, at).toBe('boolean');
          seen.threat++;
          if (c.threat.bounce) seen.bounce++;
        }
        if (c.answer !== undefined) {
          // Only an escort answers; its box is a box of its raid, and the enemy it answers an enemy fighter group.
          expect(u.job.do, at).toBe('escort');
          const b = s.unit(c.answer.box), e = s.unit(c.answer.on);
          expect(b?.air, at).toBe('bmr');
          expect(b?.side, at).toBe(u.side);
          expect(e?.air, at).toBe('ftr');
          expect(e?.side, at).not.toBe(u.side);
          seen.answer++;
          if (u.job.do === 'escort' && c.answer.box !== u.job.on) seen.otherBox++;
        }
      }
    };
    // Alsace (an escort and the defence's fighters on the bombers), Kent with the defender's order, and the fight for
    // the sky (fighters only: hunters on both sides).
    for (const r of runs().filter((x) => /^(alsace|order|sky):[01]$/.test(x.key))) {
      const s = fly(r, look(r.key));
      expect(s.over, r.key).not.toBeNull();
    }
    // Today only 'bombers' and 'escort' occur: 'wait' and 'turn' are R7's.
    expect([...seen.aim].sort()).toEqual(['bombers', 'escort']);
    expect(seen.threat).toBeGreaterThan(0);
    expect(seen.answer).toBeGreaterThan(0);
  });
});
