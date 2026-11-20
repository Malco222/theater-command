// The guns hold their fire for their own fighters (Brian, 20 November 2026: "flak should stop in the air battles when
// their own fighters are in the air. for example german flak 88s would stop when their bf 109s would arrive to attack
// the bombers to prevent taking down their own planes"). While a defending fighter group is on a box inside the ring
// the guns are silent for that box, and they open up again when the fighters break off. Shown, never counted (ruling
// 237): the bombers lost and when each falls are the same with the rule on or off; only who brought them down moves.
import { afterEach, describe, expect, it } from 'vitest';
import { planRaid, raidTargets, type RaidKind } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import type { FieldEvent, FieldTicket } from '../src/battle/core/types';
import { SKY } from '../src/battle/air/book';
import { airBattle, AirSim } from '../src/battle/air/sim';
import { airTicket } from '../src/battle/air/ticket';
import type { AirPlace, AirUnit } from '../src/battle/air/types';

type Ticket = FieldTicket<AirPlace>;
const WARS: Record<string, string[]> = { GER: ['UK', 'FRA'], UK: ['GER'], FRA: ['GER'] };

/** The staff's raid on a named province with every fighter group that can go by day (tests/battle-air-fates.test.ts). */
function raid(nat: string, kind: RaidKind, name: string, turn = 0): Ticket {
  const g: Game = startOpen(createGame(7, nat));
  for (const w of WARS[nat]) setWar(g, nat, w);
  if (turn) g.turn = turn;
  const t = raidTargets(g, kind, nat).find((x) => planRaid(g, kind, x.id, undefined, nat).place === name);
  if (!t) throw new Error(`no ${kind} raid on ${name} for ${nat}`);
  const day = planRaid(g, kind, t.id, undefined, nat);
  const ids = [...day.picked, ...day.seats.filter((s) => !s.why && !s.picked && s.kind === 'ftr').map((s) => s.id)];
  return airTicket(g, kind, t.id, ids, nat).ticket!;
}

function fight(tk: Ticket, k: number, show = true, each?: (s: AirSim) => void): AirSim {
  const s = airBattle({ ...tk, seed: tk.seed + k }, { show });
  s.ai.att = true; s.ai.def = true;
  while (!s.over) { s.step(); each?.(s); }
  return s;
}

const RAIDS: [string, RaidKind, string, number][] = [['GER', 'bomb', 'Alsace', 0], ['GER', 'bomb', 'Kent', 0], ['FRA', 'bomb', 'Rhineland', 30], ['UK', 'bomb', 'Ruhr', 30]];
const RUNS = 6;
const tickets = RAIDS.map(([nat, kind, name, turn]) => raid(nat, kind, name, turn));
const lostOf = (h: FieldEvent[]) => h.filter((e) => e.what === 'air:lost');
const byOf = (e: FieldEvent): number | 'flak' | null | undefined => (e.extra as { by?: number | 'flak' | null }).by;
const inRing = (u: AirUnit): boolean => Math.hypot(u.x - SKY.target.x, u.y - SKY.target.y) <= SKY.flakR;
const pictureOf = (u: AirUnit) => u.extra as { flak?: string[]; hold?: number };

afterEach(() => { SKY.flak.hold = true; });

describe('the guns hold their fire for their own fighters', () => {
  it('every battle ends the same with the rule on and off: the counts, the bombs, the verdict, and the moment each aircraft falls', () => {
    let moved = 0;
    tickets.forEach((tk) => {
      for (let k = 0; k < RUNS; k++) {
        SKY.flak.hold = true;
        const on = fight(tk, k);
        SKY.flak.hold = false;
        const off = fight(tk, k);
        expect(on.outcome()).toEqual(off.outcome());
        expect(on.over!.level).toBe(off.over!.level);
        expect(on.over!.lines).toEqual(off.over!.lines);
        const when = (s: AirSim) => lostOf(s.history()).map((e) => `${e.t}:${e.unit}`);
        expect(when(on)).toEqual(when(off));
        // What moves is whose doing a loss was: fewer are the guns' with the rule on, never more.
        const guns = (s: AirSim) => lostOf(s.history()).filter((e) => byOf(e) === 'flak').length;
        expect(guns(on)).toBeLessThanOrEqual(guns(off));
        moved += guns(off) - guns(on);
        expect(off.history().some((e) => e.what === 'air:hold')).toBe(false);
      }
    });
    expect(moved).toBeGreaterThan(0);
  });

  it('no shell bursts round a box while a defending fighter group is on it, and none of its losses then is the guns\'', () => {
    let held = 0, firing = 0;
    tickets.forEach((tk) => {
      for (let k = 0; k < 3; k++) {
        const heldAt = new Map<number, number>();
        const s = fight(tk, k, true, (sim) => {
          const def = new Map(sim.units.filter((u) => u.side === 'def' && u.air === 'ftr').map((u) => [u.id, u]));
          for (const b of sim.units) {
            if (b.side !== 'att' || b.air !== 'bmr' || !b.extra) continue;
            const p = pictureOf(b);
            if (p.hold !== undefined) {
              held++;
              heldAt.set(b.id, sim.t);
              // Silent, for a fighter group of the defence that is in contact with the box, inside the ring.
              expect(p.flak).toEqual([]);
              expect(def.has(p.hold)).toBe(true);
              expect(b.contacts).toContain(p.hold);
              expect(inRing(b)).toBe(true);
            } else if (p.flak?.length) {
              firing++;
              // Firing: nobody of their own is on the box.
              const on = b.contacts.map((id) => def.get(id)).filter((d) => d && d.job.do !== 'home' && d.state !== 'routing');
              expect(on).toEqual([]);
            }
          }
        });
        // A bomber lost in the very tick the guns were holding for its box was not brought down by the guns.
        for (const e of lostOf(s.history())) if (heldAt.get(e.unit!) === e.t && e.side === 'att') expect(byOf(e)).not.toBe('flak');
      }
    });
    expect(held).toBeGreaterThan(0);
    expect(firing).toBeGreaterThan(0);
  });

  it('the guns open up again when the fighters break off', () => {
    let again = 0;
    tickets.forEach((tk) => {
      for (let k = 0; k < RUNS; k++) {
        const was = new Set<number>(), back = new Set<number>();
        fight(tk, k, true, (sim) => {
          for (const b of sim.units) {
            if (b.side !== 'att' || b.air !== 'bmr' || !b.extra) continue;
            const p = pictureOf(b);
            if (p.hold !== undefined) was.add(b.id);
            else if (p.flak?.length && was.has(b.id)) back.add(b.id);
          }
        });
        again += back.size;
      }
    });
    expect(again).toBeGreaterThan(0);
  });

  it('a shown battle tells each box\'s first holding of fire and first opening up again, once each, and an unshown one neither', () => {
    let holds = 0, opens = 0;
    tickets.forEach((tk) => {
      for (let k = 0; k < RUNS; k++) {
        const s = fight(tk, k, true);
        const told = s.history().filter((e) => e.what === 'air:hold');
        const key = (e: FieldEvent) => `${e.unit}:${(e.extra as { hold: boolean }).hold}`;
        expect(new Set(told.map(key)).size).toBe(told.length);
        for (const e of told) {
          const x = e.extra as { box: number; by: number; hold: boolean };
          expect(x.box).toBe(e.unit);
          const box = s.unit(x.box)!, by = s.unit(x.by)!;
          expect(box.air).toBe('bmr');
          expect([by.side, by.air]).toEqual(['def', 'ftr']);
          expect(e.text).toContain(`guns of ${tk.place.name}`);
          if (x.hold) { holds++; expect(e.text).toMatch(/^The guns of .+ hold their fire as .+ (goes|go) in on .+\.$/); }
          else {
            opens++;
            expect(e.text).toMatch(/ (breaks|break) off, and the guns of .+ open up again on .+\.$/);
            // Never an opening before the holding it ends.
            expect(told.some((h) => h.unit === e.unit && (h.extra as { hold: boolean }).hold && h.t <= e.t)).toBe(true);
          }
        }
        expect(fight(tk, k, false).history().some((e) => e.what === 'air:hold')).toBe(false);
      }
    });
    expect(holds).toBeGreaterThan(0);
    expect(opens).toBeGreaterThan(0);
  });

  it('with no fighters up, the guns fire all the way in and nothing is held', () => {
    const tk = tickets[0];
    const bare: Ticket = { ...tk, def: { ...tk.def, forces: [] } };
    let firing = 0;
    const s = fight(bare, 0, true, (sim) => {
      for (const b of sim.units) if (b.side === 'att' && b.air === 'bmr' && b.extra) { expect(pictureOf(b).hold).toBeUndefined(); if (pictureOf(b).flak?.length) firing++; }
    });
    expect(firing).toBeGreaterThan(0);
    expect(s.history().some((e) => e.what === 'air:hold')).toBe(false);
  });
});
