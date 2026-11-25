// R1 (the 3D air thread's pilot design, pilot-ai-r2 section 7; the design director, 25 November 2026: "R1 (the pass
// drawn at the quarry): yes, with its test (totals AND credit by group and flak)"). The pass a fighter group will fly
// at a bomber box is drawn when it is given that box as its quarry, by the computer or by the player's order, not at
// contact, and the view carries it in the group's extra as 'pass' for as long as it has that quarry (the 3D air thread,
// 15:53Z: "Send it from the moment the group has its quarry; drop it when it has none"). The contact still names it.
// It is drawn again only when the quarry changes. Totals do not move: the pass comes from a stream of its own, and
// sets only the beat of the passes, which decides between the fighters and the guns who a box's loss is booked to.
import { describe, expect, it } from 'vitest';
import { AirSim as AirSimOf, type AirSim } from '../src/battle/air/sim';
import type { AirUnit } from '../src/battle/air/types';
import type { FieldEvent } from '../src/battle/core/types';
import { BASELINE, digest, fly, runs, type Run } from './battle-air-pilot-raids';

const ALL = runs();
const passOf = (u: AirUnit): string | undefined => (u.extra as { pass?: string } | undefined)?.pass;
/** The bomber box a fighter group has been given as its quarry: its job sends it at one, and it is still fighting. */
function quarry(s: AirSim, u: AirUnit): number | null {
  if (u.air !== 'ftr' || u.state === 'gone' || u.state === 'withdrawn' || u.state === 'routing' || u.job.do !== 'intercept') return null;
  const b = s.unit(u.job.on);
  return b && b.air === 'bmr' && b.side !== u.side && b.state !== 'gone' && b.state !== 'withdrawn' && b.job.do !== 'wait' ? b.id : null;
}

describe('the pass is drawn when the fighters are given their quarry (R1)', () => {
  it('every battle loses the same aircraft on each side, each at the same moment, as the baseline; the main stream ends where it did', () => {
    // Four raids and five seeds: the guns and fighters together (Kent, Alsace), two equal defending groups, the
    // defender's order mid-battle, and a fight for the sky with no bombers.
    for (const r of ALL) expect(digest(fly(r)), r.key).toBe(BASELINE[r.key]);
  });

  it('every loss is booked to one group or to the guns', () => {
    for (const r of ALL.filter((x) => x.key.endsWith(':0') || x.key.endsWith(':1'))) {
      const s = fly(r);
      const lost = s.history().filter((e) => e.what === 'air:lost');
      const all = s.units.reduce((t, u) => t + u.n0 - u.n, 0);
      expect(lost.length, r.key).toBe(all);
      for (const e of lost) {
        const by = (e.extra as { by?: number | 'flak' | null }).by;
        expect(by === 'flak' || (typeof by === 'number' && s.unit(by)!.side !== e.side), `${r.key} at ${e.t}`).toBe(true);
      }
    }
  });

  it("extra.pass: from the think that gives the quarry, as long as the group has it, absent with none; it is the contact's, and changes only with the quarry", () => {
    let shown = 0, contacts = 0, changed = 0, ordered = 0;
    const check = (r: Run): void => {
      const last = new Map<number, { q: number | null; pass?: string }>();
      let seen = 0;
      const look = (s: AirSim): void => {
        const h = s.history(), now: FieldEvent[] = h.slice(seen);
        seen = h.length;
        for (const u of s.units) {
          if (u.air !== 'ftr' || !u.extra) continue;
          const q = quarry(s, u), pass = passOf(u), was = last.get(u.id);
          if (q === null) expect(pass, `${r.key} ${u.id} at ${s.t}`).toBeUndefined();
          else { expect(pass, `${r.key} ${u.id} at ${s.t}`).toBeDefined(); shown++; }
          // The same quarry as the tick before: the same pass.
          if (was && q !== null && was.q === q) expect(pass).toBe(was.pass);
          if (was && q !== null && was.q !== null && was.q !== q) changed++;
          last.set(u.id, { q, pass });
          // The contact names the pass the group shows for its quarry.
          for (const e of now) {
            if (e.what !== 'contact' || e.unit !== u.id) continue;
            const x = e.extra as { with?: number; pass?: string } | undefined;
            if (q === null || x?.with !== q) continue;
            contacts++;
            expect(x.pass, `${r.key} ${u.id} at ${e.t}`).toBe(pass);
          }
        }
      };
      const s = fly(r, look);
      if (r.play === 'order') ordered++;
      expect(s.over).not.toBeNull();
    };
    for (const r of ALL.filter((x) => Number(x.key.split(':')[1]) < 3)) check(r);
    expect(shown).toBeGreaterThan(1000);
    expect(contacts).toBeGreaterThan(10);
    expect(changed).toBeGreaterThan(0);
    expect(ordered).toBe(3);
  });

  it('the opening view, before the clock starts, already carries the pass of each group the computer sent at a box', () => {
    let n = 0;
    for (const r of ALL.filter((x) => x.key.endsWith(':0'))) {
      const s = new AirSimOf(r.ticket, { show: true });
      for (const u of s.units) if (quarry(s, u) !== null) { n++; expect(passOf(u), r.key).toBeDefined(); } else if (u.air === 'ftr') expect(passOf(u)).toBeUndefined();
    }
    expect(n).toBeGreaterThan(0);
  });

  it("the player's order mid-battle gives the group its new quarry, and the next view its pass", () => {
    let given = 0, moved = 0;
    for (const r of ALL.filter((x) => x.play === 'ai' && x.key.startsWith('kent:'))) {
      const s = new AirSimOf(r.ticket, { show: true });
      s.ai.att = true; s.ai.def = true;
      while (!s.fired && !s.over) s.step();
      const f = s.units.find((u) => u.side === 'def' && u.air === 'ftr' && quarry(s, u) !== null)!;
      const was = { q: quarry(s, f), pass: passOf(f) };
      const to = s.units.find((u) => u.side === 'att' && u.air === 'bmr' && u.state !== 'gone' && u.id !== was.q)!;
      expect(s.command({ do: 'attack', units: [f.id], target: to.id }, 'def')).toBe(true);
      s.step();
      expect(quarry(s, f)).toBe(to.id);
      expect(passOf(f)).toBeDefined();
      given++;
      if (passOf(f) !== was.pass) moved++;
      // Kept while the quarry is: the same pass tick after tick.
      const p = passOf(f);
      for (let i = 0; i < 20 && !s.over && quarry(s, f) === to.id; i++) { s.step(); if (quarry(s, f) === to.id) expect(passOf(f)).toBe(p); }
    }
    expect(given).toBe(5);
  });
});
