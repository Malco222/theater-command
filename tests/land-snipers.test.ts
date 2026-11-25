// Sniper teams (release 2; Brian, 25 November 2026, 16:07Z: "should we add snipers? how would that impact the
// battle?", then "yes"; LAND.snipers, withSnipers, the design form snipers/form.md and its ruling). These tests hold that
// the teams are carved from each side's own foot (the card's odds stand), by doctrine and never by the year; that with
// the switch off nothing of theirs is ever set; that they hide in a town's houses, fire from hiding beyond any ambush's
// range and are found in time; that what they do to a block is a capped pin and nerve, never harm to a tank; that a team
// never routs and slips away at most twice; and that the men drawn are sniper and spotter pairs whose rounds are aimed
// singly, never two of a team at once, whose kills are shots, never shells, and whose hidden shots show no flash, nor
// any dispatch where the shooter lies.
import { describe, expect, it } from 'vitest';
import groundCard from '../data/campaign/ground.json';
import { LAND, LandBattle, PRACTICE_GROUNDS, partsFor, practiceTicket, sniperShare, withSnipers, withTeams } from '../src/battle/land';
import { LiveBattle } from '../src/battle/land/live';
import type { FieldTicket } from '../src/battle/core/types';
import type { LandPlace } from '../src/battle/land';

const ground = (terrain: string) => PRACTICE_GROUNDS.find((p) => p.terrain === terrain)!;
const ticket = (terrain = 'urban', seed = 3, att = 'USA', def = 'GER'): FieldTicket<LandPlace> => withTeams(practiceTicket(ground(terrain), { seed, nat: { att, def } }));
const sum = (t: FieldTicket<LandPlace>, s: 'att' | 'def'): number => t[s].forces.reduce((a, f) => a + f.share, 0);

describe('sniper teams: the ticket', () => {
  it('carves the teams out of each side\'s own foot, so each side\'s shares still add up as before', () => {
    const t0 = ticket(), t1 = withSnipers(t0);
    for (const s of ['att', 'def'] as const) {
      expect(sum(t1, s)).toBeCloseTo(sum(t0, s), 9);
      const snp = t1[s].forces.filter((f) => f.kind === 'snp');
      expect(snp.length).toBeGreaterThanOrEqual(1);
      expect(snp.length).toBeLessThanOrEqual(LAND.snipers.most);
      // Each team carries a corps its side brought, so its losses are billed to that corps.
      for (const f of snp) expect(t0[s].forces.some((g) => g.corps === f.corps && g.kind !== 'snp')).toBe(true);
    }
  });

  it('carves only from the card\'s foot, pro rata, one team for every four foot blocks (one to three)', () => {
    expect([...LAND.snipers.foot]).toEqual(groundCard.arms.foot);
    const t0 = ticket(), t1 = withSnipers(t0);
    for (const s of ['att', 'def'] as const) {
      const before = t0[s].forces, after = t1[s].forces;
      before.forEach((f, i) => {
        const g = after[i];
        if (LAND.snipers.foot.includes(f.kind)) expect(g.share).toBeCloseTo(f.share * (1 - sniperShare(f.nat)), 12);
        else expect(g.share).toBe(f.share);
      });
      const parts = partsFor(before), blocks = before.reduce((a, f, i) => a + (LAND.snipers.foot.includes(f.kind) ? parts[i].length : 0), 0);
      expect(after.filter((f) => f.kind === 'snp').length).toBe(Math.max(1, Math.min(LAND.snipers.most, Math.ceil(blocks / LAND.snipers.per))));
    }
  });

  it('gives kin their senior partner\'s share', () => {
    for (const k of ['CAN', 'AUS', 'NZL', 'NZ', 'SAF', 'IND']) expect(sniperShare(k)).toBe(sniperShare('UK'));
    expect(sniperShare('VIC')).toBe(sniperShare('FRA'));
    expect(sniperShare('XYZ')).toBe(LAND.snipers.share._);
  });

  it('makes no team where hiding is off', () => {
    const live = new LiveBattle(ticket(), null, { snipers: true, hide: false });
    expect(live.sim.units.some((u) => u.team === 'snp')).toBe(false);
    const on = new LiveBattle(ticket(), null, { snipers: true });
    expect(on.sim.units.some((u) => u.team === 'snp')).toBe(true);
  });

  it('gives an army with a sniper doctrine more of its foot than one without, whatever the year', () => {
    const share = (nat: string, turn?: number): number => {
      const t0 = ticket('urban', 3, nat, 'GER'), t1 = withSnipers(turn === undefined ? t0 : { ...t0, turn });
      return t1.att.forces.filter((f) => f.kind === 'snp').reduce((a, f) => a + f.share, 0);
    };
    expect(share('SOV')).toBeGreaterThan(share('USA'));
    // Never the calendar: the same army gives the same share in any month.
    expect(share('SOV', 1)).toBeCloseTo(share('SOV', 60), 12);
  });
});

describe('sniper teams: the player\'s orders', () => {
  it('in a group attack a team takes no lane and aims at the target from its cover; sent alone it goes', () => {
    const t = withSnipers(ticket('plains', 5));
    const b = new LandBattle({ ...t, att: { ...t.att, human: true } });
    const mine = b.units.filter((u) => u.side === 'att' && u.kind !== 'fort');
    const team = mine.find((u) => u.team === 'snp')!;
    const foe = b.units.find((u) => u.side === 'def' && u.team !== 'snp')!;
    const at = [team.x, team.y];
    expect(b.command({ do: 'attack', units: mine.map((u) => u.id), target: foe.id }, 'att')).toBe(true);
    expect(team.aim).toBe(foe.id);
    expect(team.order).toBeNull();
    expect(mine.filter((u) => u.team !== 'snp').every((u) => u.order?.kind === 'attack')).toBe(true);
    for (let k = 0; k < 50; k++) b.step();
    expect(Math.hypot(team.x - at[0], team.y - at[1])).toBeLessThan(1);
    expect(b.command({ do: 'attack', units: [team.id], target: foe.id }, 'att')).toBe(true);
    expect(team.order?.kind).toBe('attack');
    // Sniper teams sent on their own, together, go as ordered too.
    const teams = mine.filter((u) => u.team === 'snp');
    expect(teams.length).toBeGreaterThan(1);
    expect(b.command({ do: 'attack', units: teams.map((u) => u.id), target: foe.id }, 'att')).toBe(true);
    expect(teams.every((u) => u.order?.kind === 'attack')).toBe(true);
  });
});

describe('sniper teams: the switch off', () => {
  it('sets nothing of theirs on any block when the field has none', () => {
    const b = new LandBattle(ticket('urban', 4));
    b.hideOn();
    b.drillOn({ sections: true, platoons: true, hatches: true });
    let set = 0;
    for (let k = 0; !b.over && k < 60000; k++) {
      b.step();
      for (const u of b.units) {
        if (u.team === 'snp' || u.closedTo !== undefined || u.pinTo !== undefined || u.pinSpent !== undefined || u.leadTo !== undefined) set++;
        if (u.quietTo !== undefined || u.slipTo !== undefined || u.slips !== undefined || u.hunts !== undefined) set++;
      }
    }
    expect(b.over).toBeTruthy();
    expect(set).toBe(0);
  }, 180_000);
});

describe('sniper teams: on the field', () => {
  function fight(terrain = 'urban', seed = 3) {
    const b = new LandBattle(withSnipers(ticket(terrain, seed)));
    b.hideOn();
    const d = b.drillOn({ sections: true, platoons: true, hatches: true });
    const snp = b.units.filter((u) => u.team === 'snp');
    const out = { startHid: 0, farFromHiding: 0, found: 0, tanksHarmed: 0, sniped: 0, rounds: [] as ReturnType<typeof d.rounds>, spotterNear: 0, spotterSeen: 0, routed: 0, pinMost: 0, slipsMost: 0, hidShots: 0 };
    const was = new Map<number, boolean>();
    let seen = 0;
    for (let k = 0; !b.over && k < 60000; k++) {
      const tankStr = new Map(snp.some((u) => u.engaged !== undefined && b.unit(u.engaged)?.kind === 'arm') ? b.units.filter((u) => u.kind === 'arm').map((u) => [u.id, u.str]) : []);
      b.step();
      if (k === 0) out.startHid = snp.filter((u) => u.hid).length;
      for (const u of snp) {
        if (u.hid && u.engaged !== undefined) { const e = b.unit(u.engaged)!; if (b.gap(u, e) > LAND.hide.ambush.foot) out.farFromHiding++; }
        if (was.get(u.id) && !u.hid) out.found++;
        if (u.state === 'routing' || u.state === 'gone') out.routed++;
        out.slipsMost = Math.max(out.slipsMost, u.slips ?? 0);
        was.set(u.id, !!u.hid);
        // A tank it fires on loses nothing to it (bullets do not hurt armour): none harmed while it alone is on her.
        if (u.engaged !== undefined) {
          const e = b.unit(u.engaged)!;
          if (e.kind === 'arm' && !b.units.some((f) => f !== u && f.side === u.side && f.engaged === e.id) && e.str < (tankStr.get(e.id) ?? e.str)) out.tanksHarmed++;
        }
      }
      for (const u of b.units) out.pinMost = Math.max(out.pinMost, u.pinSpent ?? 0);
      for (const r of d.rounds()) if (r.n > seen) { seen = r.n; if (r.sn) { out.rounds.push(r); if (r.hd) out.hidShots++; } }
      if (k % 20 === 0) for (const blk of d.view()) if (snp.some((u) => u.id === blk.id)) {
        const shooters = blk.men.filter((m) => m.sn === 1 && m.d === undefined);
        for (const m of blk.men) if (m.sn === 2 && m.d === undefined && m.p === 'observe') {
          out.spotterSeen++;
          if (shooters.some((s) => Math.hypot(s.x - m.x, s.y - m.y) < 6)) out.spotterNear++;
        }
      }
    }
    return { b, out };
  }

  let fought: ReturnType<typeof fight> | null = null;
  const once = (): ReturnType<typeof fight> => (fought ??= fight('urban', 3));

  it('hides its teams in a town\'s houses and has them fire from hiding beyond an ambush\'s range, found in time', () => {
    const { out } = once();
    expect(out.startHid).toBeGreaterThanOrEqual(2);
    expect(out.farFromHiding).toBeGreaterThan(0);
    expect(out.found).toBeGreaterThan(0);
    expect(out.tanksHarmed).toBe(0);
  }, 240_000);

  it('never routs a team, slows a block at most LAND.pin.most seconds, and slips away at most twice', () => {
    const { out } = once();
    expect(out.routed).toBe(0);
    expect(out.pinMost).toBeGreaterThan(0);
    expect(out.pinMost).toBeLessThanOrEqual(LAND.pin.most + 1e-9);
    expect(out.slipsMost).toBeLessThanOrEqual(LAND.snipers.slip.most);
  }, 240_000);

  it('never places a hidden enemy sniper in a dispatch: his first shot is told where it lands', () => {
    const t = withSnipers(ticket('urban', 3));
    const b = new LandBattle({ ...t, att: { ...t.att, human: true } });
    b.ai.att = b.ai.def = true;
    b.hideOn();
    const theirs = new Set(b.units.filter((u) => u.team === 'snp' && u.side === 'def').map((u) => u.id));
    let placed = 0, firedHidden = 0, told = 0;
    for (let k = 0; !b.over && k < 60000; k++) {
      b.step();
      for (const id of theirs) { const u = b.unit(id)!; if (u.hid && u.engaged !== undefined) firedHidden++; }
      for (const e of b.view('att').events) {
        for (const id of [e.unit, e.by]) if (id !== undefined && theirs.has(id) && b.unit(id)!.hid) placed++;
        if (e.what === 'contact' && e.unit === undefined && /troops are attacking your/.test(e.text)) told++;
      }
    }
    expect(firedHidden).toBeGreaterThan(0);
    expect(told).toBeGreaterThan(0);
    expect(placed).toBe(0);
  }, 240_000);

  it('draws sniper and spotter pairs: single aimed rifle rounds, kills that are shots and never shells, the spotter on his glasses beside him', () => {
    const { out } = once();
    expect(out.rounds.length).toBeGreaterThan(10);
    for (const r of out.rounds) expect(r.w).toBe('rifle');
    // Never two of a team on one tick (the kills are the rules', booked as they fall); a hidden shooter shows no flash.
    const fired = out.rounds.filter((r) => r.r !== 'kill').map((r) => `${r.by}:${r.t}`);
    expect(new Set(fired).size).toBe(fired.length);
    expect(out.hidShots).toBeGreaterThan(0);
    expect(out.rounds.filter((r) => r.r === 'kill').length).toBeGreaterThan(0);
    expect(out.spotterSeen).toBeGreaterThan(0);
    expect(out.spotterNear / out.spotterSeen).toBeGreaterThan(0.9);
  }, 240_000);
});
