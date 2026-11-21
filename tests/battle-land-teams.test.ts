// Support teams you place before the battle (release 2; Brian, 21 November 2026, 03:49Z: "are there mortar, artillery,
// and machine gun teams? i want these to be units you can place at the begining of a battle on your side and they can
// then attack from the back of the battle"; 03:57Z: "separate things to build outside of a normal infantry squad").
// The field's part: a machine-gun team and a mortar team are blocks of their own that fire from the rear, and a setup
// step, off unless asked, lets the player place his guns and teams before the clock runs.
import { describe, expect, it } from 'vitest';
import { LAND, LandBattle, PRACTICE_GROUNDS, practiceTicket, reachOf, sampleTicket, withTeams, type LandUnit } from '../src/battle/land';
import type { FieldEvent } from '../src/battle/core';

type Ground = Parameters<typeof sampleTicket>[0];
const ticket = (g: Ground, odds: number, seed: number, teams = true) => {
  const t = sampleTicket(g, odds, { seed, style: { att: 'methodical', def: 'bold' } });
  return teams ? withTeams(t) : t;
};
const end = (b: LandBattle, each?: (b: LandBattle) => void): FieldEvent[] => {
  const ev: FieldEvent[] = [];
  for (let k = 0; !b.over && k < 30000; k++) { b.step(); ev.push(...b.view().events); each?.(b); }
  return ev;
};
const sig = (b: LandBattle): string => JSON.stringify({ over: b.over, t: b.t, units: b.units.map((u) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.figures]) });
const teamsOf = (b: LandBattle): LandUnit[] => b.units.filter((u) => u.team);

describe('support teams are blocks of their own (release 2)', () => {
  it('a ticket force of kind mg or mor makes one team block: a gun block with its team, never cut into regiments', () => {
    const b = new LandBattle(ticket('plains', 1.3, 5));
    const ts = teamsOf(b);
    expect(ts.map((u) => [u.side, u.team])).toEqual([['att', 'mg'], ['att', 'mor'], ['def', 'mg'], ['def', 'mor']]);
    for (const u of ts) {
      expect(u.kind).toBe('art');
      expect([u.w, u.d]).toEqual(LAND.teams[u.team!].size);
      expect(u.tray).toBe(LAND.teams[u.team!].figures);
      expect(u.name).toMatch(/^1st (Machine-Gun|Mortar) Battalion$/);
    }
    // Each side shares out exactly the power the card counted, teams and all.
    const A = b.ticket.reckoning.A, power = { att: A, def: A / b.ticket.reckoning.odds };
    for (const s of ['att', 'def'] as const) expect(b.units.filter((u) => u.side === s).reduce((a, u) => a + u.power0, 0)).toBeCloseTo(power[s], 9);
    // The mortar throws mortar bombs; the machine guns throw none.
    expect(ts.filter((u) => u.team === 'mor').every((u) => u.cal === 'mortar')).toBe(true);
    expect(ts.filter((u) => u.team === 'mg').every((u) => u.cal === undefined)).toBe(true);
  });

  it('teams cut no division into regiments it would not have been: the line is drawn as without them', () => {
    const a = new LandBattle(ticket('forest', 1.2, 9, false)), b = new LandBattle(ticket('forest', 1.2, 9));
    const line = (x: LandBattle): string[] => x.units.filter((u) => !u.team).map((u) => `${u.side} ${u.name}`);
    expect(line(b)).toEqual(line(a));
  });

  it('a battle with no teams and no setup step is exactly as before: nothing new runs', () => {
    // The teams' code paths fire only on a block with a team; the battle-shells digests pin whole battles besides.
    const a = new LandBattle(ticket('hills', 1.3, 17, false)), b = new LandBattle(ticket('hills', 1.3, 17, false));
    end(a); end(b);
    expect(sig(b)).toBe(sig(a));
    expect(teamsOf(a)).toEqual([]);
  });

  it('they fire from the back: each team strikes from beyond the line, within its own reach, and the machine guns land no shells', () => {
    const struck = new Map<string, number>(), farthest = new Map<string, number>();
    let shellsFromMg = 0;
    for (const [g, seed] of [['plains', 3], ['hills', 8], ['forest', 21]] as [Ground, number][]) {
      const b = new LandBattle(ticket(g, 1.25, seed));
      const ids = new Set(teamsOf(b).map((u) => u.id));
      // Every shell the field lands, by who threw it (sim.ts shell: private, watched here only).
      const sh = b as unknown as { shell: (by: LandUnit, ...r: unknown[]) => unknown };
      const real = sh.shell.bind(b);
      sh.shell = (by, ...r) => { if (by.team === 'mg') shellsFromMg++; return real(by, ...r); };
      end(b, (x) => {
        for (const u of x.units) {
          if (!ids.has(u.id) || u.engaged === undefined) continue;
          const t = x.unit(u.engaged)!;
          const gap = x.gap(u, t);
          expect(gap).toBeLessThanOrEqual(reachOf(u) + 1e-6);
          const key = `${g}:${u.id}`;
          struck.set(key, (struck.get(key) ?? 0) + 1);
          farthest.set(key, Math.max(farthest.get(key) ?? 0, gap));
        }
      });
    }
    expect(struck.size).toBe(12);
    // From the back: most teams struck from further off than any foot can reach.
    expect([...farthest.values()].filter((d) => d > LAND.reach.inf).length).toBeGreaterThanOrEqual(10);
    expect(shellsFromMg).toBe(0);
  });

  it('the harm they do is shared out of their own power: a team that strikes is owed for it', () => {
    const b = new LandBattle(ticket('plains', 1.1, 31));
    end(b);
    const ids = teamsOf(b).map((u) => u.id);
    const owed = b.units.filter((u) => u.owed).some((u) => ids.some((i) => (u.owed![i] ?? 0) !== 0));
    expect(owed).toBe(true);
  });
});

describe('the setup step: place your guns and teams before the clock runs (release 2)', () => {
  const open = (seed = 11): { b: LandBattle; guns: LandUnit[] } => {
    const b = new LandBattle(withTeams(practiceTicket(PRACTICE_GROUNDS[0], { seed, humanAtt: true })));
    expect(b.setupOn('att')).toBe(true);
    return { b, guns: b.units.filter((u) => u.side === 'att' && u.kind === 'art') };
  };

  it('is off unless asked, and while it is open the battle does not run', () => {
    const plain = new LandBattle(practiceTicket(PRACTICE_GROUNDS[0], { seed: 11, humanAtt: true }));
    expect(plain.setup).toBeNull();
    expect(plain.view().setup).toBeUndefined();
    const { b } = open();
    expect(b.view().setup).toBe('att');
    for (let k = 0; k < 50; k++) b.step();
    expect(b.t).toBe(0);
    expect(b.command({ do: 'land:begin' }, 'def')).toBe(false);
    expect(b.command({ do: 'land:begin' }, 'att')).toBe(true);
    expect(b.setup).toBeNull();
    b.step();
    expect(b.t).toBeGreaterThan(0);
    expect(b.setupOn('att')).toBe(false);
  });

  it('places a gun or team in his own ground, on dry ground clear of every block, and nowhere else', () => {
    const { b, guns } = open();
    const z = b.setupRoom()!;
    const mg = guns.find((u) => u.team === 'mg')!, foot = b.units.find((u) => u.side === 'att' && u.kind === 'inf')!;
    const enemy = b.units.find((u) => u.side === 'def')!;
    // Outside his ground, onto another block, a block that is no gun, or the enemy's: refused, and it stays.
    const was = [mg.x, mg.y];
    expect(b.placeWhy(mg, (z[0] + z[2]) / 2, z[3] + 200)).toMatch(/Outside/);
    expect(b.placeWhy(mg, foot.x, foot.y)).toMatch(/Another block/);
    expect(b.placeWhy(foot, (z[0] + z[2]) / 2, z[1] + 60)).toMatch(/Only guns/);
    expect(b.command({ do: 'land:place', units: [enemy.id], x: enemy.x, y: enemy.y - 50 }, 'att')).toBe(false);
    expect(b.command({ do: 'land:place', units: [mg.id], x: foot.x, y: foot.y }, 'att')).toBe(false);
    expect([mg.x, mg.y]).toEqual(was);
    // A clear spot near the front of his ground: placed there, drawn up facing the enemy, home there.
    let spot: [number, number] | null = null;
    for (let x = z[0] + 60; x < z[2] - 60 && !spot; x += 25) if (b.placeWhy(mg, x, z[3] - 30) === null) spot = [x, z[3] - 30];
    expect(spot).not.toBeNull();
    expect(b.command({ do: 'land:place', units: [mg.id], x: spot![0], y: spot![1] }, 'att')).toBe(true);
    expect([mg.x, mg.y]).toEqual(spot);
    expect(mg.home).toEqual(spot);
    expect(mg.face).toBe(0);
    expect(mg.order).toBeNull();
    // Once the battle has begun nothing more is placed.
    b.command({ do: 'land:begin' }, 'att');
    expect(b.command({ do: 'land:place', units: [mg.id], x: spot![0] + 30, y: spot![1] }, 'att')).toBe(false);
  });

  it('his teams fire from where he put them, and stand there with no order', () => {
    const { b, guns } = open(23);
    const z = b.setupRoom()!;
    for (const u of guns) {
      for (let x = z[0] + 60; x < z[2] - 60; x += 25) if (b.command({ do: 'land:place', units: [u.id], x, y: z[3] - 30 }, 'att')) break;
    }
    const placed = guns.map((u) => [u.x, u.y]);
    b.command({ do: 'land:begin' }, 'att');
    let fired = 0;
    for (let k = 0; k < 2500 && !b.over; k++) { b.step(); if (guns.some((u) => u.engaged !== undefined)) fired++; }
    expect(guns.map((u) => [u.x, u.y])).toEqual(placed);
    expect(fired).toBeGreaterThan(0);
  });

  it('the same placings replay the same battle; the men are drawn where the block was put, never left behind', () => {
    const play = (): { s: string; jump: number } => {
      const { b, guns } = open(29);
      const d = b.drillOn();
      const z = b.setupRoom()!;
      const mor = guns.find((u) => u.team === 'mor')!;
      for (let x = z[2] - 80; x > z[0] + 80; x -= 25) if (b.command({ do: 'land:place', units: [mor.id], x, y: z[1] + 60 }, 'att')) break;
      const men = d.view().find((m) => m.id === mor.id)!;
      const far = Math.max(...men.men.map((m) => Math.hypot(m.x - mor.x, m.y - mor.y)));
      b.command({ do: 'land:begin' }, 'att');
      end(b);
      return { s: sig(b), jump: far };
    };
    const one = play(), two = play();
    expect(two.s).toBe(one.s);
    expect(one.jump).toBeLessThan(80);
  });

  it('the computer side places nothing: its guns and teams stand where they were drawn up', () => {
    const { b } = open(37);
    const theirs = b.units.filter((u) => u.side === 'def' && u.kind === 'art').map((u) => [u.id, u.x, u.y]);
    expect(b.command({ do: 'land:place', units: [theirs[0][0] as number], x: (theirs[0][1] as number) + 40, y: theirs[0][2] as number }, 'def')).toBe(false);
    expect(b.units.filter((u) => u.side === 'def' && u.kind === 'art').map((u) => [u.id, u.x, u.y])).toEqual(theirs);
  });
});

describe('the looks: tripod machine guns and mortar tubes, each with its crew (release 2)', () => {
  it('a team block is three weapons with their crews, carried by hand (no tractor); a battery keeps its guns and tractor', () => {
    const b = new LandBattle(ticket('plains', 1.3, 5)), d = b.drillOn();
    for (const u of teamsOf(b)) {
      const sq = d.squad(u.id)!;
      expect(sq.men.map((m) => m.role)).toEqual(Array(u.figures0).fill(u.team === 'mg' ? 'hmg' : 'mortar'));
      expect(sq.veh).toEqual([]);
    }
    const guns = b.units.filter((x) => x.kind === 'art' && !x.team);
    expect(guns.length).toBeGreaterThan(0);
    for (const u of guns) { expect(d.squad(u.id)!.men.every((m) => m.role === 'gun')).toBe(true); expect(d.squad(u.id)!.veh.map((v) => v.role)).toEqual(['lorry']); }
  });

  it('a battle with teams ends exactly the same with the men drawn as without them (ruling 217)', () => {
    for (const [g, seed] of [['plains', 3], ['hills', 8], ['forest', 21]] as [Ground, number][]) {
      const a = new LandBattle(ticket(g, 1.25, seed)), b = new LandBattle(ticket(g, 1.25, seed));
      b.drillOn();
      end(a); end(b);
      expect(sig(b)).toBe(sig(a));
    }
  });

  it('the machine guns fire long bursts, the mortars bombs over the crest with a spotter up when far; never towed, never a lorry', () => {
    const by = { mg: 0, mor: 0, other: 0, mortarKills: 0, mgKills: 0 }, poses = new Set<string>();
    let spotted = 0;
    for (const [g, seed] of [['plains', 3], ['hills', 8], ['forest', 21]] as [Ground, number][]) {
      const b = new LandBattle(ticket(g, 1.25, seed)), d = b.drillOn();
      const team = new Map(teamsOf(b).map((u) => [u.id, u.team!]));
      let last = 0;
      for (let k = 0; !b.over && k < 30000; k++) {
        b.step();
        for (const r of d.rounds()) {
          if (r.n <= last) continue;
          last = r.n;
          const t = r.by !== undefined ? team.get(r.by) : undefined;
          if (!t) continue;
          if (t === 'mg' && r.w === 'mg') { by.mg++; if (r.r === 'kill') by.mgKills++; } else if (t === 'mor' && r.w === 'mortar') { by.mor++; if (r.r === 'kill') by.mortarKills++; } else by.other++;
        }
        if (k % 10 === 0) for (const blk of d.view()) if (team.has(blk.id)) { for (const m of blk.men) poses.add(m.p); if (blk.fo) spotted++; }
      }
    }
    expect(by.mg).toBeGreaterThan(50);
    expect(by.mor).toBeGreaterThan(10);
    expect(by.other).toBe(0);
    expect(by.mgKills + by.mortarKills).toBeGreaterThan(0);
    expect(spotted).toBeGreaterThan(0);
    expect(poses.has('towed')).toBe(false);
    expect(poses.has('fire')).toBe(true);
    expect([...poses].every((p) => ['deployed', 'fire', 'walk', 'run', 'wait', 'flee', 'cower', 'down'].includes(p))).toBe(true);
  });
});
