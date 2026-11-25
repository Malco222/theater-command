// Every infantry force with its own machine gun and mortar teams in a campaign battle (release 2, switched, off;
// Brian, 25 November 2026: "Where are the machine gun teams and mortar teams in the infantry squad", and his choice of
// both: every infantry force brings its own, and teams are units he builds). src/battle/land/own-teams.ts: each force
// of infantry, mountain troops or lorried infantry gives a small share of itself to a machine-gun and a mortar
// battalion of its own. Nothing is added to a side: its shares and its men add up to what they did. Their ids are
// their own, so the campaign's bill passes over them. Off, the ticket is the campaign's own, untouched.
//
// And the rule that rides on the same switch (sim.teamsRule; the design director's ruling of 25 November 17:52Z on
// the ground thread's own-teams-causes-1125.md): a team's nerve multiplier weighs its own harm only, where before
// every striker's was added and the sum weighed all the harm on the block that tick. A battle with no team block is
// the same on or off; off, every battle is as it was. (The staff's half of the ruling, ai.ts defend, is the tank and
// soldier AI thread's line and test.)
import { describe, expect, it, vi } from 'vitest';
import { liveTicket } from '../src/battle/land/live';
import { OWN_TEAM_ID, OWN_TEAM_SHARE, isOwnTeam, ownTeams, ownTeamsLook } from '../src/battle/land/own-teams';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { LandBattle } from '../src/battle/land/sim';
import type { FieldTicket, ForceTicket, SideTicket } from '../src/battle/core/types';
import type { LandPlace } from '../src/battle/land/field';

type Ticket = FieldTicket<LandPlace>;

/** A set piece's ticket as a campaign battle's: without the piece's own battalions (pieces.ts withTeams), its shares whole again. */
function campaign(key: string, seed = 11, me: 'att' | 'def' | null = null): Ticket {
  const t = ticketOf(PIECES.find((x) => x.key === key)!, seed, me);
  const side = (s: SideTicket): SideTicket => ({ ...s, forces: s.forces.filter((f) => f.kind !== 'mg' && f.kind !== 'mor').map((f) => ({ ...f, share: f.share / 0.92 })) });
  return { ...t, att: side(t.att), def: side(t.def) };
}

const sum = (fs: ForceTicket[], of: (f: ForceTicket) => number): number => fs.reduce((s, f) => s + of(f), 0);

describe('every infantry force with its own machine gun and mortar teams', () => {
  it('gives each infantry force a machine-gun and a mortar battalion, and no other kind any', () => {
    const t = campaign('normandy-1944');
    const o = ownTeams(t).ticket;
    for (const side of ['att', 'def'] as const) {
      const foot = t[side].forces.filter((f) => ['inf', 'mtn', 'mot'].includes(f.kind));
      expect(foot.length).toBeGreaterThan(0);
      const made = o[side].forces.filter(isOwnTeam);
      expect(made.map((f) => f.kind)).toEqual(foot.flatMap(() => ['mg', 'mor']));
      // the forces the campaign sent are all still there, in their order, with their ids
      expect(o[side].forces.filter((f) => !isOwnTeam(f)).map((f) => f.corps)).toEqual(t[side].forces.map((f) => f.corps));
      // tanks and guns give up nothing
      for (const f of t[side].forces.filter((x) => !['inf', 'mtn', 'mot'].includes(x.kind))) expect(o[side].forces.find((x) => x.corps === f.corps)).toBe(f);
    }
  });

  it('adds nothing to a side: its shares and its men add up to what they did', () => {
    for (const key of PIECES.map((p) => p.key)) {
      const t = campaign(key), o = ownTeams(t).ticket;
      for (const side of ['att', 'def'] as const) {
        expect(sum(o[side].forces, (f) => f.share)).toBeCloseTo(sum(t[side].forces, (f) => f.share), 12);
        // (each force's men are rounded three ways: within two men a force)
        expect(Math.abs(sum(o[side].forces, (f) => f.men) - sum(t[side].forces, (f) => f.men))).toBeLessThanOrEqual(2 * t[side].forces.length);
        for (const f of t[side].forces.filter((x) => x.kind === 'inf')) {
          const now = o[side].forces.find((x) => x.corps === f.corps)!;
          expect(now.share).toBeCloseTo(f.share * (1 - 2 * OWN_TEAM_SHARE), 12);
        }
      }
    }
  });

  it("makes each battalion its parent's own, with an id no corps and no town's guard has", () => {
    const t = campaign('prokhorovka-1943');
    // a dug-in parent, worn, that comes on over dry ground
    t.def.forces = t.def.forces.map((f) => (f.kind === 'inf' ? { ...f, dug: true, wear: 0.7, cross: false } : f));
    const o = ownTeams(t).ticket;
    const made = [...o.att.forces, ...o.def.forces].filter(isOwnTeam);
    expect(new Set(made.map((f) => f.corps)).size).toBe(made.length);
    for (const f of made) {
      expect(f.corps).toBeLessThanOrEqual(OWN_TEAM_ID);
      expect(f.divs).toBe(1);
      expect(f.divisions?.length).toBe(1);
    }
    const parent = t.def.forces.find((f) => f.kind === 'inf')!;
    for (const f of o.def.forces.filter(isOwnTeam)) {
      expect([f.nat, f.str, f.org, f.xp, f.mor, f.dug, f.wear, f.cross]).toEqual([parent.nat, parent.str, parent.org, parent.xp, parent.mor, true, 0.7, false]);
      expect(f.name).toBe(parent.name);
      expect(f.divisions![0].name).toContain(parent.name);
    }
  });

  it("leaves a town's guard, and a side with no infantry, as they were", () => {
    const t = campaign('normandy-1944');
    const guard: ForceTicket = { corps: -8, name: 'Town guard of Caen', nat: 'GER', kind: 'inf', divs: 1, men: 3000, str: 1, org: 0.4, xp: 0.1, share: 0.1, dug: true };
    const tanksOnly: Ticket = { ...t, att: { ...t.att, forces: t.att.forces.filter((f) => f.kind !== 'inf') }, def: { ...t.def, forces: [...t.def.forces.filter((f) => f.kind !== 'inf'), guard] } };
    const o = ownTeams(tanksOnly);
    // (nothing to make: the very ticket comes back)
    expect(o.ticket).toBe(tanksOnly);
    const withGuard: Ticket = { ...t, def: { ...t.def, forces: [...t.def.forces, guard] } };
    const made = ownTeams(withGuard).ticket.def.forces.filter(isOwnTeam);
    expect(made.length).toBe(2 * t.def.forces.filter((f) => f.kind === 'inf').length);
    expect(ownTeams(withGuard).ticket.def.forces.find((f) => f.corps === -8)).toBe(guard);
  });

  it('asked twice makes them once', () => {
    const o = ownTeams(campaign('normandy-1944'));
    expect(ownTeams(o.ticket).ticket).toBe(o.ticket);
  });

  it("shows the other side a battalion where the card knew its parent corps, and only there", () => {
    const t = campaign('normandy-1944');
    const parent = t.def.forces.find((f) => f.kind === 'inf')!.corps;
    const known = ownTeams(t, [parent]);
    const theirs = known.ticket.def.forces.filter(isOwnTeam).map((f) => f.corps);
    expect(known.seen).toEqual([parent, ...theirs]);
    expect(ownTeams(t, []).seen).toEqual([]);
    // the campaign's answer as it stands, in the shape it came
    const look = ownTeamsLook({ ticket: t, live: { fog: true, seen: [parent], designYears: { GER: 1943 } } });
    expect('ticket' in look && look.live).toEqual({ fog: true, seen: [parent, ...theirs], designYears: { GER: 1943 } });
    expect('ticket' in look && look.ticket.def.forces.filter(isOwnTeam).length).toBe(2);
    // a bare ticket comes back a bare ticket
    const bare = ownTeamsLook(t);
    expect('ticket' in bare).toBe(false);
    expect((bare as Ticket).att.forces.some(isOwnTeam)).toBe(true);
  });

  it('puts them on the field as team blocks behind their side, and the battle is fought to its end', () => {
    const t = campaign('normandy-1944');
    const o = ownTeams(t).ticket;
    const ids = new Set([...o.att.forces, ...o.def.forces].filter(isOwnTeam).map((f) => f.corps));
    const L = liveTicket(o, { me: null });
    const blocks = (L.header() as unknown as { blocks: { corps: number; kind: string; team?: string; side: string; figures0: number }[] }).blocks;
    const teams = blocks.filter((b) => ids.has(b.corps));
    expect(teams.length).toBe(ids.size);
    for (const b of teams) { expect(b.kind).toBe('art'); expect(['mg', 'mor']).toContain(b.team); expect(b.figures0).toBeGreaterThan(0); }
    // every other block is one the campaign's own ticket makes (the line is cut as it would be without them)
    const plain = (L0: ReturnType<typeof liveTicket>): string[] => (L0.header() as unknown as { blocks: { corps: number; kind: string; name: string }[] }).blocks.filter((b) => !ids.has(b.corps)).map((b) => `${b.corps} ${b.kind} ${b.name}`);
    expect(plain(L)).toEqual(plain(liveTicket(t, { me: null })));
    // fought by the rules alone, to its end: the result names the battalions among its forces beside every corps the
    // campaign sent, and each side's share lost is a share
    const sim = new LandBattle(o);
    let guard = 0;
    while (!sim.over && guard++ < 2_000_000) sim.step();
    const over = sim.over!;
    expect(over).toBeTruthy();
    const named = new Set((over.forces ?? []).map((f) => f.corps));
    for (const f of [...t.att.forces, ...t.def.forces]) expect(named.has(f.corps)).toBe(true);
    for (const id of ids) expect(named.has(id)).toBe(true);
    for (const s of ['att', 'def'] as const) { expect(over.lost[s]).toBeGreaterThanOrEqual(0); expect(over.lost[s]).toBeLessThanOrEqual(1); }
  }, 120_000);
});

describe("support teams in battle: a team's fire shakes by its own harm (sim.teamsRule)", () => {
  type Blow = { dmg: number; nerve: number };
  type U = { id: number; team?: string };
  const fought = (t: Ticket, rule: boolean): { R: number; t: number; lost: Record<'att' | 'def', number>; winner: string } => {
    const sim = new LandBattle(t);
    sim.teamsRule = rule;
    let guard = 0;
    while (!sim.over && guard++ < 2_000_000) sim.step();
    return { R: sim.over!.R, t: sim.t, lost: sim.over!.lost, winner: sim.over!.winner };
  };

  it('is off unless the bridge turns it on, for a set piece and a campaign battle alike', () => {
    const t = campaign('normandy-1944');
    expect(new LandBattle(t).teamsRule).toBe(false);
    expect(liveTicket(t, { me: null }).sim.teamsRule).toBe(false);
    expect(liveTicket(t, { me: null, teams: true }).sim.teamsRule).toBe(true);
    expect(liveTicket(ticketOf(PIECES[0], 11, null), { me: null, teams: true }, PIECES[0].key).sim.teamsRule).toBe(true);
  });

  it('changes nothing in a battle with no team block on the field', () => {
    const t = campaign('prokhorovka-1943', 7);
    expect([...t.att.forces, ...t.def.forces].some((f) => f.kind === 'mg' || f.kind === 'mor')).toBe(false);
    expect(fought(t, true)).toEqual(fought(t, false));
  }, 120_000);

  it("weighs each team's multiplier by its own harm, the line's and the guns' added as they were", () => {
    for (const rule of [false, true]) {
      // (both are private: reached through the prototype, as the fight calls them)
      const proto = LandBattle.prototype as unknown as { blow: (u: U, t: U) => Blow; suffer: (t: U, dmg: number, mul: number, from?: [U, number][], blind?: boolean, bare?: boolean) => void };
      const blow = vi.spyOn(proto, 'blow'), suffer = vi.spyOn(proto, 'suffer');
      try {
        const sim = new LandBattle(ownTeams(campaign('normandy-1944', 3)).ticket);
        sim.teamsRule = rule;
        let withTeam = 0, mixed = 0, guard = 0;
        while (!sim.over && mixed < 300 && guard++ < 400_000) {
          const b0 = blow.mock.calls.length, s0 = suffer.mock.calls.length;
          sim.step();
          // this tick's blows, by who struck whom: the nerve multiplier each carried
          const nerve = new Map<string, number>();
          for (let i = b0; i < blow.mock.calls.length; i++) {
            const [u, t] = blow.mock.calls[i];
            nerve.set(`${u.id}>${t.id}`, (blow.mock.results[i].value as Blow).nerve);
          }
          for (let i = s0; i < suffer.mock.calls.length; i++) {
            const [t, dmg, mul, from, , bare] = suffer.mock.calls[i];
            // (the tick's blows only: an air strike's harm and kept harm landing name no strikers, or are 'bare')
            if (!from?.length || bare || !from.every(([u]) => nerve.has(`${u.id}>${t.id}`))) continue;
            const of = (u: U): number => nerve.get(`${u.id}>${t.id}`)!;
            const all = from.reduce((a, [u]) => a + of(u), 0);
            const teams = from.filter(([u]) => u.team), others = from.filter(([u]) => !u.team);
            if (!teams.length) { expect(mul).toBeCloseTo(all, 9); continue; }
            withTeam++;
            if (others.length) mixed++;
            const teamD = teams.reduce((a, [, d]) => a + d, 0), teamDN = teams.reduce((a, [u, d]) => a + d * of(u), 0);
            const othersSum = others.reduce((a, [u]) => a + of(u), 0);
            const want = rule && dmg > 0 ? ((dmg - teamD) * othersSum + teamDN) / dmg : all;
            expect(mul).toBeCloseTo(want, 9);
            // on, a team never makes the others' harm cost more nerve than the others' own sum would
            if (rule && dmg > 0 && others.length) expect(mul).toBeLessThan(all);
          }
        }
        // (the battle did put teams and the line on the same block in the same tick, or this proves nothing)
        expect(withTeam).toBeGreaterThanOrEqual(mixed);
        expect(mixed).toBeGreaterThanOrEqual(300);
      } finally { blow.mockRestore(); suffer.mockRestore(); }
    }
  }, 120_000);

  it('is read: with teams on the field, on and off are different battles', () => {
    const o = ownTeams(campaign('normandy-1944', 3)).ticket;
    expect(fought(o, true)).not.toEqual(fought(o, false));
  }, 120_000);
});

// The staff's half of the ruling (ai.ts defend; the tank and soldier AI thread's line): a defence's line block steps
// out "to take one that has come round a neighbour's flank". A block's 'engaged' is its target, a team's too, from as
// far back as it reaches; so before the mend a team block firing on an enemy within the leash of a line block's place
// drew that block out of it, though no neighbour of the line was in a fight. Silent team blocks alone (no harm done)
// gave the attack 16 battles in 200 at Prokhorovka that way.
describe("support teams in battle: a team firing from far back is no neighbour in a fight (ai.ts defend)", () => {
  type U = { id: number; side: string; kind: string; team?: string; state: string; engaged?: number; reserve?: string };
  /** The defence given silent team blocks (they stand and aim, and do no harm), on its full line. */
  const silent = (seed: number): Ticket => {
    const b = campaign('prokhorovka-1943', seed), o = ownTeams(b).ticket;
    return { ...b, def: { ...b.def, forces: [...b.def.forces, ...o.def.forces.filter(isOwnTeam).map((f) => ({ ...f, share: f.share * 1e-6 }))] } };
  };
  /**
   * The defence's staff orders that send a line block (no reserve) at an enemy that is not wavering: how many, and how
   * many of them at an enemy only team blocks of its side had as their target.
   */
  const drawnOut = (seed: number, rule: boolean): { orders: number; byTeamsAlone: number } => {
    const sim = new LandBattle(silent(seed));
    sim.teamsRule = rule;
    const units = sim.units as unknown as U[];
    const order = sim.command.bind(sim);
    let orders = 0, byTeamsAlone = 0;
    sim.command = ((c: { do: string; units?: number[]; target?: number }, side: string, ...rest: unknown[]) => {
      if (side === 'def' && c.do === 'attack' && c.units?.length === 1) {
        const u = units.find((x) => x.id === c.units![0])!, f = units.find((x) => x.id === c.target)!;
        if (!u.reserve && f.state !== 'wavering') {
          const on = units.filter((m) => m.side === 'def' && m.id !== u.id && m.engaged === f.id);
          orders++;
          if (on.length && on.every((m) => m.team)) byTeamsAlone++;
        }
      }
      return (order as (...a: unknown[]) => unknown)(c, side, ...rest);
    }) as typeof sim.command;
    let guard = 0;
    while (!sim.over && guard++ < 2_000_000) sim.step();
    return { orders, byTeamsAlone };
  };

  it('on, no line block is drawn out by a team alone; off, it is as it was', () => {
    let on = 0, off = 0, orders = 0;
    for (let seed = 1; seed <= 12; seed++) {
      const a = drawnOut(seed, true), b = drawnOut(seed, false);
      on += a.byTeamsAlone; off += b.byTeamsAlone; orders += a.orders;
    }
    // (the old behaviour is there to be mended, and the mended staff still sends its line out for a neighbour)
    expect(off).toBeGreaterThan(0);
    expect(orders).toBeGreaterThan(0);
    expect(on).toBe(0);
  }, 240_000);
});
