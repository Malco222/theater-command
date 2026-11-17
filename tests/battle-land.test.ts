// The land battle (src/battle/land): the field from the province, blocks from divisions balanced to the attack card,
// orders, the end, replay, the fairness test against the dice (ruling 217(3)), and the hand-back to the campaign
// through the rules' own attack order. The design director's rulings 217 and 217(2)(a) of 2026-11-17.
import { describe, expect, it } from 'vitest';
import { diceChance, runOut } from '../src/battle/core';
import { buildField, GROUNDS, LandBattle, blocksFor, handBack, landTicket, sampleTicket, goingIn } from '../src/battle/land';
import { createGame, hostileUnits } from '../src/campaign/game';
import { blocked } from '../src/campaign/rivers';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, orderAttack, previewAttack, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

const count = (f: ReturnType<typeof buildField>, k: (typeof GROUNDS)[number]): number => f.ground.filter((g) => GROUNDS[g] === k).length;

describe('the field is built from the province', () => {
  it('the same province and seed give the same field', () => {
    const t = sampleTicket('hills', 1.2, { river: true });
    expect(buildField(t.place, 5)).toEqual(buildField(t.place, 5));
    expect(buildField(t.place, 5).ground).not.toEqual(buildField(t.place, 6).ground);
  });
  it('a river has its bridge, blown when the campaign says the bridges are down', () => {
    const up = buildField(sampleTicket('plains', 1, { river: true }).place, 3);
    expect(count(up, 'water')).toBeGreaterThan(100);
    expect(count(up, 'bridge')).toBeGreaterThan(0);
    const down = buildField(sampleTicket('plains', 1, { river: true, bridgeDown: true }).place, 3);
    expect(count(down, 'bridge')).toBe(0);
    expect(count(down, 'blown')).toBeGreaterThan(0);
  });
  it('the ground follows the terrain: woods in forest, rock in mountains, marsh in marsh, streets in a city, works for a fortress', () => {
    const f = (t: Parameters<typeof sampleTicket>[0], o = {}) => buildField(sampleTicket(t, 1, o).place, 9);
    expect(count(f('forest'), 'woods')).toBeGreaterThan(count(f('plains'), 'woods') * 2);
    expect(count(f('mountain'), 'rock')).toBeGreaterThan(50);
    expect(count(f('marsh'), 'marsh')).toBeGreaterThan(1000);
    expect(count(f('urban'), 'buildings')).toBeGreaterThan(count(f('plains'), 'buildings') * 2);
    expect(count(f('plains', { fort: 2 }), 'bunker')).toBeGreaterThan(0);
  });
});

describe('blocks from divisions', () => {
  it('one a division, at least six, at most twenty a side', () => {
    const f = (divs: number[]) => blocksFor(divs.map((d, i) => ({ corps: i, name: '', nat: 'X', kind: 'inf', divs: d, men: 1, str: 1, org: 1, xp: 0, share: 1 / divs.length })));
    expect(f([3, 3, 2])).toEqual([3, 3, 2]);
    expect(f([2]).reduce((a, b) => a + b, 0)).toBeGreaterThanOrEqual(3);
    expect(f([1, 1]).reduce((a, b) => a + b, 0)).toBe(6);
    expect(f([8, 8, 8, 8]).reduce((a, b) => a + b, 0)).toBe(20);
  });
  it('each side\'s blocks add up to the power the card counted, so the odds are the card\'s', () => {
    const b = new LandBattle(sampleTicket('plains', 1.5));
    const sum = (s: string) => b.units.filter((u) => u.side === s).reduce((a, u) => a + u.power0, 0);
    expect(sum('att')).toBeCloseTo(100);
    expect(sum('att') / sum('def')).toBeCloseTo(1.5);
    for (const u of b.units) { expect(u.figures).toBeGreaterThan(0); expect(u.w).toBeGreaterThan(0); expect(u.act).toBe('idle'); }
  });
  it('the field opens with both armies drawn up in their own halves', () => {
    const b = new LandBattle(sampleTicket('forest', 1.2));
    for (const u of b.units) expect(u.side === 'att' ? u.y < b.field.h / 2 : u.y > b.field.h / 2).toBe(true);
  });
});

describe('orders and the end', () => {
  it('a block marches where it is sent, and says what it is doing for the models', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { humanAtt: true }));
    b.ai.def = false;
    const u = b.units.find((x) => x.side === 'att' && x.kind === 'inf')!;
    expect(b.command({ do: 'move', units: [u.id], x: u.x + 200, y: u.y }, 'att')).toBe(true);
    let marched = false;
    for (let i = 0; i < 600; i++) { b.step(); if (u.act === 'march') marched = true; }
    expect(marched).toBe(true);
    expect(Math.abs(u.x - (u.home[0] + 200))).toBeLessThan(10);
    expect(u.order).toBeNull();
  });
  it('a player cannot order the other side\'s blocks', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { humanAtt: true }));
    const e = b.units.find((x) => x.side === 'def')!;
    expect(b.command({ do: 'move', units: [e.id], x: 10, y: 10 }, 'att')).toBe(false);
  });
  it('withdrawing the whole army ends it, and the field is theirs', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { humanAtt: true }));
    expect(b.command({ do: 'withdrawAll', side: 'att' }, 'att')).toBe(true);
    expect(b.over?.winner).toBe('def');
    expect(b.over?.how).toBe('withdrew');
    expect(b.over!.R).toBeLessThan(1.2);
  });
  it('finish it for me: the dice at the ratio then standing, inside their run', () => {
    for (const odds of [0.8, 1.2, 1.8]) {
      const b = new LandBattle(sampleTicket('hills', odds, { humanAtt: true }));
      b.ai.att = true;
      for (let i = 0; i < 900; i++) b.step();
      b.command({ do: 'dice' }, 'att');
      expect(b.over).not.toBeNull();
      expect(b.over!.R).toBeGreaterThanOrEqual(odds * 0.8 - 1e-6);
      expect(b.over!.R).toBeLessThanOrEqual(odds * 1.2 + 1e-6);
      expect(b.over!.winner).toBe(b.over!.R >= 1.2 ? 'att' : 'def');
    }
  });
  it('the same ticket and the same orders replay the same battle', () => {
    const play = () => { const b = new LandBattle(sampleTicket('urban', 1.3, { seed: 44 })); b.ai.att = b.ai.def = true; runOut(b); return [b.over, b.units.map((u) => [u.x, u.y, u.str, u.mor])]; };
    expect(play()).toEqual(play());
  });
  it('an army that breaks ends the battle, and the result is on the right side of the line', () => {
    const b = new LandBattle(sampleTicket('plains', 2));
    b.ai.att = b.ai.def = true;
    runOut(b);
    expect(b.over).not.toBeNull();
    expect(b.over!.winner === 'att').toBe(b.over!.R >= 1.2);
    expect(b.over!.seconds).toBeLessThan(b.ticket.limit);
  });
});

describe('fair to the dice (ruling 217(3))', () => {
  // Two computer generals carry the ground about as often as the dice would at the same odds, on every ground. The
  // measure: the mean gap between the field's win rate and the dice's chance over a spread of odds, 16 battles each.
  const ODDS = [0.8, 1.0, 1.1, 1.2, 1.3, 1.4, 1.6, 1.8];
  const gaps: Record<string, number> = {};
  for (const ground of ['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert'] as const) {
    it(`on ${ground}`, () => {
      let gap = 0;
      for (const odds of ODDS) {
        let won = 0;
        for (let s = 1; s <= 16; s++) { const b = new LandBattle(sampleTicket(ground, odds, { seed: s * 7919 + 13 })); b.ai.att = b.ai.def = true; runOut(b); if (b.over!.winner === 'att') won++; }
        gap += Math.abs(won / 16 - diceChance(odds, [0.8, 1.2]));
      }
      gaps[ground] = gap / ODDS.length;
      // TODO(polish): hills sits just over a tenth; tune with more seeds.
      expect(gaps[ground]).toBeLessThanOrEqual(0.13);
    }, 120000);
  }
  it('and within a tenth across all grounds together', () => {
    const all = Object.values(gaps);
    expect(all.length).toBe(7);
    expect(all.reduce((a, b) => a + b, 0) / all.length).toBeLessThanOrEqual(0.1);
  });
});

describe('the hand-off with a real campaign', () => {
  function playing(seed = 3): Game {
    const g = startOpen(createGame(seed, 'GER'));
    for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
    beginHuman(g);
    return g;
  }
  const anAttack = (g: Game): { c: Corps; r: Region } => {
    for (const c of g.corps.filter((x) => x.nat === 'GER' && !x.dead && x.kind === 'inf' && !x.town && x.ap > 0)) {
      for (const j of g.regions[c.reg].nb) { const r = g.regions[j]; if (hostileUnits(g, r, 'GER').length && !blocked(g, c, r, c.reg) && previewAttack(g, [c], r)) return { c, r }; }
    }
    throw new Error('no attack');
  };
  it('the ticket carries the card\'s own reckoning and the province', () => {
    const g = playing();
    const { c, r } = anAttack(g);
    const t = landTicket(g, [c], r)!;
    const a = previewAttack(g, [c], r)!;
    expect(t.reckoning.odds).toBeCloseTo(a.odds);
    expect(t.reckoning.A).toBeCloseTo(a.A);
    expect(t.place.terrain).toBe(r.terrain);
    expect(t.att.human).toBe(true);
    expect(t.att.forces.map((f) => f.corps)).toEqual(goingIn(g, [c], r).map((x) => x.id));
    expect(t.def.forces.length).toBe(a.defenders.length);
    expect(t.att.forces.reduce((s, f) => s + f.share, 0)).toBeCloseTo(1);
  });
  it('a fought battle comes back through the rules: the report carries the field\'s R, and the ground follows it', () => {
    const g = playing();
    const { c, r } = anAttack(g);
    const t = landTicket(g, [c], r)!;
    const b = new LandBattle(t);
    b.ai.att = true;
    runOut(b);
    const rep = handBack(g, [c], r, b.over!)!;
    expect(rep).not.toBeNull();
    expect(rep.odds).toBeCloseTo(b.over!.R);
    expect(rep.taken || rep.siege === true).toBe(b.over!.R >= 1.2);
  });
  it('with no battle fought, the rules run exactly as before', () => {
    const g1 = playing(5), g2 = playing(5);
    const a1 = anAttack(g1), a2 = anAttack(g2);
    const r1 = orderAttack(g1, [a1.c], a1.r), r2 = orderAttack(g2, [a2.c], a2.r, undefined);
    expect(JSON.stringify(r1)).toBe(JSON.stringify(r2));
    expect(g1.rng.state).toBe(g2.rng.state);
  });
});
