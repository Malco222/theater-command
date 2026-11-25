// Every infantry force with its own machine gun and mortar teams in a campaign battle (release 2, switched, off;
// Brian, 25 November 2026: "Where are the machine gun teams and mortar teams in the infantry squad", and his choice of
// both: every infantry force brings its own, and teams are units he builds). src/battle/land/own-teams.ts: each force
// of infantry, mountain troops or lorried infantry gives a small share of itself to a machine-gun and a mortar
// battalion of its own. Nothing is added to a side: its shares and its men add up to what they did. Their ids are
// their own, so the campaign's bill passes over them. Off, the ticket is the campaign's own, untouched.
import { describe, expect, it } from 'vitest';
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
