// The guns hold their fire for their own fighters (Brian, 20 November 2026: "flak should stop in the air battles when
// their own fighters are in the air. for example german flak 88s would stop when their bf 109s would arrive to attack
// the bombers to prevent taking down their own planes"). While a defending fighter group is going in on a box inside
// the ring the guns are silent for that box. They fire in the gaps between its passes, while the fighters stand off to
// form up again, and once the fighters have gone (the coordinator's ruling, 20 November: the first build held the guns
// for as long as fighters were anywhere on a box, which left a defended raid with no flak in it at all). Shown, never
// counted (ruling 237): the bombers lost and when each falls are the same with the rule on or off; only who brought
// them down moves.
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

const OFF = { ...SKY.flak.pass.off };
afterEach(() => { SKY.flak.hold = true; Object.assign(SKY.flak.pass.off, OFF); });

type Fire = { on: number; go?: number; wait?: number };
const fireOn = (d: AirUnit, box: number): Fire | undefined => (d.fire as Fire[]).find((f) => f.on === box);
/** The defence's fighter groups firing on a box this tick. */
const attackers = (sim: AirSim, b: AirUnit): AirUnit[] => b.contacts.map((id) => sim.unit(id)!).filter((d) => d.side === 'def' && d.air === 'ftr' && fireOn(d, b.id));
const boxes = (sim: AirSim): AirUnit[] => sim.units.filter((b) => b.side === 'att' && b.air === 'bmr' && !!b.extra);

describe('the guns hold their fire for their own fighters', () => {
  it('every battle ends the same with the rule on, off, and as first built: the counts, the bombs, the verdict, and the moment each aircraft falls', () => {
    let moved = 0, more = 0;
    tickets.forEach((tk) => {
      for (let k = 0; k < RUNS; k++) {
        SKY.flak.hold = true;
        const on = fight(tk, k);
        SKY.flak.hold = false;
        const off = fight(tk, k);
        // The first build's rule: no gaps, so the guns are silent for as long as fighters are attacking a box.
        SKY.flak.hold = true;
        for (const p of Object.keys(SKY.flak.pass.off)) SKY.flak.pass.off[p] = 0;
        const strict = fight(tk, k);
        Object.assign(SKY.flak.pass.off, OFF);
        const when = (s: AirSim) => lostOf(s.history()).map((e) => `${e.t}:${e.unit}`);
        for (const other of [off, strict]) {
          expect(on.outcome()).toEqual(other.outcome());
          expect(on.over!.level).toBe(other.over!.level);
          expect(on.over!.lines).toEqual(other.over!.lines);
          expect(when(on)).toEqual(when(other));
        }
        // What moves is whose doing a loss was: fewer are the guns' with the rule on, never more, and fewer still
        // with no gaps between the passes.
        const guns = (s: AirSim) => lostOf(s.history()).filter((e) => byOf(e) === 'flak').length;
        expect(guns(on)).toBeLessThanOrEqual(guns(off));
        expect(guns(strict)).toBeLessThanOrEqual(guns(on));
        moved += guns(off) - guns(on);
        more += guns(on) - guns(strict);
        expect(off.history().some((e) => e.what === 'air:hold')).toBe(false);
      }
    });
    expect(moved).toBeGreaterThan(0);
    expect(more).toBeGreaterThan(0);
  });

  it('no shell bursts round a box while a defending fighter group is going in on it, and none of its losses then is the guns\'', () => {
    let held = 0, firing = 0, between = 0;
    tickets.forEach((tk) => {
      for (let k = 0; k < 3; k++) {
        const heldAt = new Map<number, number>();
        const s = fight(tk, k, true, (sim) => {
          for (const b of boxes(sim)) {
            const p = pictureOf(b) as { flak?: string[]; hold?: number; holdLeft?: number; holdIn?: number };
            // The defence's fire on the box that is in the beat of its passes: going in, or standing off.
            const on = attackers(sim, b).filter((d) => fireOn(d, b.id)!.go !== undefined || fireOn(d, b.id)!.wait !== undefined);
            if (p.hold !== undefined) {
              held++;
              heldAt.set(b.id, sim.t);
              // Silent, for a fighter group of the defence that is firing on the box, inside the ring, on its pass.
              expect(p.flak).toEqual([]);
              expect(on.map((d) => d.id)).toContain(p.hold);
              expect(inRing(b)).toBe(true);
              expect(p.holdLeft).toBeGreaterThan(0);
              expect(p.holdIn).toBeUndefined();
              for (const d of on) { expect(fireOn(d, b.id)!.go).toBe(p.holdLeft); expect(fireOn(d, b.id)!.wait).toBeUndefined(); }
            } else if (p.flak?.length) {
              firing++;
              // Firing: nobody of their own is going in on the box. Any fighters still on it are standing off.
              for (const d of on) { expect(fireOn(d, b.id)!.go).toBeUndefined(); expect(fireOn(d, b.id)!.wait).toBe(p.holdIn); }
              if (on.length) { between++; expect(p.holdIn).toBeGreaterThan(0); } else expect(p.holdIn).toBeUndefined();
            }
          }
        });
        // A bomber lost in the very tick the guns were holding for its box was not brought down by the guns.
        for (const e of lostOf(s.history())) if (heldAt.get(e.unit!) === e.t && e.side === 'att') expect(byOf(e)).not.toBe('flak');
      }
    });
    expect(held).toBeGreaterThan(0);
    expect(firing).toBeGreaterThan(0);
    expect(between).toBeGreaterThan(0);
  });

  it('the guns fire in the gaps between the passes: a box under attack all the way in is shelled, spared, and shelled again', () => {
    // Alsace: the French fighters are on both boxes from the edge of the ring to the drop.
    const tk = tickets[0];
    let both = 0;
    for (let k = 0; k < RUNS; k++) {
      const per = new Map<number, { quiet: number; shelled: number; turns: number; last: string }>();
      fight(tk, k, true, (sim) => {
        for (const b of boxes(sim)) {
          if (!attackers(sim, b).length) continue;
          const p = pictureOf(b);
          const now = p.hold !== undefined ? 'quiet' : p.flak?.length ? 'shelled' : '';
          if (!now) continue;
          const r = per.get(b.id) ?? { quiet: 0, shelled: 0, turns: 0, last: '' };
          r[now as 'quiet' | 'shelled']++;
          if (now !== r.last) { r.turns++; r.last = now; }
          per.set(b.id, r);
        }
      });
      for (const r of per.values()) {
        if (r.quiet + r.shelled < 300) continue; // under attack for most of the way in
        both++;
        // Neither all silence nor all fire: the guns have between a quarter and three quarters of the time.
        const share = r.shelled / (r.quiet + r.shelled);
        expect(share).toBeGreaterThan(0.25);
        expect(share).toBeLessThan(0.8);
        // At least two passes, with the guns firing between them.
        expect(r.turns).toBeGreaterThanOrEqual(3);
      }
    }
    expect(both).toBeGreaterThan(0);
  });

  it('a pass and a gap each last as the book has them, and with no gaps the guns never fire on a box under attack', () => {
    const P = SKY.flak.pass;
    const most = (m: Record<string, number>) => Math.max(...Object.values(m)) * (1 + P.vary), least = (m: Record<string, number>) => Math.min(...Object.values(m)) * (1 - P.vary);
    let seen = 0;
    fight(tickets[0], 0, true, (sim) => {
      for (const d of sim.units) for (const f of d.fire as Fire[]) {
        if (f.go !== undefined) { seen++; expect(f.go).toBeLessThanOrEqual(most(P.go)); expect(f.go).toBeGreaterThan(0); }
        if (f.wait !== undefined) { seen++; expect(f.wait).toBeLessThanOrEqual(most(P.off)); expect(f.wait).toBeGreaterThan(0); }
      }
    });
    expect(seen).toBeGreaterThan(0);
    expect(least(P.off)).toBeGreaterThan(5);
    for (const p of Object.keys(P.off)) P.off[p] = 0;
    let shelled = 0, quiet = 0;
    fight(tickets[0], 0, true, (sim) => {
      for (const b of boxes(sim)) {
        if (!attackers(sim, b).length) continue;
        if (pictureOf(b).flak?.length) shelled++;
        if (pictureOf(b).hold !== undefined) quiet++;
      }
    });
    expect(shelled).toBe(0);
    expect(quiet).toBeGreaterThan(0);
  });

  it('the beat is the same in a shown battle and an unshown one, and a new attack opens with a pass', () => {
    const tk = tickets[0];
    const beat = (show: boolean): string[] => {
      const out: string[] = [];
      const was = new Set<string>();
      fight(tk, 1, show, (sim) => {
        const now = new Set<string>();
        for (const d of sim.units) for (const f of d.fire as Fire[]) {
          if (f.go === undefined && f.wait === undefined) continue;
          const key = `${d.id}:${f.on}`;
          now.add(key);
          // The first tick of an attack on a box is the first tick of a pass.
          if (!was.has(key) && ![...was].some((x) => x.endsWith(`:${f.on}`))) expect(f.go).toBeGreaterThan(0);
          out.push(`${sim.t}:${key}:${f.go ?? ''}:${f.wait ?? ''}`);
        }
        was.clear(); for (const x of now) was.add(x);
      });
      return out;
    };
    const shown = beat(true);
    expect(shown.length).toBeGreaterThan(0);
    expect(beat(false)).toEqual(shown);
  });

  it('the guns open up again when the fighters pull away or break off', () => {
    let again = 0;
    tickets.forEach((tk) => {
      for (let k = 0; k < RUNS; k++) {
        const was = new Set<number>(), back = new Set<number>();
        fight(tk, k, true, (sim) => {
          for (const b of boxes(sim)) {
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
    let holds = 0, opens = 0, between = 0;
    tickets.forEach((tk) => {
      for (let k = 0; k < RUNS; k++) {
        const s = fight(tk, k, true);
        const told = s.history().filter((e) => e.what === 'air:hold');
        const key = (e: FieldEvent) => `${e.unit}:${(e.extra as { hold: boolean }).hold}`;
        expect(new Set(told.map(key)).size).toBe(told.length);
        for (const e of told) {
          const x = e.extra as { box: number; by: number; hold: boolean; between?: boolean };
          expect(x.box).toBe(e.unit);
          const box = s.unit(x.box)!, by = s.unit(x.by)!;
          expect(box.air).toBe('bmr');
          expect([by.side, by.air]).toEqual(['def', 'ftr']);
          expect(e.text).toContain(`guns of ${tk.place.name}`);
          if (x.hold) { holds++; expect(e.text).toMatch(/^The guns of .+ hold their fire as .+ (goes|go) in on .+\.$/); }
          else {
            opens++;
            // Between two passes the fighters pull away; when they are done with the box they break off.
            if (x.between) { between++; expect(e.text).toMatch(/ (pulls|pull) away, and the guns of .+ open up again on .+\.$/); }
            else expect(e.text).toMatch(/ (breaks|break) off, and the guns of .+ open up again on .+\.$/);
            // Never an opening before the holding it ends.
            expect(told.some((h) => h.unit === e.unit && (h.extra as { hold: boolean }).hold && h.t <= e.t)).toBe(true);
          }
        }
        expect(fight(tk, k, false).history().some((e) => e.what === 'air:hold')).toBe(false);
      }
    });
    expect(holds).toBeGreaterThan(0);
    expect(opens).toBeGreaterThan(0);
    expect(between).toBeGreaterThan(0);
  });

  it('with no fighters up, the guns fire all the way in and nothing is held', () => {
    const tk = tickets[0];
    const bare: Ticket = { ...tk, def: { ...tk.def, forces: [] } };
    let firing = 0;
    const s = fight(bare, 0, true, (sim) => {
      for (const b of boxes(sim)) { expect(pictureOf(b).hold).toBeUndefined(); expect((pictureOf(b) as { holdIn?: number }).holdIn).toBeUndefined(); if (pictureOf(b).flak?.length) firing++; }
    });
    expect(firing).toBeGreaterThan(0);
    expect(s.history().some((e) => e.what === 'air:hold')).toBe(false);
  });
});
