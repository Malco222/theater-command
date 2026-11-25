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
import { coverAt } from '../src/battle/land/field';
import { isOwnTeam, ownTeams } from '../src/battle/land/own-teams';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { LAND } from '../src/battle/land/rules';
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

// The rule reads only a machine-gun or a mortar team (the sim's ownArm, 1dea1eee; ai.ts's own ownArm): a team of any
// other arm is laid, waited under and counted a neighbour in a fight as it is with the rule off. Each test makes every
// team block of a battle the other arm and asks that on is off, at the staff's three lines that read `team` (the aim
// and the wait in planned, the neighbour in defend).
// The other arm is a made-up one that no branch has (OTHER), lent a machine gun's figures for the test: it tests ownArm
// and nothing else. A sniper pair ('snp', release 2) is such an arm, but where its own rules are joined its aim and its
// lane are the sniper thread's (a pair sent with other blocks takes no lane, and its target is its aim only by order),
// so a stand-in named 'snp' tested those rules there, not this one (the assembler, 25 November 21:14Z).
describe('support teams in battle: the staff reads only a machine-gun or a mortar team (ai.ts)', () => {
  type V = U & { state: string; str: number; x: number; y: number; engaged?: number; reserve?: string };
  /** The battle with every team block made the arm asked for, fought to a second: its blocks, and when the attack went in. */
  const armed = (t: Ticket, rule: boolean, arm: string, until: number): { units: V[]; went: number | null; state: string } => {
    const sim = new LandBattle(t);
    sim.teamsRule = rule;
    const units = sim.units as unknown as V[];
    for (const u of units) if (u.team) u.team = arm;
    let guard = 0;
    while (sim.t < until && !sim.over && guard++ < 2_000_000) sim.step();
    const plan = (sim as unknown as { plan: { att?: Plan } }).plan.att;
    return { units, went: plan ? plan.went : null, state: JSON.stringify(units.map((u) => [u.id, u.state, +u.str.toFixed(6), +u.x.toFixed(3), +u.y.toFixed(3), u.aim ?? -1])) };
  };
  /** The made-up arm: no branch's rules name it. */
  const OTHER = 'xx';
  /** The made-up arm's figures for the length of a test: a machine gun's, taken away after it. */
  const lent = <R>(run: () => R): R => {
    const teams = LAND.teams as unknown as Record<string, unknown>;
    expect(OTHER in teams).toBe(false);
    teams[OTHER] = { ...LAND.teams.mg };
    try { return run(); } finally { delete teams[OTHER]; }
  };

  it("4a. the aim: teams of another arm are laid on the main effort's target with the guns, on as off", () => lent(() => {
    const o = ownTeams(campaign('normandy-1944', 1)).ticket;
    const on = armed(o, true, OTHER, 60), off = armed(o, false, OTHER, 60);
    const others = on.units.filter((u) => u.side === 'att' && u.team === OTHER);
    expect(others.length).toBeGreaterThan(0);
    expect(others.every((u) => u.aim !== undefined)).toBe(true);
    expect(on.state).toBe(off.state);
    // (and a machine-gun or a mortar team is still never laid: test 2)
    expect(armed(o, true, 'mg', 60).units.filter((u) => u.side === 'att' && u.team).every((u) => u.aim === undefined)).toBe(true);
  }), 240_000);

  it('4b. the wait: an army whose only fire is teams of another arm waits under them, on as off', () => lent(() => {
    const t = mlawa(1), o = ownTeams(t).ticket;
    const without = fought(t, true, 200).plan.went;
    const on = armed(o, true, OTHER, 200), off = armed(o, false, OTHER, 200);
    expect(on.went).not.toBeNull();
    expect(on.went).toBe(off.went);
    expect(on.went!).toBeGreaterThan(without! + 2);
    expect(on.state).toBe(off.state);
  }), 240_000);

  it('4c. the neighbour: a line block is drawn out by a team of another arm alone as often on as off', () => lent(() => {
    /** The defence given silent team blocks of the arm asked for (they stand and aim, and do no harm), on its full line. */
    const silent = (seed: number): Ticket => {
      const b = campaign('prokhorovka-1943', seed), o = ownTeams(b).ticket;
      return { ...b, def: { ...b.def, forces: [...b.def.forces, ...o.def.forces.filter(isOwnTeam).map((f) => ({ ...f, share: f.share * 1e-6 }))] } };
    };
    const drawnOut = (seed: number, rule: boolean, arm: string): number => {
      const sim = new LandBattle(silent(seed));
      sim.teamsRule = rule;
      const units = sim.units as unknown as V[];
      for (const u of units) if (u.team) u.team = arm;
      const order = sim.command.bind(sim);
      let byTeamsAlone = 0;
      sim.command = ((c: { do: string; units?: number[]; target?: number }, side: string, ...rest: unknown[]) => {
        if (side === 'def' && c.do === 'attack' && c.units?.length === 1) {
          const u = units.find((x) => x.id === c.units![0])!, f = units.find((x) => x.id === c.target)!;
          if (!u.reserve && f.state !== 'wavering') {
            const at = units.filter((m) => m.side === 'def' && m.id !== u.id && m.engaged === f.id);
            if (at.length && at.every((m) => m.team)) byTeamsAlone++;
          }
        }
        return (order as (...a: unknown[]) => unknown)(c, side, ...rest);
      }) as typeof sim.command;
      let guard = 0;
      while (!sim.over && guard++ < 2_000_000) sim.step();
      return byTeamsAlone;
    };
    let on = 0, off = 0, own = 0;
    for (let seed = 1; seed <= 12; seed++) { on += drawnOut(seed, true, OTHER); off += drawnOut(seed, false, OTHER); own += drawnOut(seed, true, 'mg'); }
    expect(off).toBeGreaterThan(0);
    expect(on).toBe(off);
    // (and a machine-gun or a mortar team alone still draws nobody out: mend T)
    expect(own).toBe(0);
  }), 480_000);
});

// Two made-up fields for the two sums of the design director's 20:22Z bound ("every sum the staff makes of a force
// reads it with its teams") that no real force measured moves (the tank and soldier AI thread, 20:30Z: 200 plans the
// same with and without them). Every infantry force gives its teams the same 8%, so these sums only turn where a
// force with teams is weighed against one without: here the fields are made so that it is.
describe("support teams in battle: every sum the staff makes of a force reads it with its teams (made-up fields)", () => {
  it("5. the enemy's line: on, a division is weighed with its own teams, so the stretch chosen is the one chosen against it without them", () => {
    type B = U & { x: number; y: number; power0: number; dug?: boolean; reserve?: string };
    type Sim = { units: B[]; field: Parameters<typeof coverAt>[0]; ownTeamsPower(u: B): number; plan: { att?: Plan } };
    const t = campaign('normandy-1944', 1), o = ownTeams(t).ticket;
    // The defence's line made two blocks, an end each and neither dug in: one of an infantry division (X) and one of
    // the tanks (Y, who bring no teams). Y is made to weigh, for its ground, between X without its teams' 8% and X whole.
    const main = (ticket: Ticket, rule: boolean): string => {
      const sim = new LandBattle(ticket);
      sim.teamsRule = rule;
      const b = sim as unknown as Sim, line = b.units.filter((u) => u.side === 'def' && !u.team && u.kind !== 'art');
      const X = line.filter((u) => u.kind === 'inf')[1], Y = line.filter((u) => u.kind === 'arm')[1];
      for (const u of line) { u.dug = false; if (u !== X && u !== Y) u.reserve = 'held'; }
      const cover = (u: B): number => LAND.cover[coverAt(b.field, u.x, u.y)];
      // (X whole is its own weight and its share of its division's teams: the same figure on either ticket)
      const whole = X.power0 + b.ownTeamsPower(X);
      Y.power0 = (cover(Y) / cover(X)) * whole * 0.96;
      let guard = 0;
      while (sim.t < 1 && guard++ < 100_000) sim.step();
      const k = b.plan.att!.main[0];
      return k === X.id ? 'the infantry' : k === Y.id ? 'the tanks' : 'another';
    };
    // Without teams X is whole, the heavier for its ground: the main effort goes at the tanks.
    expect(main(t, true)).toBe('the tanks');
    // With its own teams and the rule on, the same. Off, X is read 8% light and the main effort goes at it instead.
    expect(main(o, true)).toBe('the tanks');
    expect(main(o, false)).toBe('the infantry');
  });

  it('6. the start line: on, the three quarters that have come up are weighed with their own teams; off, without them', () => {
    type B = U & { x: number; y: number; corps: number; power0: number; order?: unknown; path: unknown[] };
    type Sim = { units: B[]; ownLine(u: B): B[]; ownTeamsPower(u: B): number; plan: { att?: Plan & { spot: Record<number, [number, number]>; arrived: number | null } } };
    const o = ownTeams(mlawa(1)).ticket;
    const arrived = (rule: boolean): boolean => {
      const sim = new LandBattle(o);
      sim.teamsRule = rule;
      let guard = 0;
      while (sim.t < 1 && guard++ < 100_000) sim.step();
      const b = sim as unknown as Sim, P = b.plan.att!;
      expect(P.arrived).toBeNull();
      // The main effort's foot is of several divisions. The one with most blocks in it is made a division that brought
      // no teams and is kept back; the others' blocks are stood on their start line.
      const foot = b.units.filter((u) => u.side === 'att' && !u.team && P.role[u.id] === 'main');
      const count = (c: number): number => foot.filter((u) => u.corps === c).length;
      const most = [...new Set(foot.map((u) => u.corps))].sort((x, y) => count(y) - count(x) || x - y)[0];
      const back = foot.filter((u) => u.corps === most), up = foot.filter((u) => u.corps !== most);
      expect(back.length).toBeGreaterThan(0);
      expect(up.length).toBeGreaterThan(0);
      for (const g of b.units) if (g.team && b.ownLine(g).includes(back[0])) g.power0 = 0;
      expect(b.ownTeamsPower(back[0])).toBe(0);
      const own = up.reduce((a, u) => a + u.power0, 0), teams = up.reduce((a, u) => a + b.ownTeamsPower(u), 0);
      expect(teams).toBeGreaterThan(0);
      // Those kept back weigh a third of (the blocks come up and half their teams): three quarters of the whole have
      // come up only if the teams of those come up are counted with them.
      for (const u of back) { u.power0 = (own + teams / 2) / 3 / back.length; u.y -= 300; }
      for (const u of up) { [u.x, u.y] = P.spot[u.id]; u.order = null; u.path = []; }
      const until = sim.t + 6;
      while (sim.t < until && guard++ < 200_000) sim.step();
      return P.arrived !== null;
    };
    expect(arrived(true)).toBe(true);
    expect(arrived(false)).toBe(false);
  });
});
