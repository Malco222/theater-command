// Support teams in battle, the staff's half (ai.ts; behind sim.teamsRule, off): the design director's ruling of 25
// November 2026 20:03Z on the ground thread's teams-traces-1125.md, "a team is its own division's, never a gun".
//   1. The plan (makePlan): a division's own teams are still its weight, on its own line and on the enemy's, so a force
//      that brings its own teams makes the plan it makes without them. Before, the main effort's share was summed with
//      the teams' 8% out of each infantry division: Normandy's forces fell 0.13 short of it, a fourth block joined, the
//      last tank block never pins alone, and nobody pinned.
//   2. The aim (planned): a team is never laid on the main effort's target as the guns are (it fires on what its own
//      weapon harms most: sim targetOf, tests/land-own-teams.test.ts).
//   3. The wait (planned): a team is not a gun for the preparation. An army whose only fire is its own teams goes in
//      when it is on its start line.
// Off, each is as it was (the second half of each test).
import { describe, expect, it } from 'vitest';
import mlawaLook from '../unreal/TheaterCommand/Tools/land_live/campaign-look-mlawa.json';
import { isOwnTeam, ownTeams } from '../src/battle/land/own-teams';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { LandBattle } from '../src/battle/land/sim';
import type { FieldTicket, SideTicket } from '../src/battle/core/types';
import type { LandPlace } from '../src/battle/land/field';

type Ticket = FieldTicket<LandPlace>;
type U = { id: number; side: 'att' | 'def'; kind: string; team?: string; name: string; short: string; aim?: number };
type Plan = { main: number[]; role: Record<number, 'main' | 'pin'>; went: number | null };

/** A set piece's ticket as a campaign battle's: without the piece's own battalions, its shares whole again. */
function campaign(key: string, seed: number): Ticket {
  const t = ticketOf(PIECES.find((x) => x.key === key)!, seed, null);
  const side = (s: SideTicket): SideTicket => ({ ...s, forces: s.forces.filter((f) => f.kind !== 'mg' && f.kind !== 'mor').map((f) => ({ ...f, share: f.share / 0.92 })) });
  return { ...t, att: side(t.att), def: side(t.def), reckoning: { ...t.reckoning, odds: 1.2, D: t.reckoning.A / 1.2 } };
}
/** A campaign ticket of foot against foot with no guns (the land_live look of Mlawa), the computer on both sides. */
function mlawa(seed: number): Ticket {
  const d = mlawaLook as unknown as { ticket?: Ticket } & Ticket;
  const t = (d.ticket ?? d) as Ticket, side = (s: SideTicket): SideTicket => ({ ...s, human: false });
  return { ...t, seed, att: side(t.att), def: side(t.def) };
}
/** The battle fought to a second, and what the attack's staff planned: its main stretch, each line block's role, when it went in. */
function fought(t: Ticket, rule: boolean, until: number): { units: U[]; plan: Plan; roles: string[]; main: string[] } {
  const sim = new LandBattle(t);
  sim.teamsRule = rule;
  let guard = 0;
  while (sim.t < until && !sim.over && guard++ < 2_000_000) sim.step();
  const units = sim.units as unknown as U[], plan = (sim as unknown as { plan: { att?: Plan } }).plan.att!;
  const nm = (id: number): string => { const u = units.find((x) => x.id === id)!; return `${u.side}:${u.name}:${u.short}`; };
  return { units, plan, main: plan.main.map(nm), roles: units.filter((u) => u.side === 'att' && u.kind !== 'art').map((u) => `${nm(u.id)}=${plan.role[u.id]}`).sort() };
}

describe("support teams in battle: to the staff a team is its own division's, never a gun (ai.ts)", () => {
  it('1. on, a force that brings its own teams makes the plan it makes without them; off, nobody pins', () => {
    for (const seed of [1, 2, 3]) {
      const t = campaign('normandy-1944', seed), o = ownTeams(t).ticket;
      const without = fought(t, true, 1), on = fought(o, true, 1), off = fought(o, false, 1);
      expect(without.roles.filter((r) => r.endsWith('=pin')).length).toBe(2);
      expect(on.roles).toEqual(without.roles);
      expect(on.main).toEqual(without.main);
      expect(off.roles.filter((r) => r.endsWith('=pin')).length).toBe(0);
    }
  });

  it("2. on, no team is laid on the main effort's target; off, the attack's teams are laid with its guns", () => {
    const o = ownTeams(campaign('normandy-1944', 1)).ticket;
    const on = fought(o, true, 60), off = fought(o, false, 60);
    const teams = (f: { units: U[] }): U[] => f.units.filter((u) => u.side === 'att' && u.team);
    const guns = (f: { units: U[] }): U[] => f.units.filter((u) => u.side === 'att' && u.kind === 'art' && !u.team);
    expect(teams(on).length).toBeGreaterThan(0);
    expect(teams(on).every((u) => u.aim === undefined)).toBe(true);
    // (the guns are laid as before, on or off)
    expect(guns(on).length).toBeGreaterThan(0);
    expect(guns(on).every((u) => u.aim !== undefined)).toBe(true);
    expect(teams(off).every((u) => u.aim !== undefined)).toBe(true);
  }, 120_000);

  it('3. on, an army whose only fire is its own teams does not wait under them; off, it waits as under guns', () => {
    const t = mlawa(1), o = ownTeams(t).ticket;
    expect(t.att.forces.some((f) => f.kind === 'art')).toBe(false);
    expect(o.att.forces.some(isOwnTeam)).toBe(true);
    const without = fought(t, true, 200).plan.went, on = fought(o, true, 200).plan.went, off = fought(o, false, 200).plan.went;
    expect(without).not.toBeNull();
    expect(on).toBe(without);
    expect(off!).toBeGreaterThan(without! + 2);
  }, 240_000);
});
